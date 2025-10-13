#pragma once

#include "../BLE.hpp"
#include "../Features.hpp"
#include "esp_log.h"

class Battery : public FeatureRegistrar<Battery>, public BLERegistrar<Battery> {
public:
    Battery() = default;
    ~Battery() = default;

    void notify();
    void set_value(uint8_t v) {
        this->level_ = v;
    }

    BLE_MSG_BEGIN;
    battery_char_ = register_char(_profile, level_, nullptr, ESP_GATT_UUID_BATTERY_LEVEL, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY);
    battery_descr_ = register_descr(_profile, battery_char_, ccc, nullptr, ESP_GATT_UUID_CHAR_CLIENT_CONFIG, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);
    BLE_MSG_END;

    auto registrator() -> void override;
    auto get_uuid() -> uint16_t override {
        return ESP_GATT_UUID_BATTERY_SERVICE_SVC;
    }

private:
    uint8_t level_ = 100;
    struct CCCD {
        uint8_t info[2]{
                0x00,
                0x00,
        };
    };
    CCCD ccc;

    inline static std::shared_ptr<CHAR_Profile> battery_char_;
    inline static std::shared_ptr<DESCR_Profile> battery_descr_;

    void send_notification();
};
