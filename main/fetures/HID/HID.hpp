#pragma once
#include "../BLE.hpp"
#include "../Features.hpp"
#include "config/Config.h"
#include "esp_log.h"

class HID : public FeatureRegistrar<HID>, public BLERegistrar<HID> {
public:
    HID();
    ~HID();

    void click(uint8_t button);
    void move(int8_t x, int8_t y);
    void wheel(int8_t vertical);

    struct Map {
        std::array<uint8_t, 95> report_map = {
                0x05, 0x01, // USAGE_PAGE (Generic Desktop)
                0x09, 0x06, // USAGE (Keyboard)
                0xa1, 0x01, // COLLECTION (Application)
                0x85, 0x01, //   REPORT_ID (1)
                0x05, 0x07, //   USAGE_PAGE (Keyboard)
                0x19, 0xe0, //   USAGE_MINIMUM (Keyboard LeftControl)
                0x29, 0xe7, //   USAGE_MAXIMUM (Keyboard Right GUI)
                0x15, 0x00, //   LOGICAL_MINIMUM (0)
                0x25, 0x01, //   LOGICAL_MAXIMUM (1)
                0x75, 0x01, //   REPORT_SIZE (1)
                0x95, 0x08, //   REPORT_COUNT (8)
                0x81, 0x02, //   INPUT (Data,Var,Abs)
                0x95, 0x06, //   REPORT_COUNT (6)
                0x75, 0x08, //   REPORT_SIZE (8)
                0x15, 0x00, //   LOGICAL_MINIMUM (0)
                0x25, 0x65, //   LOGICAL_MAXIMUM (101)
                0x05, 0x07, //   USAGE_PAGE (Keyboard)
                0x19, 0x00, //   USAGE_MINIMUM (Reserved (no event indicated))
                0x29, 0x65, //   USAGE_MAXIMUM (Keyboard Application)
                0x81, 0x00, //   INPUT (Data,Ary,Abs)
                0xc0, // END_COLLECTION
                0x05, 0x01, // USAGE_PAGE (Generic Desktop)
                0x09, 0x02, // USAGE (Mouse)
                0xa1, 0x01, // COLLECTION (Application)
                0x85, 0x02, //   REPORT_ID (2)
                0x09, 0x01, //   USAGE (Pointer)
                0xa1, 0x00, //   COLLECTION (Physical)
                0x05, 0x09, //     USAGE_PAGE (Button)
                0x19, 0x01, //     USAGE_MINIMUM (Button 1)
                0x29, 0x03, //     USAGE_MAXIMUM (Button 3)
                0x15, 0x00, //     LOGICAL_MINIMUM (0)
                0x25, 0x01, //     LOGICAL_MAXIMUM (1)
                0x95, 0x03, //     REPORT_COUNT (3)
                0x75, 0x01, //     REPORT_SIZE (1)
                0x81, 0x02, //     INPUT (Data,Var,Abs)
                0x95, 0x01, //     REPORT_COUNT (1)
                0x75, 0x05, //     REPORT_SIZE (5)
                0x81, 0x03, //     INPUT (Cnst,Var,Abs)
                0x05, 0x01, //     USAGE_PAGE (Generic Desktop)
                0x09, 0x30, //     USAGE (X)
                0x09, 0x31, //     USAGE (Y)
                0x09, 0x38, //     USAGE (Wheel)
                0x15, 0x81, //     LOGICAL_MINIMUM (-127)
                0x25, 0x7f, //     LOGICAL_MAXIMUM (127)
                0x75, 0x08, //     REPORT_SIZE (8)
                0x95, 0x03, //     REPORT_COUNT (3)
                0x81, 0x06, //     INPUT (Data,Var,Rel)
                0xc0, //   END_COLLECTION
                0xc0, // END_COLLECTION
        };
    };
    Map map;

    struct HIDInfo {
        uint8_t info[4]{0x11, 0x01, 0x00, 0x01};
    };
    HIDInfo hid_info;

    struct HID_REF {
        uint8_t info[2]{
                0x00,
                0x01,
        };
    };
    HID_REF mouse_ref;
    HID_REF keybrd_ref;
    
    struct CCCD {
        uint8_t info[2]{
                0x00,
                0x00,
        };
    };
    CCCD mouse_cccd;
    CCCD keybrd_cccd;

    struct MouseReport {
        uint8_t button = 0;
        int8_t x = 0;
        int8_t y = 0;
        int8_t wheel = 0;
    } __attribute__((packed));
    MouseReport mouse_report;

    struct KeyBrdReport {
        uint8_t modifier;
        uint8_t keycode[6];
    } __attribute__((packed));
    KeyBrdReport keybrd_report;

    uint8_t protocol_mode = 1;
    uint8_t control_point = 0;

    inline static std::shared_ptr<CHAR_Profile> mouse_report_char;
    inline static std::shared_ptr<CHAR_Profile> keybrd_report_char;

    BLE_MSG_BEGIN;
    register_char(_profile, hid_info, nullptr, ESP_GATT_UUID_HID_INFORMATION, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ);
    register_char(_profile, map, nullptr, ESP_GATT_UUID_HID_REPORT_MAP, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ);
    register_char(_profile, protocol_mode, nullptr, ESP_GATT_UUID_HID_PROTO_MODE, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE_NR);

    // keybrd_ref.info[0] = 0x01;
    // keybrd_ref.info[1] = 0x01;
    // keybrd_report_char = register_char(_profile, keybrd_report, nullptr, ESP_GATT_UUID_HID_REPORT, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY);
    // register_descr(_profile, keybrd_report_char, keybrd_cccd, nullptr, ESP_GATT_UUID_CHAR_CLIENT_CONFIG, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);
    // register_descr(_profile, keybrd_report_char, keybrd_ref, nullptr, ESP_GATT_UUID_RPT_REF_DESCR, ESP_GATT_PERM_READ);

    mouse_ref.info[0] = 0x02;
    mouse_ref.info[1] = 0x01;
    mouse_report_char = register_char(_profile, mouse_report, nullptr, ESP_GATT_UUID_HID_REPORT, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY);
    register_descr(_profile, mouse_report_char, mouse_cccd, nullptr, ESP_GATT_UUID_CHAR_CLIENT_CONFIG, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);
    register_descr(_profile, mouse_report_char, mouse_ref, nullptr, ESP_GATT_UUID_RPT_REF_DESCR, ESP_GATT_PERM_READ);

    register_char(_profile, control_point, nullptr, ESP_GATT_UUID_HID_CONTROL_POINT, 0, ESP_GATT_CHAR_PROP_BIT_WRITE_NR);
    BLE_MSG_END;

    auto registrator() -> void override;

    auto get_uuid() -> uint16_t override {
        return ESP_GATT_UUID_HID_SVC;
    }

private:
};
