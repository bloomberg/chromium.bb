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
constexpr const char kBarObjectPathStr[] = "fake/hci1";

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

    for (auto& observer : observers_)
      observer.AdapterAdded(object_path);
  }

  // Simulates the adapter at |object_path_str| being removed.
  void SimulateAdapterRemoved(const std::string& object_path_str) {
    dbus::ObjectPath object_path(object_path_str);

    // Properties are set to empty, 0, or false right before AdapterRemoved is
    // called.
    GetProperties(object_path)->powered.ReplaceValue(false);

    // When BlueZ calls into AdapterRemoved, the adapter is still exposed
    // through GetAdapters() and its properties are still accessible.
    for (auto& observer : observers_)
      observer.AdapterRemoved(object_path);

    size_t removed = adapter_object_paths_to_properties_.erase(object_path);
    DCHECK_EQ(1u, removed);
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
    get_state_result_ = state;
    std::move(quit_closure).Run();
  }

  // mojom::BluetoothSystemClient
  void OnStateChanged(mojom::BluetoothSystem::State state) override {
    on_state_changed_states_.push_back(state);
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

  void ResetResults() {
    get_state_result_.reset();
    on_state_changed_states_.clear();
  }

  // Saves the last state passed to StateCallback.
  base::Optional<mojom::BluetoothSystem::State> get_state_result_;

  // Saves the states passed to OnStateChanged.
  using StateVector = std::vector<mojom::BluetoothSystem::State>;
  StateVector on_state_changed_states_;

  mojom::BluetoothSystemFactoryPtr system_factory_;

  TestBluetoothAdapterClient* test_bluetooth_adapter_client_;

  mojo::Binding<mojom::BluetoothSystemClient> system_client_binding_{this};

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothSystemTest);
};

// Tests that the Create method for BluetoothSystemFactory works.
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

// Tests that the state is 'Unavailable' when there is no Bluetooth adapter
// present.
TEST_F(BluetoothSystemTest, State_NoAdapter) {
  auto system = CreateBluetoothSystem();

  base::RunLoop run_loop;
  system->GetState(base::BindOnce(&BluetoothSystemTest::StateCallback,
                                  base::Unretained(this),
                                  run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(mojom::BluetoothSystem::State::kUnavailable,
            get_state_result_.value());
  EXPECT_TRUE(on_state_changed_states_.empty());
}

// Tests that the state is "Off" when the Bluetooth adapter is powered off.
TEST_F(BluetoothSystemTest, State_PoweredOffAdapter) {
  test_bluetooth_adapter_client_->SimulateAdapterAdded(kFooObjectPathStr);
  // Added adapters are Off by default.

  auto system = CreateBluetoothSystem();

  base::RunLoop run_loop;
  system->GetState(base::BindOnce(&BluetoothSystemTest::StateCallback,
                                  base::Unretained(this),
                                  run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOff,
            get_state_result_.value());
  EXPECT_TRUE(on_state_changed_states_.empty());
}

// Tests that the state is "On" when the Bluetooth adapter is powered on.
TEST_F(BluetoothSystemTest, State_PoweredOnAdapter) {
  test_bluetooth_adapter_client_->SimulateAdapterAdded(kFooObjectPathStr);
  test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(
      kFooObjectPathStr, true);

  auto system = CreateBluetoothSystem();

  base::RunLoop run_loop;
  system->GetState(base::BindOnce(&BluetoothSystemTest::StateCallback,
                                  base::Unretained(this),
                                  run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
            get_state_result_.value());
  EXPECT_TRUE(on_state_changed_states_.empty());
}

// Tests that the state changes to On when the adapter turns on and then changes
// to Off when the adapter turns off.
TEST_F(BluetoothSystemTest, State_PoweredOnThenOff) {
  test_bluetooth_adapter_client_->SimulateAdapterAdded(kFooObjectPathStr);

  auto system = CreateBluetoothSystem();

  {
    // The adapter is initially powered off.
    base::RunLoop run_loop;
    system->GetState(base::BindOnce(&BluetoothSystemTest::StateCallback,
                                    base::Unretained(this),
                                    run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOff,
              get_state_result_.value());
    EXPECT_TRUE(on_state_changed_states_.empty());
    ResetResults();
  }

  {
    // Turn adapter on.
    test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(
        kFooObjectPathStr, true);

    base::RunLoop run_loop;
    system->GetState(base::BindOnce(&BluetoothSystemTest::StateCallback,
                                    base::Unretained(this),
                                    run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
              get_state_result_.value());
    EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOn}),
              on_state_changed_states_);
    ResetResults();
  }

  {
    // Turn adapter off.
    test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(
        kFooObjectPathStr, false);

    base::RunLoop run_loop;
    system->GetState(base::BindOnce(&BluetoothSystemTest::StateCallback,
                                    base::Unretained(this),
                                    run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOff,
              get_state_result_.value());
    EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOff}),
              on_state_changed_states_);
  }
}

// Tests that the state is updated as expected when removing and re-adding the
// same adapter.
TEST_F(BluetoothSystemTest, State_AdapterRemoved) {
  test_bluetooth_adapter_client_->SimulateAdapterAdded(kFooObjectPathStr);
  test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(
      kFooObjectPathStr, true);

  auto system = CreateBluetoothSystem();

  {
    // The adapter is initially powered on.
    base::RunLoop run_loop;
    system->GetState(base::BindOnce(&BluetoothSystemTest::StateCallback,
                                    base::Unretained(this),
                                    run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
              get_state_result_.value());
    EXPECT_TRUE(on_state_changed_states_.empty());
    ResetResults();
  }

  {
    // Remove the adapter. The state should change to Unavailable.
    test_bluetooth_adapter_client_->SimulateAdapterRemoved(kFooObjectPathStr);
    base::RunLoop run_loop;
    system->GetState(base::BindOnce(&BluetoothSystemTest::StateCallback,
                                    base::Unretained(this),
                                    run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_EQ(mojom::BluetoothSystem::State::kUnavailable,
              get_state_result_.value());
    EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOff,
                           mojom::BluetoothSystem::State::kUnavailable}),
              on_state_changed_states_);
    ResetResults();
  }

  {
    // Add the adapter again; it's off by default.
    test_bluetooth_adapter_client_->SimulateAdapterAdded(kFooObjectPathStr);
    base::RunLoop run_loop;
    system->GetState(base::BindOnce(&BluetoothSystemTest::StateCallback,
                                    base::Unretained(this),
                                    run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOff,
              get_state_result_.value());
    EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOff}),
              on_state_changed_states_);
  }
}

