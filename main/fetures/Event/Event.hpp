#pragma once
#include "../BLE.hpp"
#include "../Features.hpp"
#include "../HID/HID.hpp"
#include "config/Config.h"
#include "esp_log.h"

class Event : public FeatureRegistrar<Event>, public BLERegistrar<Event> {
public:
    Event();
    ~Event();

    enum { CLICK, MOVE, WHEEL };

    struct CLICK_Data {
        uint16_t button = 0;
    };
    CLICK_Data click_data;

    struct MOVE_Data {
        int x;
        int y;
    };
    MOVE_Data move_data;

    struct WHEEL_Data {
        uint8_t wheel;
    };
    WHEEL_Data wheel_data;

    auto registrator() -> void override;

    BLE_MSG_BEGIN;
    register_char(
            _profile, click_data, BLE_MSG(click_event), 0xEF01, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY);
    register_char(
            _profile, move_data, BLE_MSG(move_event), 0xEF02, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY);
    register_char(
            _profile, wheel_data, BLE_MSG(wheel_event), 0xEF03, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY);
    BLE_MSG_END;

    BLE_MSG_FUNC(click_event) {
        HID::instance()->click(click_data.button);
        return ESP_GATT_OK;
    }

    BLE_MSG_FUNC(move_event) {
        while (move_data.x != 0 || move_data.y != 0) {
            int8_t step_x = static_cast<int8_t>(std::clamp<int32_t>(move_data.x, -128, 127));
            int8_t step_y = static_cast<int8_t>(std::clamp<int32_t>(move_data.y, -128, 127));

            const uint8_t ux = *reinterpret_cast<uint8_t*>(&step_x);
            const uint8_t uy = *reinterpret_cast<uint8_t*>(&step_y);

            HID::instance()->move(ux, uy);

            move_data.x -= step_x;
            move_data.y -= step_y;
        }
        return ESP_GATT_OK;
    }

    BLE_MSG_FUNC(wheel_event) {
        HID::instance()->wheel(wheel_data.wheel);
        return ESP_GATT_OK;
    }

private:
};
