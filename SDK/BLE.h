#pragma once
#include <windows.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Radios.h>
#include <winrt/Windows.Web.Syndication.h>
#include <windows.devices.bluetooth.h>
#include <windows.foundation.h>
#include<coroutine>
#pragma comment(lib, "windowsapp")
#pragma comment(lib, "WindowsApp.lib")

namespace ble {
    using namespace winrt;
    using namespace Windows::Devices::Bluetooth;
    using namespace Windows::Devices::Bluetooth::Advertisement;
    using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
    using namespace winrt::Windows::Devices::Radios;
    using namespace Windows::Foundation;

    class Descriptor {
    public:
        explicit Descriptor(const GattDescriptor& _desc) : descriptor(_desc) {}

        [[nodiscard]] auto uuid() const -> guid {
            return descriptor.Uuid();
        }

        [[nodiscard]] auto read() const -> IAsyncOperation<GattReadResult> {
            return descriptor.ReadValueAsync(BluetoothCacheMode::Uncached);
        }
    private:
        GattDescriptor descriptor;
    };

    class Characteristic {
    public:
        explicit Characteristic(const GattCharacteristic& _chr) : characteristic(_chr) {
            const auto result = characteristic.GetDescriptorsAsync().get();
            if (result.Status() != GattCommunicationStatus::Success) {
                return;
            }

            for (const auto& d : result.Descriptors()) {
                descriptors.emplace_back(std::make_shared<Descriptor>(d));
            }
        }

        [[nodiscard]] auto uuid() const -> guid {
            return characteristic.Uuid();
        }

        [[nodiscard]] auto read() const -> IAsyncOperation<GattReadResult> {
            return characteristic.ReadValueAsync(BluetoothCacheMode::Uncached);
        }

        template<typename T>
        [[nodiscard]] auto write(const T& _data) const -> IAsyncOperation<GattWriteResult> {
            return characteristic.WriteValueWithResultAsync(to_buffer(_data), GattWriteOption::WriteWithResponse);
        }

        template<typename T>
        [[nodiscard]] auto write_no_response(const T& _data) const -> IAsyncOperation<GattCommunicationStatus> {
            return characteristic.WriteValueAsync(to_buffer(_data), GattWriteOption::WriteWithoutResponse);
        }

        [[nodiscard]] auto register_value_changed(const TypedEventHandler<GattCharacteristic, GattValueChangedEventArgs>& _handler) const -> event_token {
            return characteristic.ValueChanged(_handler);
        }

        [[nodiscard]] auto get_descriptor(const uint32_t _uuid) const -> std::optional<std::shared_ptr<Descriptor>> {
            for (const auto& d : descriptors) {
                if (d->uuid().Data1 == _uuid) {
                    return d;
                }
            }
            return std::nullopt;
        }

    private:
        GattCharacteristic characteristic;
        std::vector<std::shared_ptr<Descriptor>> descriptors;

        template<typename T>
        auto to_buffer(const T& _data) const {
            static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
            const auto writer = Windows::Storage::Streams::DataWriter();
            writer.WriteBytes(std::vector(reinterpret_cast<const uint8_t*>(&_data), reinterpret_cast<const uint8_t*>(&_data) + sizeof(T)));
            return writer.DetachBuffer();
        }
    };

    class Service {
    public:
        explicit Service(const GattDeviceService& _svc) : service(_svc) {
            const auto result = _svc.GetCharacteristicsAsync().get();
            if (result.Status() != GattCommunicationStatus::Success) {
                return;
            }

            for (const auto& c : result.Characteristics()) {
                characteristics.emplace_back(std::make_shared<Characteristic>(c));
            }
        }

        [[nodiscard]] auto uuid() const -> guid {
            return service.Uuid();
        }

        [[nodiscard]] auto get_characteristic(const uint32_t _uuid) const -> std::optional<std::shared_ptr<Characteristic>> {
            for (const auto& c : characteristics) {
                if (c->uuid().Data1 == _uuid) {
                    return c;
                }
            }
            return std::nullopt;
        }

    private:
        GattDeviceService service;
        std::vector<std::shared_ptr<Characteristic>> characteristics;
    };

    class Devices {
    public:
        explicit Devices(const BluetoothLEDevice& _dev) : device(_dev) {
            const auto result = device.GetGattServicesAsync().get();
            if (result.Status() != GattCommunicationStatus::Success) {
                return;
            }

            for (const auto& s : result.Services()) {
                services.emplace_back(std::make_shared<Service>(s));
            }
        }

        [[nodiscard]] auto name() const -> std::wstring {
            return std::wstring(device.Name().data());
        }

        [[nodiscard]] auto address() const -> uint64_t {
            return device.BluetoothAddress();
        }

        [[nodiscard]] auto get_service(const uint32_t _uuid) const -> std::optional<std::shared_ptr<Service>> {
            for (const auto& s : services) {
                if (s->uuid().Data1 == _uuid) {
                    return s;
                }
            }
            return std::nullopt;
        }

    private:
        BluetoothLEDevice device;
        std::vector<std::shared_ptr<Service>> services;
    };

    class BLE {
    public:
        BLE() = delete;
        ~BLE() = delete;

        static auto is_low_energy_supported() -> bool {
            try {
                const auto adapter = BluetoothAdapter::GetDefaultAsync().get();
                return adapter.IsLowEnergySupported() && adapter.GetRadioAsync().get().State() == RadioState::On;
            } catch (...) {
                return false;
            }
        }

        static auto connect(const uint64_t _address) -> std::shared_ptr<Devices> {
            try {
                auto device = BluetoothLEDevice::FromBluetoothAddressAsync(_address).get();
                if (!device) {
                    throw std::runtime_error("无法连接到指定BLE设备");
                }
                return std::make_shared<Devices>(device);
            } catch (const std::exception& e) {
                throw std::runtime_error(std::string("连接失败: ") + e.what());
            }
        }
    };
}
