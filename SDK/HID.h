#pragma once
#include <BLE.h>
#include <numbers>
#include <random>

namespace hid {
    using namespace std::chrono_literals;
    using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;

    class Mouse {
    public:
        static auto connect(const uint64_t _address) -> bool {
            const auto devices = ble::BLE::connect(_address);
            const auto service = devices->get_service(0x843A);
            if (!service) {
                return false;
            }

            const auto char_click = service.value()->get_characteristic(0xEF01);
            if (!char_click) {
                return false;
            }

            const auto char_move = service.value()->get_characteristic(0xEF02);
            if (!char_move) {
                return false;
            }

            const auto char_wheel = service.value()->get_characteristic(0xEF03);
            if (!char_wheel) {
                return false;
            }

            click_char = char_click.value();
            move_char = char_move.value();
            wheel_char = char_wheel.value();
            return true;
        }

        /**
         * @brief 移动鼠标
         * @param _x 水平方向相对像素（正=右，负=左）
         * @param _y 垂直方向相对像素（正=下，负=上）
         * @return 是否成功发送
         */
        static auto move(const int _x, const int _y) -> bool {
            Move data;
            data.x = _x;
            data.y = _y;
            const auto result = move_char->write(data).get();
            return result.Status() == GattCommunicationStatus::Success;
        }

        /**
         * @brief 模拟人手移动鼠标
         * @param _rel_x  水平方向相对像素（正=右，负=左）
         * @param _rel_y  垂直方向相对像素（正=下，负=上）
         * @param _duration_us 每段轨迹耗时
         */
        static auto human_move(const int _rel_x, const int _rel_y, const int _duration_us = 50) -> void {
            POINT pt{};
            GetCursorPos(&pt);
            const int x0 = pt.x;
            const int y0 = pt.y;
            const int x1 = x0 + _rel_x;
            const int y1 = y0 + _rel_y;
            const int dx = x1 - x0;
            const int dy = y1 - y0;
            const double dist = std::hypot(dx, dy);
            if (dist < 0.5) {
                return;
            }

            constexpr int steps = 3;
            const double base_delay = _duration_us / static_cast<double>(steps);
            const double cx = (x0 + x1) / 2.0 + rand_real(-(_rel_x * 0.3f), _rel_x * 0.3f);
            const double cy = (y0 + y1) / 2.0 + rand_real(-(_rel_y * 0.3f), _rel_y * 0.3f);
            const double wave_amp = rand_real(1.0, 2.0);
            const double wave_prd = rand_real(1.0, 2.0);

            auto bezier = [&](const double _t) {
                const double u = 1 - _t;
                double x = u * u * x0 + 2 * u * _t * cx + _t * _t * x1;
                double y = u * u * y0 + 2 * u * _t * cy + _t * _t * y1;
                return std::make_pair(x, y);
            };

            const double nx = -dy / dist;
            const double ny = dx / dist;
            double px = x0, py = y0;
            for (int i = 1; i <= steps; ++i) {
                const double t = i / static_cast<double>(steps);
                auto [bx, by] = bezier(t);
                const double wave = wave_amp * std::sin(i / wave_prd * 2 * std::numbers::pi);
                bx += wave * nx;
                by += wave * ny;

                bx += rand_real(-1, 1);
                by += rand_real(-1, 1);

                const int mx = static_cast<int>(std::round(bx - px));
                const int my = static_cast<int>(std::round(by - py));
                move(mx, my);
                px = bx;
                py = by;

                const double ratio = 1.0 - std::cos(t * std::numbers::pi) * 0.3;
                int delay = static_cast<int>(base_delay * ratio);
                std::this_thread::sleep_for(std::chrono::microseconds(delay));
                std::this_thread::sleep_for(1ms);
            }
        }

        static auto click(const uint8_t _button) -> bool {
            Click data;
            data.button = 1 << _button;
            const auto result = click_char->write(data).get();
            return result.Status() == GattCommunicationStatus::Success;
        }

        static auto wheel(const int8_t _v) -> bool {
            Wheel data;
            data.wheel = _v;
            const auto result = wheel_char->write(data).get();
            return result.Status() == GattCommunicationStatus::Success;
        }

    private:
        struct Move {
            int x = 0;
            int y = 0;
        };

        struct Click {
            uint16_t button = 0;
        };

        struct Wheel {
            int8_t wheel = 0;
        };

        inline static std::shared_ptr<ble::Characteristic> move_char;
        inline static std::shared_ptr<ble::Characteristic> click_char;
        inline static std::shared_ptr<ble::Characteristic> wheel_char;

        static auto rng() -> std::mt19937& {
            static std::mt19937 e{std::random_device{}()};
            return e;
        }

        template<typename T>
        static auto rand_int(T _a, T _b) -> T {
            std::uniform_int_distribution<T> d(_a, _b);
            return d(rng());
        }

        template<typename T>
        static auto rand_real(T _a, T _b) -> T {
            std::uniform_real_distribution d(static_cast<double>(_a), static_cast<double>(_b));
            return static_cast<T>(d(rng()));
        }
    };
}
