#include <cstdlib>
#include <random>
#include <stdio.h>
#include "BSP/LCD/lcd.hpp"
#include "BSP/LED/led.hpp"
#include "HID/HID.hpp"
#include "class/hid/hid_device.h"
#include "config/Config.h"
#include "driver/gpio.h"
#include "driver/temperature_sensor.h"
#include "esp_event.h"
#include "esp_freertos_hooks.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "fetures/BLE.hpp"
#include "fetures/Battery/Battery.hpp"
#include "fetures/Features.hpp"
#include "fetures/HID/HID.hpp"
#include "fetures/Event/Event.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "nvs_flash.h"
#include "tinyusb.h"
#include "tusb.h"
#include "util.hpp"

#include <chrono>
#include <esp_chip_info.h>
#include <esp_pm.h>
#include <esp_private/esp_clk.h>
#include <print>
#include <string_view>
#include <thread>
#include "esp_partition.h"
#include "esp_vfs_fat.h"
#include "ff.h"
#include "key.h"
#include "tusb_msc_storage.h"

using namespace std::chrono_literals;

float get_cpu_usage_with_delta() {
    static uint64_t last_total_time = 0;
    static uint64_t last_non_idle_time = 0;

    UBaseType_t uxArraySize = uxTaskGetNumberOfTasks();
    TaskStatus_t* pxTaskStatusArray = static_cast<TaskStatus_t*>(pvPortMalloc(uxArraySize * sizeof(TaskStatus_t)));

    if (!pxTaskStatusArray)
        return 0.0f;

    uint64_t current_total_time;
    uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &current_total_time);

    if (current_total_time == 0) {
        vPortFree(pxTaskStatusArray);
        return 0.0f;
    }

    uint64_t current_non_idle_time = 0;
    for (UBaseType_t i = 0; i < uxArraySize; ++i) {
        if (!strstr(pxTaskStatusArray[i].pcTaskName, "IDLE")) {
            current_non_idle_time += pxTaskStatusArray[i].ulRunTimeCounter;
        }
    }
    vPortFree(pxTaskStatusArray);

    if (last_total_time == 0) {
        last_total_time = current_total_time;
        last_non_idle_time = current_non_idle_time;
        return 0.0f;
    }

    uint64_t delta_total = current_total_time - last_total_time;
    uint64_t delta_non_idle = current_non_idle_time - last_non_idle_time;

    last_total_time = current_total_time;
    last_non_idle_time = current_non_idle_time;

    if (delta_total == 0)
        return 0.0f;

    return static_cast<float>(delta_non_idle) / delta_total;
}

util::TimeGuard tg;
temperature_sensor_handle_t temp_handle = NULL;
void temperature_sensor_init(void) {
    temperature_sensor_config_t temp_sensor;
    temp_sensor.range_min = -10;
    temp_sensor.range_max = 80;
    temp_sensor.clk_src = TEMPERATURE_SENSOR_CLK_SRC_DEFAULT;
    temp_sensor.flags.allow_pd = 0;
    ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor, &temp_handle));
    ESP_ERROR_CHECK(temperature_sensor_enable(temp_handle));
}

short sensor_get_temperature(void) {
    float temp;
    ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_handle, &temp));
    return temp;
}

static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
extern "C" void app_main(void) {
    esp_err_t ret;
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    // 初始化外围设备
    led_init();
    spi2_init();
    key_init();
    lcd_init();
    lcd_clear(0xffff);
    temperature_sensor_init();

    // WIFI相关初始化
    ret = esp_netif_init();
    ESP_ERROR_CHECK(ret);

    // 挂载数据存储分区
    esp_vfs_fat_mount_config_t mount_config = {.format_if_mount_failed = true, .max_files = 16};
    ret = esp_vfs_fat_spiflash_mount_rw_wl("/FATFS", "FATFS", &mount_config, &s_wl_handle);

    config::initialize("/FATFS/config.json");
    FeatureBase::initialize();
    BLEBase::initialize();
    FeatureBase::commit_config();

    int count = FeatureBase::get_count();
    float time = tg.get_duration<std::chrono::milliseconds>();
    ESP_LOGI("启动耗时", "%.3f ms", time);
    ESP_LOGI("功能数量", "%d", count);

    std::string_view title("ESP32-S3M BLE MODULE");
    lcd_show_string(0, 0, title.size() * 16, 16, 16, title.data(), RGB(0, 153, 255));
    std::string addr = BLEBase::get_address();
    lcd_show_string(0, 16, addr.size() * 16, 16, 16, addr.data(), RGB(0, 153, 255));

    Event::instance()->touch();

    bool condition = false;
    std::thread([&condition] {
        while (true) {
            if (condition) {
                HID::instance()->click(1);
            }
            std::this_thread::sleep_for(5ms);
        }
    }).detach();

    while (true) {
        LED_TOGGLE();

        if (key_scan(0)) {
            condition = !condition;
        }

        Battery::instance()->notify();

        size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
        size_t total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
        float mem_usage = ((float)(total_heap - free_heap) / total_heap) * 100;
        float cpu_usage = get_cpu_usage_with_delta() * 100;
        float temperature = sensor_get_temperature();

        esp_chip_info_t info{};
        esp_chip_info(&info);
        uint32_t cpu_freq_mhz = esp_clk_cpu_freq() / 1000000;
        std::string cpu_str = std::format("{:<10}|{}MHz/{}   ", std::format("CPU:{:03.1f}%", cpu_usage), cpu_freq_mhz, temperature);
        lcd_show_string(0, 32, cpu_str.size() * 16, 16, 16, cpu_str.data(), RGB(0, 153, 255));

        std::string mem_str = std::format("{:<10}|F:{:>5}KB   ", std::format("MEM:{:03.1f}%", mem_usage), free_heap / 1024);
        lcd_show_string(0, 48, mem_str.size() * 16, 16, 16, mem_str.data(), RGB(0, 153, 255));

        uint32_t fre_clust = 0;
        FATFS* fatfs = nullptr;

        FRESULT fres = f_getfree("FATFS", &fre_clust, &fatfs);
        if (fres != FR_OK) {
            ESP_LOGE("FAT", "f_getfree failed: %d", fres);
        } else {
            uint32_t total_sectors = (fatfs->n_fatent - 2) * fatfs->csize;
            uint32_t free_sectors = fre_clust * fatfs->csize;
            uint32_t used_sectors = total_sectors - free_sectors;

            size_t total = total_sectors * 512;
            size_t used = used_sectors * 512;

            std::string time_str = std::format("{:<10}|U:{:>5}KB   ", std::format("DAT:{:03.1f}%", 100.f - ((float)(total - used) / total) * 100), used / 1024);

            lcd_show_string(0, 64, time_str.size() * 16, 16, 16, time_str.data(), RGB(0, 153, 255));
        }
        std::this_thread::sleep_for(500ms);
    }
}
