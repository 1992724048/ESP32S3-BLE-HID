#pragma once
#include <chrono>
#include <stdint.h>
#include "parallel_hashmap/phmap.h"

#define RGB(r, g, b) (uint8_t)((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

using namespace std::chrono_literals;

namespace util {
    /**
     * @brief 时间守护结构体，用于记录和计算时间持续
     */
    struct TimeGuard {
        using Clock = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;

        TimeGuard() : f_start_(Clock::now()) {
        }

        /// 更新起始时间
        auto update_start() -> void {
            f_start_ = Clock::now();
        }

        /**
         * @brief 获取持续时间，单位可选
         * @tparam Duration std::chrono::duration 类型（默认秒）
         * @return double 持续时间
         *
         * 示例：
         *   get_duration<std::chrono::seconds>()    → 秒
         *   get_duration<std::chrono::milliseconds>() → 毫秒
         *   get_duration<std::chrono::microseconds>() → 微秒
         *   get_duration<std::chrono::nanoseconds>()  → 纳秒
         */
        template<typename Duration = std::chrono::seconds>
        [[nodiscard]] auto get_duration() const -> double {
            return std::chrono::duration<double, typename Duration::period>(Clock::now() - f_start_).count();
        }

    private:
        TimePoint f_start_;
    };

    template<typename K, typename V>
    using Map = phmap::parallel_flat_hash_map<K, V, phmap::priv::hash_default_hash<K>, phmap::priv::hash_default_eq<K>, std::allocator<std::pair<K, V>>, 4, std::shared_mutex>;
} // namespace util
