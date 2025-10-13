#pragma once
#include <format>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <vector>
#include "../config/Config.h"
#include "../config/Field.h"
#include "../util.hpp"

#include <esp_gatt_common_api.h>
#include <esp_gattc_api.h>
#include <rwlock.hpp>
#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_log.h"

class BLEBase {
public:
    static auto initialize() -> void {
        adv_data.include_name = true;
        adv_data.include_txpower = true;
        adv_data.appearance = ESP_BLE_APPEARANCE_GENERIC_HID;
        adv_data.flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
        adv_params.adv_type = ADV_TYPE_IND;
        adv_params.channel_map = ADV_CHNL_ALL;
        adv_params.own_addr_type = BLE_ADDR_TYPE_RANDOM;
        adv_params.adv_int_max = 0x40;
        adv_params.adv_int_min = 0x20;
        adv_params.peer_addr_type = BLE_ADDR_TYPE_PUBLIC;
        adv_params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;

        init_ble();
        init_ble_security();
    }

    virtual std::string_view get_name() {
        return "FeatureBase";
    }

    static std::string get_address() {
        return std::format("{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    }

    BLEBase(const BLEBase&) = delete;
    auto operator=(const BLEBase&) -> BLEBase& = delete;
    BLEBase(BLEBase&&) = delete;
    auto operator=(BLEBase&&) -> BLEBase& = delete;

protected:
    ~BLEBase() = default;
    BLEBase() = default;

    struct DESCR_Profile {
        uint16_t descr_handle;
        esp_bt_uuid_t descr_uuid;
        esp_gatt_perm_t perm;
        esp_attr_value_t attr_value;
        RWLock lock;
        std::function<esp_gatt_status_t(esp_gatts_cb_event_t)> rw_cb;
    };

    struct CHAR_Profile {
        uint16_t char_handle;
        esp_bt_uuid_t char_uuid;
        esp_gatt_perm_t perm;
        esp_gatt_char_prop_t property;
        esp_attr_value_t attr_value;
        RWLock lock;

        std::vector<std::shared_ptr<DESCR_Profile>> descrs;
        std::function<esp_gatt_status_t(esp_gatts_cb_event_t)> rw_cb;
    };

    struct GATTS_Profile {
        std::string name;
        esp_gatt_if_t gatts_if;
        uint16_t app_id;
        uint16_t conn_id;
        uint16_t service_handle;
        esp_gatt_srvc_id_t service_id;
        uint16_t num_handle;

        std::shared_ptr<BLEBase> feature;
        std::vector<std::shared_ptr<CHAR_Profile>> chars;
    };

    static auto add_feature(const std::string& _name, const std::shared_ptr<BLEBase>& _that, uint16_t uuid) -> std::shared_ptr<GATTS_Profile> {
        auto app = std::make_shared<GATTS_Profile>();
        auto hash = std::hash<std::string>();
        apps.push_back(app);

        app->feature = _that;
        app->name = _name;
        app->app_id = next_app_id++;
        app->num_handle = 1;
        app->service_id.is_primary = true;
        app->service_id.id.inst_id = 0x00;
        app->service_id.id.uuid.len = ESP_UUID_LEN_16;
        app->service_id.id.uuid.uuid.uuid16 = uuid ? uuid : (uint16_t)hash(_name + "+_Service");

        ESP_LOGI("HASH", "%s APP ID is %d; Service UUID is %d", _name.data(), app->app_id, app->service_id.id.uuid.uuid.uuid16);
        return app;
    }

    template<typename T>
    static std::shared_ptr<CHAR_Profile> register_char(std::shared_ptr<GATTS_Profile> _profile,
                                                       T& buffer,
                                                       const std::function<esp_gatt_status_t(esp_gatts_cb_event_t)> _handler = nullptr,
                                                       uint16_t uuid_char = 0,
                                                       esp_gatt_perm_t perm = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                       esp_gatt_char_prop_t property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE) {
        auto char_ = std::make_shared<CHAR_Profile>();
        _profile->chars.push_back(char_);
        _profile->num_handle += 2;

        auto hash = std::hash<std::string>();

        char_->perm = perm;
        char_->property = property;
        char_->rw_cb = std::move(_handler);
        char_->attr_value.attr_max_len = sizeof(T);
        char_->attr_value.attr_len = sizeof(T);
        char_->attr_value.attr_value = (uint8_t*)&buffer;
        char_->char_uuid.len = ESP_UUID_LEN_16;
        char_->char_uuid.uuid.uuid16 = uuid_char ? uuid_char : hash(_profile->name + std::string(typeid(T).name()) + std::to_string(time(0)));
        return char_;
    }

    template<typename T>
    static std::shared_ptr<DESCR_Profile> register_descr(std::shared_ptr<GATTS_Profile> gatts_profile,
                                                         std::shared_ptr<CHAR_Profile> _profile,
                                                         T& buffer,
                                                         const std::function<esp_gatt_status_t(esp_gatts_cb_event_t)> _handler = nullptr,
                                                         uint16_t uuid_char = 0,
                                                         esp_gatt_perm_t perm = ESP_GATT_PERM_READ) {
        auto descr = std::make_shared<DESCR_Profile>();
        _profile->descrs.push_back(descr);
        gatts_profile->num_handle += 1;

        auto hash = std::hash<time_t>();
        descr->perm = perm;
        descr->rw_cb = std::move(_handler);
        descr->attr_value.attr_max_len = sizeof(T);
        descr->attr_value.attr_len = sizeof(T);
        descr->attr_value.attr_value = (uint8_t*)&buffer;
        descr->descr_uuid.len = ESP_UUID_LEN_16;
        descr->descr_uuid.uuid.uuid16 = uuid_char ? uuid_char : hash(rand() % 0x7FFFFFFF);
        srand(rand() % 0x7FFFFFFF);
        return descr;
    }

    bool send(std::shared_ptr<GATTS_Profile> gatts_profile, std::shared_ptr<CHAR_Profile> char_, bool need_confirm = false) {
        esp_err_t err = esp_ble_gatts_send_indicate(gatts_profile->gatts_if, gatts_profile->conn_id, char_->char_handle, char_->attr_value.attr_len, char_->attr_value.attr_value, need_confirm);
        return err == ESP_OK;
    }

    virtual bool gatts_event_callback(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t* param) {
        return false;
    }

    inline static esp_bd_addr_t addr;

protected:
    inline static std::vector<std::shared_ptr<GATTS_Profile>> apps;

private:
    inline static util::Map<esp_gatts_cb_event_t, std::function<void(esp_gatts_cb_event_t, esp_gatt_if_t, esp_ble_gatts_cb_param_t*)>> gatts_event;
    inline static util::Map<esp_gap_ble_cb_event_t, std::function<void(esp_gap_ble_cb_event_t, esp_ble_gap_cb_param_t*)>> gap_event;
    inline static util::Map<uint16_t, std::shared_ptr<GATTS_Profile>> gatts_map;
    inline static util::Map<uint16_t, std::shared_ptr<CHAR_Profile>> chars_map;
    inline static util::Map<uint16_t, std::shared_ptr<DESCR_Profile>> descr_map;

    inline static esp_bt_controller_config_t adv_config = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    inline static esp_ble_adv_data_t adv_data;
    inline static esp_ble_adv_params_t adv_params;
    inline static esp_bt_mode_t bt_mode = ESP_BT_MODE_BLE;
    inline static std::string_view ble_name{"ESP32-S3"};
    inline static uint16_t connect_id;
    inline static uint16_t current_mtu = 23;
    inline static uint16_t next_app_id = 0;

    static void gatts_callback(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t* param) {
        // ESP_LOGI("BLE GATTS", "EVENT: %s; ID: %d;", magic_enum::enum_name<esp_gatts_cb_event_t>(event).data(), param->connect.conn_id);

        if (!gatts_event.empty() || gatts_event.contains(event)) {
            return gatts_event[event](event, gatts_if, param);
        }

        auto it = std::ranges::find_if(apps, [&param, &gatts_if](std::shared_ptr<GATTS_Profile>& app) -> bool { return app->gatts_if == gatts_if; });
        if (it != apps.end()) {
            if ((*it)->feature->gatts_event_callback(event, gatts_if, param)) {
                return;
            }
        }

        switch (event) {
            case ESP_GATTS_DISCONNECT_EVT: {
                esp_err_t err = esp_ble_gap_stop_advertising();
                ESP_ERROR_CHECK(err);
                err = esp_ble_gap_start_advertising(&adv_params);
                ESP_ERROR_CHECK(err);
                connect_id = 0;
                for (auto& app : apps) {
                    app->conn_id = connect_id;
                }
            } break;
            case ESP_GATTS_CONNECT_EVT: {
                ESP_LOGI("ESP_GATTS_CONNECT_EVT",
                         "ID: %d; Remote Address: %02X:%02X:%02X:%02X:%02X:%02X;",
                         param->connect.conn_id,
                         param->connect.remote_bda[5],
                         param->connect.remote_bda[4],
                         param->connect.remote_bda[3],
                         param->connect.remote_bda[2],
                         param->connect.remote_bda[1],
                         param->connect.remote_bda[0]);
                esp_err_t err = esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT);
                ESP_ERROR_CHECK(err);
                err = esp_ble_gap_stop_advertising();
                ESP_ERROR_CHECK(err);
                connect_id = param->connect.conn_id;
                for (auto& app : apps) {
                    app->conn_id = connect_id;
                }
                esp_ble_gattc_send_mtu_req(gatts_if, connect_id);
            } break;
            case ESP_GATTS_MTU_EVT: {
                ESP_LOGI("ESP_GATTS_MTU_EVT", "MTU %d;", param->mtu.mtu);
                current_mtu = param->mtu.mtu;
            } break;
            case ESP_GATTS_REG_EVT: {
                auto it = std::ranges::find_if(apps, [&param](std::shared_ptr<GATTS_Profile>& app) -> bool { return app->app_id == param->reg.app_id; });
                if (it != apps.end()) {
                    (*it)->gatts_if = gatts_if;
                    esp_ble_gatts_create_service(gatts_if, &(*it)->service_id, (*it)->num_handle);
                    ESP_LOGI("ESP_GATTS_REG_EVT", "APP ID: %d; GATTS: %d; SERVICE ID: %d;", param->reg.app_id, gatts_if, (*it)->service_id.id);
                }
            } break;
            case ESP_GATTS_CREATE_EVT: {
                if (it != apps.end()) {
                    esp_err_t err;
                    auto service_handle = (*it)->service_handle = param->create.service_handle;
                    for (auto& char_ : (*it)->chars) {
                        err = esp_ble_gatts_add_char(service_handle, &char_->char_uuid, char_->perm, char_->property, &char_->attr_value, NULL);
                        ESP_ERROR_CHECK(err);
                        for (auto& d : char_->descrs) {
                            esp_err_t err = esp_ble_gatts_add_char_descr(service_handle, &d->descr_uuid, d->perm, &d->attr_value, nullptr);
                            ESP_ERROR_CHECK(err);
                        }
                    }
                    gatts_map[service_handle] = (*it);
                    err = esp_ble_gatts_start_service(service_handle);
                    ESP_ERROR_CHECK(err);
                    ESP_LOGI("ESP_GATTS_CREATE_EVT", "服务ID: %d; 服务启动!", (*it)->service_id.id);
                }
            } break;
            case ESP_GATTS_ADD_CHAR_EVT: {
                auto char_it =
                        std::ranges::find_if((*it)->chars, [&param](std::shared_ptr<CHAR_Profile>& char_) -> bool { return char_->char_uuid.uuid.uuid16 == param->add_char.char_uuid.uuid.uuid16; });
                if (char_it == (*it)->chars.end()) {
                    ESP_LOGW("ESP_GATTS_ADD_CHAR_EVT", "未知特征");
                    break;
                }

                (*char_it)->char_handle = param->add_char.attr_handle;
                chars_map[(*char_it)->char_handle] = (*char_it);
            } break;
            case ESP_GATTS_ADD_CHAR_DESCR_EVT: {
                for (auto char_it : (*it)->chars) {
                    auto descr_it = std::ranges::find_if(
                            char_it->descrs, [&param](std::shared_ptr<DESCR_Profile>& descr_) -> bool { return descr_->descr_uuid.uuid.uuid16 == param->add_char_descr.descr_uuid.uuid.uuid16; });
                    if (descr_it != char_it->descrs.end()) {
                        (*descr_it)->descr_handle = param->add_char_descr.attr_handle;
                        descr_map[(*descr_it)->descr_handle] = (*descr_it);
                        ESP_LOGI("ESP_GATTS_ADD_CHAR_DESCR_EVT", "已找到特征描述符");
                        break;
                    }
                }
            } break;
            case ESP_GATTS_READ_EVT: {
                std::shared_ptr<CHAR_Profile> char_ptr;
                std::shared_ptr<DESCR_Profile> descr_ptr;
                if (chars_map.contains(param->read.handle)) {
                    char_ptr = chars_map[param->read.handle];
                } else if (descr_map.contains(param->read.handle)) {
                    descr_ptr = descr_map[param->read.handle];
                }

                if (!char_ptr && !descr_ptr) {
                    ESP_LOGW("ESP_GATTS_READ_EVT", "未知特征 句柄: %d;", param->read.handle);
                    break;
                }

                RWLock::ReadLock rlk(char_ptr ? char_ptr->lock : descr_ptr->lock);

                const auto& attr = char_ptr ? char_ptr->attr_value : descr_ptr->attr_value;
                uint16_t offset = param->read.offset;
                uint16_t left = attr.attr_len - offset;
                uint16_t pkt = std::min(left, uint16_t(current_mtu - 1));

                esp_gatt_rsp_t rsp{};
                rsp.attr_value.handle = param->read.handle;
                rsp.attr_value.len = pkt;
                rsp.attr_value.offset = offset;
                std::memcpy(rsp.attr_value.value, attr.attr_value + offset, pkt);

                // ESP_LOGI("ESP_GATTS_READ_EVT",
                //          "连接ID: %d; ID: %d; 句柄: %d; 长度: %d; 指针: %p; 分段读取 偏移=%d 长度=%d 总=%d;",
                //          param->read.conn_id,
                //          param->read.trans_id,
                //          param->read.handle,
                //          rsp.attr_value.len,
                //          char_ptr ? char_ptr->attr_value.attr_value : descr_ptr->attr_value.attr_value,
                //          offset,
                //          pkt,
                //          attr.attr_len);

                if (param->read.need_rsp) {
                    esp_err_t err = esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
                    ESP_ERROR_CHECK(err);
                    ESP_LOGI("ESP_GATTS_READ_EVT", "发送成功!");
                }

                if (offset + pkt >= attr.attr_len) {
                    if (char_ptr ? char_ptr->rw_cb : descr_ptr->rw_cb) {
                        char_ptr ? char_ptr->rw_cb(ESP_GATTS_READ_EVT) : descr_ptr->rw_cb(ESP_GATTS_READ_EVT);
                    }
                }
            } break;
            case ESP_GATTS_WRITE_EVT: {
                std::shared_ptr<CHAR_Profile> char_ptr;
                std::shared_ptr<DESCR_Profile> descr_ptr;
                if (chars_map.contains(param->write.handle)) {
                    char_ptr = chars_map[param->write.handle];
                } else if (descr_map.contains(param->write.handle)) {
                    descr_ptr = descr_map[param->write.handle];
                }

                if (!char_ptr && !descr_ptr) {
                    ESP_LOGW("ESP_GATTS_WRITE_EVT", "未知特征 句柄: %d;", param->write.handle);
                    break;
                }

                RWLock::WriteLock wlk(char_ptr ? char_ptr->lock : descr_ptr->lock);

                auto& attr = char_ptr ? char_ptr->attr_value : descr_ptr->attr_value;
                uint16_t offset = param->write.offset;
                uint16_t len = param->write.len;

                if (offset + len > attr.attr_max_len) {
                    ESP_LOGE("ESP_GATTS_WRITE_EVT", "超长 %d+%d > %d", offset, len, attr.attr_max_len);
                    if (param->write.need_rsp) {
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_INVALID_ATTR_LEN, nullptr);
                    }
                    break;
                }

                std::memcpy(attr.attr_value + offset, param->write.value, len);
                attr.attr_len = std::max(attr.attr_len, uint16_t(offset + len));

                // ESP_LOGI("ESP_GATTS_WRITE_EVT",
                //          "连接ID: %d; ID: %d; 句柄:%d 偏移:%d 长度:%d 累计:%d %s",
                //          param->write.conn_id,
                //          param->write.trans_id,
                //          param->write.handle,
                //          offset,
                //          len,
                //          attr.attr_len,
                //          param->write.is_prep ? "(prep)" : "");

                if (param->write.is_prep) {
                    if (param->write.need_rsp) {
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, nullptr);
                    }
                } else {
                    if (param->write.need_rsp) {
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, nullptr);
                    }
                }

                if (char_ptr ? char_ptr->rw_cb : descr_ptr->rw_cb) {
                    char_ptr ? char_ptr->rw_cb(ESP_GATTS_WRITE_EVT) : descr_ptr->rw_cb(ESP_GATTS_WRITE_EVT);
                }
            } break;
            case ESP_GATTS_EXEC_WRITE_EVT: {
                ESP_LOGI("ESP_GATTS_EXEC_WRITE_EVT", "连接ID: %d; ID: %d; 长写提交 执行完毕", param->exec_write.conn_id, param->exec_write.trans_id);
                esp_ble_gatts_send_response(gatts_if, param->exec_write.conn_id, param->exec_write.trans_id, ESP_GATT_OK, nullptr);
            } break;
            case ESP_GATTS_CONF_EVT: {
            } break;
            default:
                break;
        }
    }

    static void gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
        // ESP_LOGI("BLE GAP", "EVENT: %s", magic_enum::enum_name<esp_gap_ble_cb_event_t>(event).data());

        if (!gap_event.empty() || gap_event.contains(event)) {
            return gap_event[event](event, param);
        }

        switch (event) {
            case ESP_GAP_BLE_AUTH_CMPL_EVT: {
                if (param->ble_security.auth_cmpl.success) {
                    ESP_LOGI("ESP_GAP_BLE_AUTH_CMPL_EVT", "JustWorks pairing success");
                } else {
                    ESP_LOGE("ESP_GAP_BLE_AUTH_CMPL_EVT", "pairing fail 0x%x", param->ble_security.auth_cmpl.fail_reason);
                }
            } break;
            case ESP_GAP_BLE_SEC_REQ_EVT: {
                ESP_LOGI("ESP_GAP_BLE_SEC_REQ_EVT", "Slave Security Request, accept");
                esp_err_t err = esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
                ESP_ERROR_CHECK(err);
            } break;
            case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT: {
            } break;
            case ESP_GAP_BLE_PASSKEY_REQ_EVT:
            case ESP_GAP_BLE_NC_REQ_EVT:
            case ESP_GAP_BLE_KEY_EVT:
            default:
                break;
        }
    }

    static void init_ble() {
        esp_err_t err;
        load_or_generate_addr();
        err = esp_bt_controller_init(&adv_config);
        ESP_ERROR_CHECK(err);
        err = esp_bt_controller_enable(bt_mode);
        ESP_ERROR_CHECK(err);
        err = esp_bluedroid_init();
        ESP_ERROR_CHECK(err);
        err = esp_bluedroid_enable();
        ESP_ERROR_CHECK(err);
        err = esp_ble_gap_config_local_privacy(false);
        ESP_ERROR_CHECK(err);
        err = esp_ble_gap_set_rand_addr(addr);
        ESP_ERROR_CHECK(err);
        err = esp_ble_gap_set_device_name(ble_name.data());
        ESP_ERROR_CHECK(err);
        err = esp_ble_gap_config_adv_data(&adv_data);
        ESP_ERROR_CHECK(err);
        err = esp_ble_gatts_register_callback(gatts_callback);
        ESP_ERROR_CHECK(err);
        err = esp_ble_gap_register_callback(gap_callback);
        ESP_ERROR_CHECK(err);
        for (auto& app : apps) {
            err = esp_ble_gatts_app_register(app->app_id);
            ESP_ERROR_CHECK(err);
        }
        err = esp_ble_gatt_set_local_mtu(512);
        ESP_ERROR_CHECK(err);
        err = esp_ble_gap_start_advertising(&adv_params);
        ESP_ERROR_CHECK(err);
    }

    static void init_ble_security() {
        esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND;
        esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
        uint8_t key_size = 16;
        uint8_t sc_mode = 0;
        uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
        uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;

        esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
        esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
        esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
        esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
        esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));
        esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH, &sc_mode, 1);
    }

    static void load_or_generate_addr(void) {
        std::ifstream in("/FATFS/bt_addr.bin", std::ios::binary);
        if (in.is_open() && in.read(reinterpret_cast<char*>(addr), 6)) {
            ESP_LOGI("BT_ADDR", "使用已保存地址 %02X:%02X:%02X:%02X:%02X:%02X", addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
            in.close();
            return;
        }

        esp_ble_gap_addr_create_static(addr);
        ESP_LOGI("BT_ADDR", "生成新地址 %02X:%02X:%02X:%02X:%02X:%02X", addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);

        std::ofstream out("/FATFS/bt_addr.bin", std::ios::binary);
        if (out.is_open()) {
            out.write(reinterpret_cast<const char*>(addr), 6);
            out.close();
        } else {
            ESP_LOGE("BT_ADDR", "写地址文件失败");
        }
    }
};

