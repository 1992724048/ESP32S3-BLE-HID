#pragma once
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <thread>
#include <vector>
#include "../config/Config.h"
#include "../config/Field.h"
#include "../util.hpp"

#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_log.h"

class FeatureBase {
public:
    static auto get_count() -> size_t {
        return fetures.size();
    }

    static auto initialize() -> void {
        config::setup_update(&config_update);
        for (const std::function<void()>& function : creates) {
            function();
        }
    }

    static auto commit_config() -> void {
        config_update();
    }

    template<class T>
    static auto get_feature(const std::string& _name) -> std::optional<std::shared_ptr<T>>
        requires(std::is_base_of_v<FeatureBase, T>)
    {
        if (!fetures.contains(_name)) {
            return std::nullopt;
        }
        return std::static_pointer_cast<T>(fetures[_name]);
    }

    virtual std::string_view get_name() {
        return "FeatureBase";
    }

    FeatureBase(const FeatureBase&) = delete;
    auto operator=(const FeatureBase&) -> FeatureBase& = delete;
    FeatureBase(FeatureBase&&) = delete;
    auto operator=(FeatureBase&&) -> FeatureBase& = delete;

protected:
    ~FeatureBase() = default;
    FeatureBase() = default;

    static auto add_feature(const std::string& _name, const std::shared_ptr<FeatureBase>& _that) -> void {
        fetures[_name] = _that;
    }

    inline static std::vector<std::function<void()>> creates;

private:
    inline static TEvent<> config_update;
    inline static util::Map<std::string, std::shared_ptr<FeatureBase>> fetures;
};

template<typename T>
class FeatureRegistrar : public FeatureBase {
public:
    static auto instance() -> std::shared_ptr<T>
        requires(std::is_base_of_v<FeatureBase, T>)
    {
        return feature_instance;
    }

    virtual auto touch() -> void* {
        return &registrator_;
    }

    FeatureRegistrar(const FeatureRegistrar&) = delete;
    auto operator=(const FeatureRegistrar&) -> FeatureRegistrar& = delete;
    FeatureRegistrar(FeatureRegistrar&&) = delete;
    auto operator=(FeatureRegistrar&&) -> FeatureRegistrar& = delete;

    virtual auto registrator() -> void {
    }

protected:
    FeatureRegistrar() {
        touch();
    }
    ~FeatureRegistrar() = default;

private:
    class Registrator {
    public:
        Registrator() {
            creates.emplace_back(create);
        }
    };

    inline static __unused Registrator registrator_;
    inline static __unused std::shared_ptr<T> feature_instance;

    static auto create() -> void {
        feature_instance = std::static_pointer_cast<T>(std::make_shared<T>());
        auto name = std::string(typeid(T).name()).substr(1);
        add_feature(name, feature_instance);
        ESP_LOGI("功能", "已添加功能->%s", name.data());
        feature_instance->registrator();
    }

    friend T;
};

#define REGISTER_FEATURE(T)                                                                                                                                                                            \
    namespace {                                                                                                                                                                                        \
        static auto _force_##T = T::instance()->touch();                                                                                                                                               \
    }