// Tests that the state is updated as expected when replacing the adapter with a
// different adapter.
TEST_F(BluetoothSystemTest, State_AdapterReplaced) {
  // Start with a powered on adapter.
  test_bluetooth_adapter_client_->SimulateAdapterAdded(kFooObjectPathStr);
  test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(
      kFooObjectPathStr, true);

  auto system = CreateBluetoothSystem();

  {
    // The adapter is initially powered on.
    base::RunLoop run_loop;
    system->GetState(base::BindOnce(&BluetoothSystemTest::StateCallback,
                                    base::Unretained(this),
                                    run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
              get_state_result_.value());
    EXPECT_TRUE(on_state_changed_states_.empty());
    ResetResults();
  }

  {
    // Remove the adapter. The state should change to Unavailable.
    test_bluetooth_adapter_client_->SimulateAdapterRemoved(kFooObjectPathStr);
    base::RunLoop run_loop;
    system->GetState(base::BindOnce(&BluetoothSystemTest::StateCallback,
                                    base::Unretained(this),
                                    run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_EQ(mojom::BluetoothSystem::State::kUnavailable,
              get_state_result_.value());
    EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOff,
                           mojom::BluetoothSystem::State::kUnavailable}),
              on_state_changed_states_);
    ResetResults();
  }

  {
    // Add a different adapter. it's off by default.
    test_bluetooth_adapter_client_->SimulateAdapterAdded(kBarObjectPathStr);
    base::RunLoop run_loop;
    system->GetState(base::BindOnce(&BluetoothSystemTest::StateCallback,
                                    base::Unretained(this),
                                    run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOff,
              get_state_result_.value());
    EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOff}),
              on_state_changed_states_);
  }
}