template<typename T>
class BLERegistrar : public BLEBase {
public:
    BLERegistrar(const BLERegistrar&) = delete;
    auto operator=(const BLERegistrar&) -> BLERegistrar& = delete;
    BLERegistrar(BLERegistrar&&) = delete;
    auto operator=(BLERegistrar&&) -> BLERegistrar& = delete;

    auto register_ble() -> void
        requires(std::is_base_of_v<BLEBase, T>)
    {
        auto name = std::string(typeid(T).name()).substr(1);
        this->register_ble_messages(add_feature(name, T::instance(), this->get_uuid()));
        ESP_LOGI("BLE", "已添加BLE功能->%s", name.data());
    }

protected:
    BLERegistrar() {
    }
    ~BLERegistrar() = default;

    virtual void register_ble_messages(std::shared_ptr<GATTS_Profile> _profile) {
    }

    virtual uint16_t get_uuid() {
        return 0;
    }

private:
    friend T;
};

#define BLE_MSG_BEGIN                                                                                                                                                                                  \
    inline static std::shared_ptr<GATTS_Profile> app_;                                                                                                                                                 \
    auto register_ble_messages(std::shared_ptr<GATTS_Profile> _profile) -> void override {                                                                                                             \
        app_ = _profile;
#define BLE_MSG_FUNC(func) esp_gatt_status_t func(esp_gatts_cb_event_t event)
#define BLE_MSG(func) [this](esp_gatts_cb_event_t hid_info_msg) -> esp_gatt_status_t { return this->func(hid_info_msg); }
#define BLE_MSG_END }
