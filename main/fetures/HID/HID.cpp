#include "HID.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

HID::HID() {
    touch();
}

HID::~HID() {
    touch();
}

void HID::registrator() {
    register_ble();
}

void HID::click(uint8_t button) {
    std::this_thread::sleep_for(1ms);
    mouse_report.button |= button;
    send(app_, mouse_report_char);
    mouse_report.button = 0;
    send(app_, mouse_report_char);
}

void HID::move(int8_t x, int8_t y) {
    std::this_thread::sleep_for(1ms);
    mouse_report.x = x;
    mouse_report.y = y;
    send(app_, mouse_report_char);
    mouse_report.x = 0;
    mouse_report.y = 0;
}
void HID::wheel(int8_t v) {
    std::this_thread::sleep_for(1ms);
    mouse_report.wheel = v;
    send(app_, mouse_report_char);
    mouse_report.wheel = 0;
}
