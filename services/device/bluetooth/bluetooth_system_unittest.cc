// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/bluetooth/bluetooth_system.h"

#include <utility>

#include "base/observer_list.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "device/bluetooth/dbus/bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/device/device_service_test_base.h"
#include "services/device/public/mojom/bluetooth_system.mojom.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace device {

constexpr const char kFooObjectPathStr[] = "fake/hci0";

namespace {

// Exposes high-level methods to simulate Bluetooth events e.g. a new adapter
// was added, adapter power state changed, etc.
//
// As opposed to FakeBluetoothAdapterClient, the other fake implementation of
// BluetoothAdapterClient, this class does not have any built-in behavior
// e.g. it won't start triggering device discovery events when StartDiscovery is
// called. It's up to its users to call the relevant Simulate*() method to
// trigger each event.
class DEVICE_BLUETOOTH_EXPORT TestBluetoothAdapterClient
    : public bluez::BluetoothAdapterClient {
 public:
  struct Properties : public bluez::BluetoothAdapterClient::Properties {
    explicit Properties(const PropertyChangedCallback& callback)
        : BluetoothAdapterClient::Properties(
              nullptr, /* object_proxy */
              bluetooth_adapter::kBluetoothAdapterInterface,
              callback) {}
    ~Properties() override = default;

    // dbus::PropertySet override
    void Get(dbus::PropertyBase* property,
             dbus::PropertySet::GetCallback callback) override {
      DVLOG(1) << "Get " << property->name();
      NOTIMPLEMENTED();
    }

    void GetAll() override {
      DVLOG(1) << "GetAll";
      NOTIMPLEMENTED();
    }

    void Set(dbus::PropertyBase* property,
             dbus::PropertySet::SetCallback callback) override {
      DVLOG(1) << "Set " << property->name();
      NOTIMPLEMENTED();
    }
  };

  TestBluetoothAdapterClient() = default;
  ~TestBluetoothAdapterClient() override = default;

  // Simulates a new adapter with |object_path_str|. Its properties are empty,
  // 0, or false.
  void SimulateAdapterAdded(const std::string& object_path_str) {
    dbus::ObjectPath object_path(object_path_str);

    ObjectPathToProperties::iterator it;
    bool was_inserted;
    std::tie(it, was_inserted) = adapter_object_paths_to_properties_.emplace(
        object_path,
        base::BindRepeating(&TestBluetoothAdapterClient::OnPropertyChanged,
                            base::Unretained(this), object_path));
    DCHECK(was_inserted);
  }

  // Simulates adapter at |object_path_str| changing its powered state to
  // |powered|.
  void SimulateAdapterPowerStateChanged(const std::string& object_path_str,
                                        bool powered) {
    GetProperties(dbus::ObjectPath(object_path_str))
        ->powered.ReplaceValue(powered);
  }

  // BluetoothAdapterClient:
  void Init(dbus::Bus* bus,
            const std::string& bluetooth_service_name) override {}

  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  std::vector<dbus::ObjectPath> GetAdapters() override {
    std::vector<dbus::ObjectPath> object_paths;
    for (const auto& object_path_to_property :
         adapter_object_paths_to_properties_) {
      object_paths.push_back(object_path_to_property.first);
    }
    return object_paths;
  }

  Properties* GetProperties(const dbus::ObjectPath& object_path) override {
    auto it = adapter_object_paths_to_properties_.find(object_path);
    if (it == adapter_object_paths_to_properties_.end())
      return nullptr;
    return &(it->second);
  }

  void StartDiscovery(const dbus::ObjectPath& object_path,
                      const base::Closure& callback,
                      ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void StopDiscovery(const dbus::ObjectPath& object_path,
                     const base::Closure& callback,
                     ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void PauseDiscovery(const dbus::ObjectPath& object_path,
                      const base::Closure& callback,
                      ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void UnpauseDiscovery(const dbus::ObjectPath& object_path,
                        const base::Closure& callback,
                        ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void RemoveDevice(const dbus::ObjectPath& object_path,
                    const dbus::ObjectPath& device_path,
                    const base::Closure& callback,
                    ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void SetDiscoveryFilter(const dbus::ObjectPath& object_path,
                          const DiscoveryFilter& discovery_filter,
                          const base::Closure& callback,
                          ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void CreateServiceRecord(const dbus::ObjectPath& object_path,
                           const bluez::BluetoothServiceRecordBlueZ& record,
                           const ServiceRecordCallback& callback,
                           ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

  void RemoveServiceRecord(const dbus::ObjectPath& object_path,
                           uint32_t handle,
                           const base::Closure& callback,
                           ErrorCallback error_callback) override {
    NOTIMPLEMENTED();
  }

 private:
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name) {
    for (auto& observer : observers_) {
      observer.AdapterPropertyChanged(object_path, property_name);
    }
  }

  using ObjectPathToProperties = std::map<dbus::ObjectPath, Properties>;
  ObjectPathToProperties adapter_object_paths_to_properties_;

  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace

class BluetoothSystemTest : public DeviceServiceTestBase,
                            public mojom::BluetoothSystemClient {
 public:
  BluetoothSystemTest() = default;
  ~BluetoothSystemTest() override = default;

  void SetUp() override {
    DeviceServiceTestBase::SetUp();
    connector()->BindInterface(mojom::kServiceName, &system_factory_);

    auto test_bluetooth_adapter_client =
        std::make_unique<TestBluetoothAdapterClient>();
    test_bluetooth_adapter_client_ = test_bluetooth_adapter_client.get();

    std::unique_ptr<bluez::BluezDBusManagerSetter> dbus_setter =
        bluez::BluezDBusManager::GetSetterForTesting();
    dbus_setter->SetAlternateBluetoothAdapterClient(
        std::move(test_bluetooth_adapter_client));
  }

  void StateCallback(base::OnceClosure quit_closure,
                     mojom::BluetoothSystem::State state) {
    last_state_ = state;
    std::move(quit_closure).Run();
  }

 protected:
  mojom::BluetoothSystemPtr CreateBluetoothSystem() {
    mojom::BluetoothSystemClientPtr client_ptr;
    system_client_binding_.Bind(mojo::MakeRequest(&client_ptr));

    mojom::BluetoothSystemPtr system_ptr;
    system_factory_->Create(mojo::MakeRequest(&system_ptr),
                            std::move(client_ptr));
    return system_ptr;
  }

  // Saves the last state passed to StateCallback.
  base::Optional<mojom::BluetoothSystem::State> last_state_;

  mojom::BluetoothSystemFactoryPtr system_factory_;

  TestBluetoothAdapterClient* test_bluetooth_adapter_client_;

  mojo::Binding<mojom::BluetoothSystemClient> system_client_binding_{this};

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothSystemTest);
};

TEST_F(BluetoothSystemTest, FactoryCreate) {
  mojom::BluetoothSystemPtr system_ptr;
  mojo::Binding<mojom::BluetoothSystemClient> client_binding(this);

  mojom::BluetoothSystemClientPtr client_ptr;
  client_binding.Bind(mojo::MakeRequest(&client_ptr));

  EXPECT_FALSE(system_ptr.is_bound());

  system_factory_->Create(mojo::MakeRequest(&system_ptr),
                          std::move(client_ptr));
  base::RunLoop run_loop;
  system_ptr.FlushAsyncForTesting(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(system_ptr.is_bound());
}

TEST_F(BluetoothSystemTest, GetState_NoAdapter) {
  auto system = CreateBluetoothSystem();

  base::RunLoop run_loop;
  system->GetState(base::BindOnce(&BluetoothSystemTest::StateCallback,
                                  base::Unretained(this),
                                  run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(mojom::BluetoothSystem::State::kUnavailable, last_state_.value());
}

TEST_F(BluetoothSystemTest, GetState_PoweredOffAdapter) {
  test_bluetooth_adapter_client_->SimulateAdapterAdded(kFooObjectPathStr);
  // Added adapters are Off by default.

  auto system = CreateBluetoothSystem();

  base::RunLoop run_loop;
  system->GetState(base::BindOnce(&BluetoothSystemTest::StateCallback,
                                  base::Unretained(this),
                                  run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOff, last_state_.value());
}

TEST_F(BluetoothSystemTest, GetState_PoweredOnAdapter) {
  test_bluetooth_adapter_client_->SimulateAdapterAdded(kFooObjectPathStr);
  test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(
      kFooObjectPathStr, true);

  auto system = CreateBluetoothSystem();

  base::RunLoop run_loop;
  system->GetState(base::BindOnce(&BluetoothSystemTest::StateCallback,
                                  base::Unretained(this),
                                  run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn, last_state_.value());
}

}  // namespace device
