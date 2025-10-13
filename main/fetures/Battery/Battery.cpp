#include "Battery.hpp"

void Battery::notify() {
    send_notification();
}

void Battery::send_notification() {
    esp_ble_gatts_send_indicate(app_->gatts_if, app_->conn_id, battery_char_->char_handle, sizeof(level_), &level_, false);
}

auto Battery::registrator() -> void {
    register_ble();
}