// Tests that the state is correctly updated when adding and removing multiple
// adapters.
TEST_F(BluetoothSystemTest, State_AddAndRemoveMultipleAdapters) {
  // Start with a powered on "foo" adapter.
  test_bluetooth_adapter_client_->SimulateAdapterAdded(kFooObjectPathStr);
  test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(
      kFooObjectPathStr, true);

  auto system = CreateBluetoothSystem();

  {
    // The "foo" adapter is initially powered on.
    base::RunLoop run_loop;
    system->GetState(base::BindOnce(&BluetoothSystemTest::StateCallback,
                                    base::Unretained(this),
                                    run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
              get_state_result_.value());
    EXPECT_TRUE(on_state_changed_states_.empty());
    ResetResults();
  }

  {
    // Add an extra "bar" adapter. The state should not change.
    test_bluetooth_adapter_client_->SimulateAdapterAdded(kBarObjectPathStr);
    base::RunLoop run_loop;
    system->GetState(base::BindOnce(&BluetoothSystemTest::StateCallback,
                                    base::Unretained(this),
                                    run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
              get_state_result_.value());
    EXPECT_TRUE(on_state_changed_states_.empty());
    ResetResults();
  }

  {
    // Remove "foo". We should retrieve the state from "bar".
    test_bluetooth_adapter_client_->SimulateAdapterRemoved(kFooObjectPathStr);
    base::RunLoop run_loop;
    system->GetState(base::BindOnce(&BluetoothSystemTest::StateCallback,
                                    base::Unretained(this),
                                    run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOff,
              get_state_result_.value());
    EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOff}),
              on_state_changed_states_);
    ResetResults();
  }

  {
    // Change "bar"'s state to On.
    test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(
        kBarObjectPathStr, true);
    base::RunLoop run_loop;
    system->GetState(base::BindOnce(&BluetoothSystemTest::StateCallback,
                                    base::Unretained(this),
                                    run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
              get_state_result_.value());
    EXPECT_EQ(StateVector({mojom::BluetoothSystem::State::kPoweredOn}),
              on_state_changed_states_);
    ResetResults();
  }

  {
    // Add "foo" again. We should still retrieve the state from "bar".
    test_bluetooth_adapter_client_->SimulateAdapterAdded(kFooObjectPathStr);
    base::RunLoop run_loop;
    system->GetState(base::BindOnce(&BluetoothSystemTest::StateCallback,
                                    base::Unretained(this),
                                    run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
              get_state_result_.value());
    EXPECT_TRUE(on_state_changed_states_.empty());
  }
}

// Tests that an extra adapter changing state does not interfer with the state.
TEST_F(BluetoothSystemTest, State_ChangeStateMultipleAdapters) {
  // Start with a powered on "foo" adapter.
  test_bluetooth_adapter_client_->SimulateAdapterAdded(kFooObjectPathStr);
  test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(
      kFooObjectPathStr, true);

  auto system = CreateBluetoothSystem();

  {
    // The "foo" adapter is initially powered on.
    base::RunLoop run_loop;
    system->GetState(base::BindOnce(&BluetoothSystemTest::StateCallback,
                                    base::Unretained(this),
                                    run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
              get_state_result_.value());
    EXPECT_TRUE(on_state_changed_states_.empty());
    ResetResults();
  }

  {
    // Add an extra "bar" adapter. The state should not change.
    test_bluetooth_adapter_client_->SimulateAdapterAdded(kBarObjectPathStr);
    base::RunLoop run_loop;
    system->GetState(base::BindOnce(&BluetoothSystemTest::StateCallback,
                                    base::Unretained(this),
                                    run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
              get_state_result_.value());
    EXPECT_TRUE(on_state_changed_states_.empty());
    ResetResults();
  }

  {
    // Turn "bar" on. The state should not change.
    test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(
        kBarObjectPathStr, true);
    base::RunLoop run_loop;
    system->GetState(base::BindOnce(&BluetoothSystemTest::StateCallback,
                                    base::Unretained(this),
                                    run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
              get_state_result_.value());
    EXPECT_TRUE(on_state_changed_states_.empty());
    ResetResults();
  }

  {
    // Turn "bar" off. The state should not change.
    test_bluetooth_adapter_client_->SimulateAdapterPowerStateChanged(
        kBarObjectPathStr, false);
    base::RunLoop run_loop;
    system->GetState(base::BindOnce(&BluetoothSystemTest::StateCallback,
                                    base::Unretained(this),
                                    run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_EQ(mojom::BluetoothSystem::State::kPoweredOn,
              get_state_result_.value());
    EXPECT_TRUE(on_state_changed_states_.empty());
    ResetResults();
  }
}

}  // namespace device
