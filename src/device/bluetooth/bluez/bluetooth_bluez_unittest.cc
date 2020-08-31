// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "device/bluetooth/bluez/bluetooth_device_bluez.h"
#include "device/bluetooth/bluez/bluetooth_pairing_bluez.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_agent_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_service_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_input_client.h"
#include "device/bluetooth/test/test_bluetooth_adapter_observer.h"
#include "device/bluetooth/test/test_pairing_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

#if defined(OS_CHROMEOS)
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/data_decoder/public/mojom/ble_scan_parser.mojom.h"
#endif

using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;
using device::BluetoothDevice;
using device::BluetoothDeviceType;
using device::BluetoothDiscoveryFilter;
using device::BluetoothDiscoverySession;
using device::BluetoothUUID;
using device::TestBluetoothAdapterObserver;
using device::TestPairingDelegate;

namespace bluez {

namespace {

// Callback for BluetoothDevice::GetConnectionInfo() that simply saves the
// connection info to the bound argument.
void SaveConnectionInfo(BluetoothDevice::ConnectionInfo* out,
                        const BluetoothDevice::ConnectionInfo& conn_info) {
  *out = conn_info;
}

// Find |address| in |devices|, if found returns the index otherwise returns -1.
int GetDeviceIndexByAddress(const BluetoothAdapter::DeviceList& devices,
                            const char* address) {
  int idx = -1;
  for (auto* device : devices) {
    ++idx;
    if (device->GetAddress().compare(address) == 0)
      return idx;
  }
  return -1;
}

#if defined(OS_CHROMEOS)
class FakeBleScanParserImpl : public data_decoder::mojom::BleScanParser {
 public:
  FakeBleScanParserImpl() = default;
  ~FakeBleScanParserImpl() override = default;

  // mojom::BleScanParser:
  void Parse(const std::vector<uint8_t>& advertisement_data,
             ParseCallback callback) override {
    std::move(callback).Run(nullptr);
  }

  DISALLOW_COPY_AND_ASSIGN(FakeBleScanParserImpl);
};
#endif

class FakeBluetoothProfileServiceProviderDelegate
    : public bluez::BluetoothProfileServiceProvider::Delegate {
 public:
  FakeBluetoothProfileServiceProviderDelegate() = default;

  // bluez::BluetoothProfileServiceProvider::Delegate:
  void Released() override {}

  void NewConnection(
      const dbus::ObjectPath&,
      base::ScopedFD,
      const bluez::BluetoothProfileServiceProvider::Delegate::Options&,
      ConfirmationCallback) override {}

  void RequestDisconnection(const dbus::ObjectPath&,
                            ConfirmationCallback) override {}

  void Cancel() override {}
};

}  // namespace


class BluetoothBlueZTest : public testing::Test {
 public:
  static const char kGapUuid[];
  static const char kGattUuid[];
  static const char kPnpUuid[];
  static const char kHeadsetUuid[];

  void SetUp() override {
    std::unique_ptr<bluez::BluezDBusManagerSetter> dbus_setter =
        bluez::BluezDBusManager::GetSetterForTesting();
    // We need to initialize BluezDBusManager early to prevent
    // Bluetooth*::Create() methods from picking the real instead of fake
    // implementations.
    fake_bluetooth_adapter_client_ = new bluez::FakeBluetoothAdapterClient;
    dbus_setter->SetBluetoothAdapterClient(
        std::unique_ptr<bluez::BluetoothAdapterClient>(
            fake_bluetooth_adapter_client_));

    fake_bluetooth_device_client_ = new bluez::FakeBluetoothDeviceClient;
    // Use the original fake behavior for these tests.
    fake_bluetooth_device_client_->set_delay_start_discovery(true);
    dbus_setter->SetBluetoothDeviceClient(
        std::unique_ptr<bluez::BluetoothDeviceClient>(
            fake_bluetooth_device_client_));
    dbus_setter->SetBluetoothInputClient(
        std::unique_ptr<bluez::BluetoothInputClient>(
            new bluez::FakeBluetoothInputClient));
    dbus_setter->SetBluetoothAgentManagerClient(
        std::unique_ptr<bluez::BluetoothAgentManagerClient>(
            new bluez::FakeBluetoothAgentManagerClient));
    dbus_setter->SetBluetoothGattServiceClient(
        std::unique_ptr<bluez::BluetoothGattServiceClient>(
            new bluez::FakeBluetoothGattServiceClient));

    fake_bluetooth_adapter_client_->SetSimulationIntervalMs(10);

#if defined(OS_CHROMEOS)
    device::BluetoothAdapterFactory::SetBleScanParserCallback(
        base::BindLambdaForTesting([&]() {
          mojo::PendingRemote<data_decoder::mojom::BleScanParser>
              ble_scan_parser;
          mojo::MakeSelfOwnedReceiver(
              std::make_unique<FakeBleScanParserImpl>(),
              ble_scan_parser.InitWithNewPipeAndPassReceiver());
          return ble_scan_parser;
        }));
#endif

    callback_count_ = 0;
    error_callback_count_ = 0;
    last_connect_error_ = BluetoothDevice::ERROR_UNKNOWN;
    last_client_error_ = "";
  }

  void TearDown() override {
#if defined(OS_CHROMEOS)
    device::BluetoothAdapterFactory::SetBleScanParserCallback(
        base::NullCallback());
#endif

    discovery_sessions_.clear();
    adapter_.reset();
    bluez::BluezDBusManager::Shutdown();
  }

  // Generic callbacks
  void Callback() {
    ++callback_count_;
    QuitMessageLoop();
  }

  void CallbackWithClosure(base::OnceClosure closure) {
    ++callback_count_;
    std::move(closure).Run();
  }

  base::Closure GetCallback() {
    return base::Bind(&BluetoothBlueZTest::Callback, base::Unretained(this));
  }
  base::OnceClosure GetOnceCallback() {
    return base::BindOnce(&BluetoothBlueZTest::Callback,
                          base::Unretained(this));
  }

  void DiscoverySessionCallback(
      std::unique_ptr<BluetoothDiscoverySession> discovery_session) {
    ++callback_count_;
    discovery_sessions_.push_back(std::move(discovery_session));
    QuitMessageLoop();
  }

  void DiscoverySessionCallbackWithClosure(
      base::OnceClosure closure,
      std::unique_ptr<BluetoothDiscoverySession> discovery_session) {
    ++callback_count_;
    discovery_sessions_.push_back(std::move(discovery_session));
    std::move(closure).Run();
  }

  void ProfileRegisteredCallback(BluetoothAdapterProfileBlueZ* profile) {
    adapter_profile_ = profile;
    ++callback_count_;
    QuitMessageLoop();
  }

  void ErrorCallback() {
    ++error_callback_count_;
    QuitMessageLoop();
  }

  void DiscoveryErrorCallback(device::UMABluetoothDiscoverySessionOutcome) {
    ErrorCallback();
  }

  base::Closure GetErrorCallback() {
    return base::Bind(&BluetoothBlueZTest::ErrorCallback,
                      base::Unretained(this));
  }

  base::Callback<void(device::UMABluetoothDiscoverySessionOutcome)>
  GetDiscoveryErrorCallback() {
    return base::Bind(&BluetoothBlueZTest::DiscoveryErrorCallback,
                      base::Unretained(this));
  }

  void DBusErrorCallback(const std::string& error_name,
                         const std::string& error_message) {
    ++error_callback_count_;
    last_client_error_ = error_name;
    QuitMessageLoop();
  }

  void ConnectErrorCallback(BluetoothDevice::ConnectErrorCode error) {
    ++error_callback_count_;
    last_connect_error_ = error;
  }

  void ErrorCompletionCallback(const std::string& error_message) {
    ++error_callback_count_;
    QuitMessageLoop();
  }

  int NumActiveDiscoverySessions() {
    int count = 0;
    for (const auto& session : discovery_sessions_) {
      if (session->IsActive())
        count++;
    }
    return count;
  }

  bool IsAdapterDiscovering() {
    BluetoothAdapterBlueZ* adapter_bluez =
        static_cast<BluetoothAdapterBlueZ*>(adapter_.get());
    return adapter_bluez->IsDiscoveringForTesting();
  }

  // Call to fill the adapter_ member with a BluetoothAdapter instance.
  void GetAdapter() {
    adapter_ = BluetoothAdapterBlueZ::CreateAdapter();
    base::RunLoop run_loop;
    adapter_->Initialize(run_loop.QuitClosure());
    run_loop.Run();
    ASSERT_TRUE(adapter_);
    ASSERT_TRUE(adapter_->IsInitialized());
  }

  // Run a discovery phase until the named device is detected, or if the named
  // device is not created, the discovery process ends without finding it.
  //
  // The correct behavior of discovery is tested by the "Discovery" test case
  // without using this function.
  void DiscoverDevice(const std::string& address) {
    ASSERT_TRUE(adapter_.get() != nullptr);
    ASSERT_TRUE(base::MessageLoopCurrent::IsSet());
    fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

    TestBluetoothAdapterObserver observer(adapter_);

    adapter_->SetPowered(true, GetCallback(), GetErrorCallback());
    adapter_->StartDiscoverySession(
        base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallback,
                       base::Unretained(this)),
        GetErrorCallback());
    base::RunLoop().Run();
    ASSERT_EQ(2, callback_count_);
    ASSERT_EQ(0, error_callback_count_);
    ASSERT_EQ((size_t)1, discovery_sessions_.size());
    ASSERT_TRUE(discovery_sessions_[0]->IsActive());
    callback_count_ = 0;

    ASSERT_TRUE(adapter_->IsPowered());
    ASSERT_TRUE(IsAdapterDiscovering());

    while (!observer.device_removed_count() &&
           observer.last_device_address() != address) {
      base::RunLoop().Run();
    }

    discovery_sessions_.clear();
  }

  // Run a discovery phase so we have devices that can be paired with.
  void DiscoverDevices() {
    // Pass an invalid address for the device so that the discovery process
    // completes with all devices.
    DiscoverDevice("does not exist");
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  bluez::FakeBluetoothAdapterClient* fake_bluetooth_adapter_client_;
  bluez::FakeBluetoothDeviceClient* fake_bluetooth_device_client_;
  scoped_refptr<BluetoothAdapter> adapter_;

  int callback_count_;
  int error_callback_count_;
  enum BluetoothDevice::ConnectErrorCode last_connect_error_;
  std::string last_client_error_;
  std::vector<std::unique_ptr<BluetoothDiscoverySession>> discovery_sessions_;
  BluetoothAdapterProfileBlueZ* adapter_profile_;

 private:
  // Some tests use a message loop since background processing is simulated;
  // break out of those loops.
  void QuitMessageLoop() {
    if (base::RunLoop::IsRunningOnCurrentThread())
      base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }
};
const char BluetoothBlueZTest::kGapUuid[] =
    "00001800-0000-1000-8000-00805f9b34fb";
const char BluetoothBlueZTest::kGattUuid[] =
    "00001801-0000-1000-8000-00805f9b34fb";
const char BluetoothBlueZTest::kPnpUuid[] =
    "00001200-0000-1000-8000-00805f9b34fb";
const char BluetoothBlueZTest::kHeadsetUuid[] =
    "00001112-0000-1000-8000-00805f9b34fb";

TEST_F(BluetoothBlueZTest, AlreadyPresent) {
  GetAdapter();

  // This verifies that the class gets the list of adapters when created;
  // and initializes with an existing adapter if there is one.
  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_EQ(bluez::FakeBluetoothAdapterClient::kAdapterAddress,
            adapter_->GetAddress());
  EXPECT_FALSE(IsAdapterDiscovering());

  // There should be 2 devices
  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  EXPECT_EQ(2U, devices.size());

  // |devices| are not ordered, verify it contains the 2 device addresses.
  EXPECT_NE(
      -1, GetDeviceIndexByAddress(
              devices, bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress));
  EXPECT_NE(
      -1,
      GetDeviceIndexByAddress(
          devices,
          bluez::FakeBluetoothDeviceClient::kPairedUnconnectableDeviceAddress));
}

TEST_F(BluetoothBlueZTest, BecomePresent) {
  fake_bluetooth_adapter_client_->SetVisible(false);
  GetAdapter();
  ASSERT_FALSE(adapter_->IsPresent());

  // Install an observer; expect the AdapterPresentChanged to be called
  // with true, and IsPresent() to return true.
  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_adapter_client_->SetVisible(true);

  EXPECT_EQ(1, observer.present_changed_count());
  EXPECT_TRUE(observer.last_present());

  EXPECT_TRUE(adapter_->IsPresent());

  // We should have had a device announced.
  EXPECT_EQ(2, observer.device_added_count());
  EXPECT_EQ(bluez::FakeBluetoothDeviceClient::kPairedUnconnectableDeviceAddress,
            observer.last_device_address());

  // Other callbacks shouldn't be called if the values are false.
  EXPECT_EQ(0, observer.powered_changed_count());
  EXPECT_EQ(0, observer.discovering_changed_count());
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsDiscovering());
}

TEST_F(BluetoothBlueZTest, BecomeNotPresent) {
  GetAdapter();
  ASSERT_TRUE(adapter_->IsPresent());

  // Install an observer; expect the AdapterPresentChanged to be called
  // with false, and IsPresent() to return false.
  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_adapter_client_->SetVisible(false);

  EXPECT_EQ(1, observer.present_changed_count());
  EXPECT_FALSE(observer.last_present());

  EXPECT_FALSE(adapter_->IsPresent());

  // We should have had 2 devices removed.
  EXPECT_EQ(2, observer.device_removed_count());
  // 2 possibilities for the last device here.
  std::string address = observer.last_device_address();
  EXPECT_TRUE(address.compare(bluez::FakeBluetoothDeviceClient::
                                  kPairedUnconnectableDeviceAddress) == 0 ||
              address.compare(
                  bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress) == 0);

  // DiscoveringChanged() should be triggered regardless of the current value
  // of Discovering property.
  EXPECT_EQ(1, observer.discovering_changed_count());

  // Other callbacks shouldn't be called since the values are false.
  EXPECT_EQ(0, observer.powered_changed_count());
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsDiscovering());
}

TEST_F(BluetoothBlueZTest, SecondAdapter) {
  GetAdapter();
  ASSERT_TRUE(adapter_->IsPresent());

  // Install an observer, then add a second adapter. Nothing should change,
  // we ignore the second adapter.
  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_adapter_client_->SetSecondVisible(true);

  EXPECT_EQ(0, observer.present_changed_count());

  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_EQ(bluez::FakeBluetoothAdapterClient::kAdapterAddress,
            adapter_->GetAddress());

  // Try removing the first adapter, we should now act as if the adapter
  // is no longer present rather than fall back to the second.
  fake_bluetooth_adapter_client_->SetVisible(false);

  EXPECT_EQ(1, observer.present_changed_count());
  EXPECT_FALSE(observer.last_present());

  EXPECT_FALSE(adapter_->IsPresent());

  // We should have had 2 devices removed.
  EXPECT_EQ(2, observer.device_removed_count());
  // As BluetoothAdapter devices removal does not keep the order of adding them,
  // 2 possibilities for the last device here.
  std::string address = observer.last_device_address();
  EXPECT_TRUE(address.compare(bluez::FakeBluetoothDeviceClient::
                                  kPairedUnconnectableDeviceAddress) == 0 ||
              address.compare(
                  bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress) == 0);

  // DiscoveringChanged() should be triggered regardless of the current value
  // of Discovering property.
  EXPECT_EQ(1, observer.discovering_changed_count());

  // Other callbacks shouldn't be called since the values are false.
  EXPECT_EQ(0, observer.powered_changed_count());
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_FALSE(IsAdapterDiscovering());

  observer.Reset();

  // Removing the second adapter shouldn't set anything either.
  fake_bluetooth_adapter_client_->SetSecondVisible(false);

  EXPECT_EQ(0, observer.device_removed_count());
  EXPECT_EQ(0, observer.powered_changed_count());
  EXPECT_EQ(0, observer.discovering_changed_count());
}

TEST_F(BluetoothBlueZTest, BecomePowered) {
  GetAdapter();
  ASSERT_FALSE(adapter_->IsPowered());

  // Install an observer; expect the AdapterPoweredChanged to be called
  // with true, and IsPowered() to return true.
  TestBluetoothAdapterObserver observer(adapter_);

  adapter_->SetPowered(true, GetCallback(), GetErrorCallback());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.powered_changed_count());
  EXPECT_TRUE(observer.last_powered());

  EXPECT_TRUE(adapter_->IsPowered());
}

TEST_F(BluetoothBlueZTest, BecomeNotPowered) {
  GetAdapter();
  adapter_->SetPowered(true, GetCallback(), GetErrorCallback());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(adapter_->IsPowered());

  // Install an observer; expect the AdapterPoweredChanged to be called
  // with false, and IsPowered() to return false.
  TestBluetoothAdapterObserver observer(adapter_);

  adapter_->SetPowered(false, GetCallback(), GetErrorCallback());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.powered_changed_count());
  EXPECT_FALSE(observer.last_powered());

  EXPECT_FALSE(adapter_->IsPowered());
}

TEST_F(BluetoothBlueZTest, SetPoweredWhenNotPresent) {
  GetAdapter();
  ASSERT_TRUE(adapter_->IsPresent());

  // Install an observer; expect the AdapterPresentChanged to be called
  // with false, and IsPresent() to return false.
  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_adapter_client_->SetVisible(false);

  EXPECT_EQ(1, observer.present_changed_count());
  EXPECT_FALSE(observer.last_present());

  EXPECT_FALSE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->IsPowered());

  adapter_->SetPowered(true, GetCallback(), GetErrorCallback());
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);

  EXPECT_EQ(0, observer.powered_changed_count());
  EXPECT_FALSE(observer.last_powered());

  EXPECT_FALSE(adapter_->IsPowered());
}

TEST_F(BluetoothBlueZTest, ChangeAdapterName) {
  GetAdapter();

  const std::string new_name(".__.");

  adapter_->SetName(new_name, GetCallback(), GetErrorCallback());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(new_name, adapter_->GetName());
}

TEST_F(BluetoothBlueZTest, ChangeAdapterNameWhenNotPresent) {
  GetAdapter();
  ASSERT_TRUE(adapter_->IsPresent());

  // Install an observer; expect the AdapterPresentChanged to be called
  // with false, and IsPresent() to return false.
  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_adapter_client_->SetVisible(false);

  EXPECT_EQ(1, observer.present_changed_count());
  EXPECT_FALSE(observer.last_present());

  EXPECT_FALSE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->IsPowered());

  adapter_->SetName("^o^", GetCallback(), GetErrorCallback());
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);

  EXPECT_EQ("", adapter_->GetName());
}

TEST_F(BluetoothBlueZTest, GetUUIDs) {
  std::vector<std::string> adapterUuids;
  GetAdapter();

  adapterUuids.push_back(kGapUuid);
  adapterUuids.push_back(kGattUuid);
  adapterUuids.push_back(kPnpUuid);
  adapterUuids.push_back(kHeadsetUuid);

  fake_bluetooth_adapter_client_->SetUUIDs(adapterUuids);

  BluetoothAdapter::UUIDList uuids = adapter_->GetUUIDs();

  ASSERT_EQ(4U, uuids.size());
  // Check that the UUIDs match those from above - in order, GAP, GATT, PnP, and
  // headset.
  EXPECT_EQ(uuids[0], BluetoothUUID("1800"));
  EXPECT_EQ(uuids[1], BluetoothUUID("1801"));
  EXPECT_EQ(uuids[2], BluetoothUUID("1200"));
  EXPECT_EQ(uuids[3], BluetoothUUID("1112"));
}

TEST_F(BluetoothBlueZTest, BecomeDiscoverable) {
  GetAdapter();
  ASSERT_FALSE(adapter_->IsDiscoverable());

  // Install an observer; expect the AdapterDiscoverableChanged to be called
  // with true, and IsDiscoverable() to return true.
  TestBluetoothAdapterObserver observer(adapter_);

  adapter_->SetDiscoverable(true, GetCallback(), GetErrorCallback());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.discoverable_changed_count());

  EXPECT_TRUE(adapter_->IsDiscoverable());
}

TEST_F(BluetoothBlueZTest, BecomeNotDiscoverable) {
  GetAdapter();
  adapter_->SetDiscoverable(true, GetCallback(), GetErrorCallback());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(adapter_->IsDiscoverable());

  // Install an observer; expect the AdapterDiscoverableChanged to be called
  // with false, and IsDiscoverable() to return false.
  TestBluetoothAdapterObserver observer(adapter_);

  adapter_->SetDiscoverable(false, GetCallback(), GetErrorCallback());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.discoverable_changed_count());

  EXPECT_FALSE(adapter_->IsDiscoverable());
}

TEST_F(BluetoothBlueZTest, SetDiscoverableWhenNotPresent) {
  GetAdapter();
  ASSERT_TRUE(adapter_->IsPresent());
  ASSERT_FALSE(adapter_->IsDiscoverable());

  // Install an observer; expect the AdapterDiscoverableChanged to be called
  // with true, and IsDiscoverable() to return true.
  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_adapter_client_->SetVisible(false);

  EXPECT_EQ(1, observer.present_changed_count());
  EXPECT_FALSE(observer.last_present());

  EXPECT_FALSE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->IsDiscoverable());

  adapter_->SetDiscoverable(true, GetCallback(), GetErrorCallback());
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);

  EXPECT_EQ(0, observer.discoverable_changed_count());

  EXPECT_FALSE(adapter_->IsDiscoverable());
}

TEST_F(BluetoothBlueZTest, StopDiscovery) {
  GetAdapter();

  adapter_->SetPowered(true, GetCallback(), GetErrorCallback());
  adapter_->StartDiscoverySession(
      base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallback,
                     base::Unretained(this)),
      GetErrorCallback());
  base::RunLoop().Run();
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  ASSERT_TRUE(discovery_sessions_[0]->IsActive());

  base::RunLoop stop_loop;
  // Install an observer; aside from the callback, expect the
  // AdapterDiscoveringChanged method to be called and no longer to be
  // discovering,
  TestBluetoothAdapterObserver observer(adapter_);
  observer.RegisterDiscoveringChangedWatcher(stop_loop.QuitClosure());
  discovery_sessions_.clear();
  stop_loop.Run();
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());

  EXPECT_FALSE(IsAdapterDiscovering());
}

TEST_F(BluetoothBlueZTest, Discovery) {
  // Test a simulated discovery session.
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);
  GetAdapter();

  TestBluetoothAdapterObserver observer(adapter_);

  adapter_->SetPowered(true, GetCallback(), GetErrorCallback());
  adapter_->StartDiscoverySession(
      base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallback,
                     base::Unretained(this)),
      GetErrorCallback());
  base::RunLoop().Run();
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  ASSERT_TRUE(discovery_sessions_[0]->IsActive());

  // First two devices to appear.
  base::RunLoop().Run();

  EXPECT_EQ(2, observer.device_added_count());
  EXPECT_EQ(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress,
            observer.last_device_address());

  // Next we should get another two devices...
  base::RunLoop().Run();
  EXPECT_EQ(4, observer.device_added_count());

  // Okay, let's run forward until a device is actually removed...
  while (!observer.device_removed_count())
    base::RunLoop().Run();

  EXPECT_EQ(1, observer.device_removed_count());
  EXPECT_EQ(bluez::FakeBluetoothDeviceClient::kVanishingDeviceAddress,
            observer.last_device_address());
}

TEST_F(BluetoothBlueZTest, PoweredAndDiscovering) {
  GetAdapter();
  adapter_->SetPowered(true, GetCallback(), GetErrorCallback());
  adapter_->StartDiscoverySession(
      base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallback,
                     base::Unretained(this)),
      GetErrorCallback());
  base::RunLoop().Run();
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  ASSERT_TRUE(discovery_sessions_[0]->IsActive());

  // Stop the timers that the simulation uses
  fake_bluetooth_device_client_->EndDiscoverySimulation(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath));

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(IsAdapterDiscovering());

  fake_bluetooth_adapter_client_->SetVisible(false);
  ASSERT_FALSE(adapter_->IsPresent());
  ASSERT_FALSE(discovery_sessions_[0]->IsActive());

  // Install an observer; expect the AdapterPresentChanged,
  // AdapterPoweredChanged and AdapterDiscoveringChanged methods to be called
  // with true, and IsPresent(), IsPowered() and IsDiscoveringForTesting() to
  // all return true.
  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_adapter_client_->SetVisible(true);

  EXPECT_EQ(1, observer.present_changed_count());
  EXPECT_TRUE(observer.last_present());
  EXPECT_TRUE(adapter_->IsPresent());

  EXPECT_EQ(1, observer.powered_changed_count());
  EXPECT_TRUE(observer.last_powered());
  EXPECT_TRUE(adapter_->IsPowered());

  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());

  observer.Reset();

  // Now mark the adapter not present again. Expect the methods to be called
  // again, to reset the properties back to false.
  fake_bluetooth_adapter_client_->SetVisible(false);

  EXPECT_EQ(1, observer.present_changed_count());
  EXPECT_FALSE(observer.last_present());
  EXPECT_FALSE(adapter_->IsPresent());

  EXPECT_EQ(1, observer.powered_changed_count());
  EXPECT_FALSE(observer.last_powered());
  EXPECT_FALSE(adapter_->IsPowered());

  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(IsAdapterDiscovering());
}

// This unit test asserts that a DiscoverySession which is queued to start does
// not get interrupted by a stop discovery call being executed.
TEST_F(BluetoothBlueZTest, StopAndStartDiscoverySimultaneously) {
  GetAdapter();
  adapter_->SetPowered(/*powered=*/true, GetCallback(), GetErrorCallback());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(adapter_->IsPowered());
  callback_count_ = 0;

  TestBluetoothAdapterObserver observer(adapter_);

  EXPECT_EQ(0, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(IsAdapterDiscovering());

  // Start Discovery in order to call Stop.
  base::RunLoop start_loop_1;
  adapter_->StartDiscoverySession(
      base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallbackWithClosure,
                     base::Unretained(this), start_loop_1.QuitClosure()),
      GetErrorCallback());
  // Run the callbacks to set up state for new requests.
  {
    base::RunLoop discovering_changed_loop;
    observer.RegisterDiscoveringChangedWatcher(
        discovering_changed_loop.QuitClosure());
    discovering_changed_loop.Run();
  }
  start_loop_1.Run();

  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ(1u, discovery_sessions_.size());


  // Register loop to watch for Discovery changes.
  base::RunLoop discovering_changed_loop;
  int discovery_changed_count = 0;
  observer.RegisterDiscoveringChangedWatcher(base::BindLambdaForTesting([&]() {
    ++discovery_changed_count;
    if (discovery_changed_count == 1) {
      EXPECT_FALSE(observer.last_discovering());
      EXPECT_EQ(2, observer.discovering_changed_count());
      EXPECT_FALSE(IsAdapterDiscovering());
    }

    if (discovery_changed_count == 2) {
      EXPECT_TRUE(observer.last_discovering());
      discovering_changed_loop.Quit();
    }
  }));

  discovery_sessions_.clear();
  EXPECT_EQ(1, observer.discovering_changed_count());

  // Queue up start to ensure all still works properly with a
  // StartDiscoverySession pending.
  base::RunLoop start_loop_2;
  adapter_->StartDiscoverySession(
      base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallbackWithClosure,
                     base::Unretained(this), start_loop_2.QuitClosure()),
      GetErrorCallback());

  // Run loop waiting for DiscoveryChanged to be called in the observer
  // twice(once from stop and once from start).
  discovering_changed_loop.Run();

  // Finish start call.
  start_loop_2.Run();

  EXPECT_EQ(3, observer.discovering_changed_count());
  EXPECT_EQ(2, callback_count_);
  EXPECT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ(1, NumActiveDiscoverySessions());
}

// This unit test asserts that the basic reference counting logic works
// correctly for discovery requests done via the BluetoothAdapter.
TEST_F(BluetoothBlueZTest, MultipleDiscoverySessions) {
  GetAdapter();
  adapter_->SetPowered(true, GetCallback(), GetErrorCallback());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(adapter_->IsPowered());
  callback_count_ = 0;

  TestBluetoothAdapterObserver observer(adapter_);

  EXPECT_EQ(0, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(IsAdapterDiscovering());

  // Request device discovery 3 times.
  for (int i = 0; i < 3; i++) {
    adapter_->StartDiscoverySession(
        base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallback,
                       base::Unretained(this)),
        GetErrorCallback());
  }

  // Run the callbacks to set up state for new requests.
  base::RunLoop().Run();

  // The observer should have received the discovering changed event exactly
  // once, the success callback should have been called 3 times and the adapter
  // should be discovering.
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_EQ(3, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ((size_t)3, discovery_sessions_.size());

  // Request to stop discovery twice.
  for (int i = 0; i < 2; i++) {
    discovery_sessions_.erase(discovery_sessions_.begin());
  }
  // The observer should have received no additional discovering changed events,
  // and the adapter should still be discovering.
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());

  // Request device discovery 3 times.
  for (int i = 0; i < 3; i++) {
    adapter_->StartDiscoverySession(
        base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallback,
                       base::Unretained(this)),
        GetErrorCallback());
  }

  // The observer should have received no additional discovering changed events,
  // the adapter should still be discovering.
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_EQ(6, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ(4u, discovery_sessions_.size());

  // Request to stop discovery 4 times.
  base::RunLoop stop_loop;
  observer.RegisterDiscoveringChangedWatcher(stop_loop.QuitClosure());

  for (int i = 0; i < 4; i++) {
    discovery_sessions_.erase(discovery_sessions_.begin());
  }
  stop_loop.Run();
  // The observer should have received the discovering changed event exactly
  // once, the adapter should no longer be discovering.
  EXPECT_EQ(2, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(IsAdapterDiscovering());
  EXPECT_TRUE(discovery_sessions_.empty());
}

// This unit test asserts that the reference counting logic works correctly in
// the cases when the adapter gets reset and D-Bus calls are made outside of
// the BluetoothAdapter.
TEST_F(BluetoothBlueZTest, UnexpectedChangesDuringMultipleDiscoverySessions) {
  GetAdapter();
  adapter_->SetPowered(true, GetCallback(), GetErrorCallback());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(adapter_->IsPowered());
  callback_count_ = 0;

  TestBluetoothAdapterObserver observer(adapter_);

  EXPECT_EQ(0, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(IsAdapterDiscovering());

  // Request device discovery 3 times.
  for (int i = 0; i < 3; i++) {
    adapter_->StartDiscoverySession(
        base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallback,
                       base::Unretained(this)),
        GetErrorCallback());
  }

  // Run the callbacks to set up state for new requests.
  base::RunLoop().Run();
  // The observer should have received the discovering changed event exactly
  // once, the success callback should have been called 3 times and the adapter
  // should be discovering.
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_EQ(3, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ((size_t)3, discovery_sessions_.size());

  for (int i = 0; i < 3; i++)
    EXPECT_TRUE(discovery_sessions_[i]->IsActive());

  // Stop the timers that the simulation uses
  fake_bluetooth_device_client_->EndDiscoverySimulation(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath));

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(IsAdapterDiscovering());

  // Stop device discovery behind the adapter. The adapter and the observer
  // should be notified of the change and the reference count should be reset.
  // Even though bluez::FakeBluetoothAdapterClient does its own reference
  // counting and
  // we called 3 BluetoothAdapter::StartDiscoverySession 3 times, the
  // bluez::FakeBluetoothAdapterClient's count should be only 1 and a single
  // call to
  // bluez::FakeBluetoothAdapterClient::StopDiscovery should work.
  fake_bluetooth_adapter_client_->BluetoothAdapterClient::StopDiscovery(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      GetCallback(),
      base::BindOnce(&BluetoothBlueZTest::DBusErrorCallback,
                     base::Unretained(this)));
  base::RunLoop().Run();
  EXPECT_EQ(2, observer.discovering_changed_count());
  EXPECT_EQ(4, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(IsAdapterDiscovering());

  // All discovery session instances should have been updated.
  for (int i = 0; i < 3; i++)
    EXPECT_FALSE(discovery_sessions_[i]->IsActive());
  discovery_sessions_.clear();

  // It should be possible to successfully start discovery.
  for (int i = 0; i < 2; i++) {
    adapter_->StartDiscoverySession(
        base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallback,
                       base::Unretained(this)),
        GetErrorCallback());
  }

  base::RunLoop().Run();
  EXPECT_EQ(3, observer.discovering_changed_count());
  EXPECT_EQ(6, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ((size_t)2, discovery_sessions_.size());

  for (int i = 0; i < 2; i++)
    EXPECT_TRUE(discovery_sessions_[i]->IsActive());

  fake_bluetooth_device_client_->EndDiscoverySimulation(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath));

  // Make the adapter disappear and appear. This will make it come back as
  // discovering. When this happens, the reference count should become and
  // remain 0 as no new request was made through the BluetoothAdapter.
  fake_bluetooth_adapter_client_->SetVisible(false);
  ASSERT_FALSE(adapter_->IsPresent());
  EXPECT_EQ(4, observer.discovering_changed_count());
  EXPECT_EQ(6, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(IsAdapterDiscovering());

  for (int i = 0; i < 2; i++)
    EXPECT_FALSE(discovery_sessions_[i]->IsActive());
  discovery_sessions_.clear();

  fake_bluetooth_adapter_client_->SetVisible(true);
  ASSERT_TRUE(adapter_->IsPresent());
  EXPECT_EQ(5, observer.discovering_changed_count());
  EXPECT_EQ(6, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());

  // Start and stop discovery. At this point, bluez::FakeBluetoothAdapterClient
  // has
  // a reference count that is equal to 1. Pretend that this was done by an
  // application other than us. Starting and stopping discovery will succeed
  // but it won't cause the discovery state to change.
  adapter_->StartDiscoverySession(
      base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallback,
                     base::Unretained(this)),
      GetErrorCallback());
  // Run the loop, as there should have been a D-Bus call.
  base::RunLoop().Run();
  EXPECT_EQ(5, observer.discovering_changed_count());
  EXPECT_EQ(7, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());

  discovery_sessions_.clear();
  EXPECT_EQ(5, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());
  EXPECT_TRUE(discovery_sessions_.empty());

  // Start discovery again.
  adapter_->StartDiscoverySession(
      base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallback,
                     base::Unretained(this)),
      GetErrorCallback());
  // Run the loop, as there should have been a D-Bus call.
  base::RunLoop().Run();
  EXPECT_EQ(5, observer.discovering_changed_count());
  EXPECT_EQ(8, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());

  // Stop discovery via D-Bus. The fake client's reference count will drop but
  // the discovery state won't change since our BluetoothAdapter also just
  // requested it via D-Bus.
  fake_bluetooth_adapter_client_->BluetoothAdapterClient::StopDiscovery(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      GetCallback(),
      base::BindOnce(&BluetoothBlueZTest::DBusErrorCallback,
                     base::Unretained(this)));
  base::RunLoop().Run();
  EXPECT_EQ(5, observer.discovering_changed_count());
  EXPECT_EQ(9, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());

  // Now end the discovery session. This should change the adapter's discovery
  // state.
  base::RunLoop stop_loop;
  observer.RegisterDiscoveringChangedWatcher(stop_loop.QuitClosure());
  discovery_sessions_.clear();
  stop_loop.Run();
  EXPECT_EQ(6, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(IsAdapterDiscovering());
  EXPECT_TRUE(discovery_sessions_.empty());
}

TEST_F(BluetoothBlueZTest, InvalidatedDiscoverySessions) {
  GetAdapter();
  adapter_->SetPowered(true, GetCallback(), GetErrorCallback());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(adapter_->IsPowered());
  callback_count_ = 0;

  TestBluetoothAdapterObserver observer(adapter_);

  EXPECT_EQ(0, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(IsAdapterDiscovering());

  // Request device discovery 3 times.
  for (int i = 0; i < 3; i++) {
    adapter_->StartDiscoverySession(
        base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallback,
                       base::Unretained(this)),
        GetErrorCallback());
    // Run loop every time as we are calling dbus on every session start.
    base::RunLoop().Run();
  }

  // The observer should have received the discovering changed event exactly
  // once, the success callback should have been called 3 times and the adapter
  // should be discovering.
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_EQ(3, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ((size_t)3, discovery_sessions_.size());

  for (int i = 0; i < 3; i++)
    EXPECT_TRUE(discovery_sessions_[i]->IsActive());

  // Stop the timers that the simulation uses
  fake_bluetooth_device_client_->EndDiscoverySimulation(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath));

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(IsAdapterDiscovering());

  // Delete all but one discovery session.
  discovery_sessions_.pop_back();
  discovery_sessions_.pop_back();
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());
  EXPECT_TRUE(IsAdapterDiscovering());

  // Stop device discovery behind the adapter. The one active discovery session
  // should become inactive, but more importantly, we shouldn't run into any
  // memory errors as the sessions that we explicitly deleted should get
  // cleaned up.
  fake_bluetooth_adapter_client_->BluetoothAdapterClient::StopDiscovery(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      GetCallback(),
      base::BindOnce(&BluetoothBlueZTest::DBusErrorCallback,
                     base::Unretained(this)));
  base::RunLoop().Run();
  EXPECT_EQ(2, observer.discovering_changed_count());
  EXPECT_EQ(4, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(IsAdapterDiscovering());
  EXPECT_FALSE(discovery_sessions_[0]->IsActive());
}

TEST_F(BluetoothBlueZTest, StartDiscoverySession) {
  GetAdapter();

  adapter_->SetPowered(true, GetCallback(), GetErrorCallback());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(adapter_->IsPowered());
  callback_count_ = 0;

  TestBluetoothAdapterObserver observer(adapter_);

  EXPECT_EQ(0, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(IsAdapterDiscovering());
  EXPECT_TRUE(discovery_sessions_.empty());

  // Request a new discovery session.
  adapter_->StartDiscoverySession(
      base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallback,
                     base::Unretained(this)),
      GetErrorCallback());
  base::RunLoop().Run();
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());

  // Start another session. A new one should be returned in the callback, which
  // in turn will destroy the previous session. Adapter should still be
  // discovering and the reference count should be 1.
  adapter_->StartDiscoverySession(
      base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallback,
                     base::Unretained(this)),
      GetErrorCallback());
  base::RunLoop().Run();
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ((size_t)2, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());

  // Request a new session.
  adapter_->StartDiscoverySession(
      base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallback,
                     base::Unretained(this)),
      GetErrorCallback());
  base::RunLoop().Run();
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_EQ(3, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ(3u, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[1]->IsActive());
  EXPECT_NE(discovery_sessions_[0], discovery_sessions_[1]);

  // Stop the previous discovery session. The session should end but discovery
  // should continue.
  discovery_sessions_.erase(discovery_sessions_.begin());
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ(2u, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());

  // Delete the current active session.
  base::RunLoop stop_loop;
  observer.RegisterDiscoveringChangedWatcher(stop_loop.QuitClosure());
  discovery_sessions_.clear();
  stop_loop.Run();

  EXPECT_EQ(2, observer.discovering_changed_count());
  EXPECT_EQ(3, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(IsAdapterDiscovering());
}

TEST_F(BluetoothBlueZTest, SetDiscoveryFilterBeforeStartDiscovery) {
  // Test a simulated discovery session.
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);
  GetAdapter();

  TestBluetoothAdapterObserver observer(adapter_);

  auto discovery_filter = std::make_unique<BluetoothDiscoveryFilter>(
      device::BLUETOOTH_TRANSPORT_LE);
  discovery_filter->SetRSSI(-60);
  device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
  device_filter.uuids.insert(BluetoothUUID("1000"));
  discovery_filter->AddDeviceFilter(std::move(device_filter));

  adapter_->SetPowered(
      true, base::Bind(&BluetoothBlueZTest::Callback, base::Unretained(this)),
      base::Bind(&BluetoothBlueZTest::ErrorCallback, base::Unretained(this)));

  auto* comparison_filter_holder = discovery_filter.get();
  adapter_->StartDiscoverySessionWithFilter(
      std::move(discovery_filter),
      base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallback,
                     base::Unretained(this)),
      base::BindOnce(&BluetoothBlueZTest::ErrorCallback,
                     base::Unretained(this)));
  base::RunLoop().Run();
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  ASSERT_TRUE(discovery_sessions_[0]->IsActive());
  ASSERT_TRUE(comparison_filter_holder->Equals(
      *discovery_sessions_[0]->GetDiscoveryFilter()));

  auto* filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_NE(nullptr, filter);
  EXPECT_EQ("le", *filter->transport);
  EXPECT_EQ(-60, *filter->rssi);
  EXPECT_EQ(nullptr, filter->pathloss.get());
  std::vector<std::string> uuids = *filter->uuids;
  EXPECT_TRUE(base::Contains(uuids, "1000"));

  base::RunLoop stop_loop;
  observer.RegisterDiscoveringChangedWatcher(stop_loop.QuitClosure());
  discovery_sessions_.clear();
  stop_loop.Run();

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_FALSE(IsAdapterDiscovering());
  ASSERT_TRUE(discovery_sessions_.empty());

  filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_EQ(nullptr, filter);
}

TEST_F(BluetoothBlueZTest, SetDiscoveryFilterBeforeStartDiscoveryFail) {
  // Test a simulated discovery session.
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);
  GetAdapter();

  TestBluetoothAdapterObserver observer(adapter_);

  auto discovery_filter = std::make_unique<BluetoothDiscoveryFilter>(
      device::BLUETOOTH_TRANSPORT_LE);
  discovery_filter->SetRSSI(-60);
  device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
  device_filter.uuids.insert(BluetoothUUID("1000"));
  discovery_filter->AddDeviceFilter(std::move(device_filter));

  adapter_->SetPowered(
      true, base::Bind(&BluetoothBlueZTest::Callback, base::Unretained(this)),
      base::Bind(&BluetoothBlueZTest::ErrorCallback, base::Unretained(this)));
  EXPECT_EQ(1, callback_count_);
  callback_count_ = 0;

  fake_bluetooth_adapter_client_->MakeSetDiscoveryFilterFail();

  adapter_->StartDiscoverySessionWithFilter(
      std::move(discovery_filter),
      base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallback,
                     base::Unretained(this)),
      base::BindOnce(&BluetoothBlueZTest::ErrorCallback,
                     base::Unretained(this)));

  base::RunLoop().Run();

  EXPECT_EQ(1, error_callback_count_);
  error_callback_count_ = 0;

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_FALSE(IsAdapterDiscovering());
  ASSERT_EQ((size_t)0, discovery_sessions_.size());

  auto* filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_EQ(nullptr, filter);
}

// This unit test asserts that the basic reference counting, and filter merging
// works correctly for discovery requests done via the BluetoothAdapter.
TEST_F(BluetoothBlueZTest, SetDiscoveryFilterBeforeStartDiscoveryMultiple) {
  GetAdapter();
  adapter_->SetPowered(
      true, base::Bind(&BluetoothBlueZTest::Callback, base::Unretained(this)),
      base::Bind(&BluetoothBlueZTest::ErrorCallback, base::Unretained(this)));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(adapter_->IsPowered());
  callback_count_ = 0;

  TestBluetoothAdapterObserver observer(adapter_);

  // Request device discovery with pre-set filter 3 times.
  for (int i = 0; i < 3; i++) {
    std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter;
    if (i == 0) {
      discovery_filter = std::make_unique<BluetoothDiscoveryFilter>(
          device::BLUETOOTH_TRANSPORT_LE);
      discovery_filter->SetRSSI(-85);
      device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
      device_filter.uuids.insert(BluetoothUUID("1000"));
      discovery_filter->AddDeviceFilter(std::move(device_filter));
    } else if (i == 1) {
      discovery_filter = std::make_unique<BluetoothDiscoveryFilter>(
          device::BLUETOOTH_TRANSPORT_LE);
      discovery_filter->SetRSSI(-60);
      device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
      device_filter.uuids.insert(BluetoothUUID("1020"));
      device_filter.uuids.insert(BluetoothUUID("1001"));
      discovery_filter->AddDeviceFilter(std::move(device_filter));
    } else if (i == 2) {
      discovery_filter = std::make_unique<BluetoothDiscoveryFilter>(
          device::BLUETOOTH_TRANSPORT_LE);
      discovery_filter->SetRSSI(-65);
      device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
      device_filter.uuids.insert(BluetoothUUID("1020"));
      device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter2;
      device_filter2.uuids.insert(BluetoothUUID("1003"));
      discovery_filter->AddDeviceFilter(std::move(device_filter));
      discovery_filter->AddDeviceFilter(std::move(device_filter2));
    }

    adapter_->StartDiscoverySessionWithFilter(
        std::move(discovery_filter),
        base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallback,
                       base::Unretained(this)),
        base::BindOnce(&BluetoothBlueZTest::ErrorCallback,
                       base::Unretained(this)));

    base::RunLoop().Run();

    if (i == 0) {
      EXPECT_EQ(1, observer.discovering_changed_count());
      observer.Reset();

      auto* filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
      EXPECT_EQ("le", *filter->transport);
      EXPECT_EQ(-85, *filter->rssi);
      EXPECT_EQ(nullptr, filter->pathloss.get());
      std::vector<std::string> uuids = *filter->uuids;
      EXPECT_TRUE(base::Contains(uuids, "1000"));
    } else if (i == 1) {
      auto* filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
      EXPECT_EQ("le", *filter->transport);
      EXPECT_EQ(-85, *filter->rssi);
      EXPECT_EQ(nullptr, filter->pathloss.get());
      std::vector<std::string> uuids = *filter->uuids;
      EXPECT_TRUE(base::Contains(uuids, "1000"));
      EXPECT_TRUE(base::Contains(uuids, "1001"));
      EXPECT_TRUE(base::Contains(uuids, "1020"));
    } else if (i == 2) {
      auto* filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
      EXPECT_EQ("le", *filter->transport);
      EXPECT_EQ(-85, *filter->rssi);
      EXPECT_EQ(nullptr, filter->pathloss.get());
      std::vector<std::string> uuids = *filter->uuids;
      EXPECT_TRUE(base::Contains(uuids, "1000"));
      EXPECT_TRUE(base::Contains(uuids, "1001"));
      EXPECT_TRUE(base::Contains(uuids, "1003"));
      EXPECT_TRUE(base::Contains(uuids, "1020"));
    }
  }

  // the success callback should have been called 3 times and the adapter should
  // be discovering.
  EXPECT_EQ(3, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ((size_t)3, discovery_sessions_.size());

  callback_count_ = 0;
  // Request to stop discovery twice.
  for (int i = 0; i < 2; i++) {
    base::RunLoop change_loop;
    observer.RegisterDiscoveryChangeCompletedWatcher(change_loop.QuitClosure());
    discovery_sessions_.erase(discovery_sessions_.begin());
    change_loop.Run();
    if (i == 0) {
      auto* filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
      EXPECT_EQ("le", *filter->transport);
      EXPECT_EQ(-65, *filter->rssi);
      EXPECT_EQ(nullptr, filter->pathloss.get());
      std::vector<std::string> uuids = *filter->uuids;
      EXPECT_EQ(3UL, uuids.size());
      EXPECT_FALSE(base::Contains(uuids, "1000"));
      EXPECT_TRUE(base::Contains(uuids, "1001"));
      EXPECT_TRUE(base::Contains(uuids, "1003"));
      EXPECT_TRUE(base::Contains(uuids, "1020"));
    } else if (i == 1) {
      auto* filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
      EXPECT_EQ("le", *filter->transport);
      EXPECT_EQ(-65, *filter->rssi);
      EXPECT_EQ(nullptr, filter->pathloss.get());
      std::vector<std::string> uuids = *filter->uuids;
      EXPECT_EQ(2UL, uuids.size());
      EXPECT_FALSE(base::Contains(uuids, "1000"));
      EXPECT_FALSE(base::Contains(uuids, "1001"));
      EXPECT_TRUE(base::Contains(uuids, "1003"));
      EXPECT_TRUE(base::Contains(uuids, "1020"));
    } else if (i == 2) {
      auto* filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
      EXPECT_EQ("le", *filter->transport);
      EXPECT_EQ(-65, *filter->rssi);
      EXPECT_EQ(nullptr, filter->pathloss.get());
      std::vector<std::string> uuids = *filter->uuids;
      EXPECT_EQ(0UL, uuids.size());
    }
  }

  // The success callback should have been called 2 times and the adapter should
  // still be discovering.
  EXPECT_TRUE(IsAdapterDiscovering());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());
  ASSERT_EQ(1u, discovery_sessions_.size());

  callback_count_ = 0;

  // Request device discovery 3 times.
  for (int i = 0; i < 3; i++) {
    std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter;

    if (i == 0) {
      discovery_filter = std::make_unique<BluetoothDiscoveryFilter>(
          device::BLUETOOTH_TRANSPORT_LE);
      discovery_filter->SetRSSI(-85);
      device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
      device_filter.uuids.insert(BluetoothUUID("1000"));
      discovery_filter->AddDeviceFilter(std::move(device_filter));
    } else if (i == 1) {
      discovery_filter = std::make_unique<BluetoothDiscoveryFilter>(
          device::BLUETOOTH_TRANSPORT_LE);
      discovery_filter->SetRSSI(-60);
      device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
      device_filter.uuids.insert(BluetoothUUID("1020"));
      device_filter.uuids.insert(BluetoothUUID("1001"));
      discovery_filter->AddDeviceFilter(std::move(device_filter));
    } else if (i == 2) {
      discovery_filter = std::make_unique<BluetoothDiscoveryFilter>(
          device::BLUETOOTH_TRANSPORT_LE);
      discovery_filter->SetRSSI(-65);
      device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
      device_filter.uuids.insert(BluetoothUUID("1020"));
      device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter2;
      device_filter2.uuids.insert(BluetoothUUID("1003"));
      discovery_filter->AddDeviceFilter(std::move(device_filter));
      discovery_filter->AddDeviceFilter(std::move(device_filter2));
    }

    adapter_->StartDiscoverySessionWithFilter(
        std::move(discovery_filter),
        base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallback,
                       base::Unretained(this)),
        base::BindOnce(&BluetoothBlueZTest::ErrorCallback,
                       base::Unretained(this)));

    base::RunLoop().Run();

    if (i == 0) {
      auto* filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
      EXPECT_EQ("le", *filter->transport);
      EXPECT_EQ(-85, *filter->rssi);
      EXPECT_EQ(nullptr, filter->pathloss.get());
      std::vector<std::string> uuids = *filter->uuids;
      EXPECT_TRUE(base::Contains(uuids, "1000"));
      EXPECT_TRUE(base::Contains(uuids, "1003"));
      EXPECT_TRUE(base::Contains(uuids, "1020"));
    } else if (i == 1 || i == 2) {
      auto* filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
      EXPECT_EQ("le", *filter->transport);
      EXPECT_EQ(-85, *filter->rssi);
      EXPECT_EQ(nullptr, filter->pathloss.get());
      std::vector<std::string> uuids = *filter->uuids;
      EXPECT_TRUE(base::Contains(uuids, "1000"));
      EXPECT_TRUE(base::Contains(uuids, "1001"));
      EXPECT_TRUE(base::Contains(uuids, "1003"));
      EXPECT_TRUE(base::Contains(uuids, "1020"));
    }
  }

  // The success callback should have been called 3 times and the adapter should
  // still be discovering.
  EXPECT_EQ(3, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(IsAdapterDiscovering());
  ASSERT_EQ(4u, discovery_sessions_.size());

  callback_count_ = 0;
  // Request to stop discovery 4 times.
  base::RunLoop adapter_stop_loop;
  observer.RegisterDiscoveringChangedWatcher(adapter_stop_loop.QuitClosure());
  for (int i = 2; i < 6; i++) {
    base::RunLoop change_loop;
    observer.RegisterDiscoveryChangeCompletedWatcher(change_loop.QuitClosure());
    discovery_sessions_.erase(discovery_sessions_.begin());
    change_loop.Run();
  }
  adapter_stop_loop.Run();

  // The adapter should no longer be discovering.
  EXPECT_FALSE(IsAdapterDiscovering());
  EXPECT_EQ(1, observer.discovering_changed_count());

  // All discovery sessions should be inactive.
  ASSERT_TRUE(discovery_sessions_.empty());
  auto* filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_EQ(nullptr, filter);
}

// This unit test asserts that filter merging logic works correctly for filtered
// discovery requests done via the BluetoothAdapter.
TEST_F(BluetoothBlueZTest, SetDiscoveryFilterMergingTest) {
  GetAdapter();
  adapter_->SetPowered(
      true, base::Bind(&BluetoothBlueZTest::Callback, base::Unretained(this)),
      base::Bind(&BluetoothBlueZTest::ErrorCallback, base::Unretained(this)));

  BluetoothDiscoveryFilter* df =
      new BluetoothDiscoveryFilter(device::BLUETOOTH_TRANSPORT_LE);
  df->SetRSSI(-15);
  device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
  device_filter.uuids.insert(BluetoothUUID("1000"));
  df->AddDeviceFilter(std::move(device_filter));
  std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter(df);

  adapter_->StartDiscoverySessionWithFilter(
      std::move(discovery_filter),
      base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallback,
                     base::Unretained(this)),
      base::BindOnce(&BluetoothBlueZTest::ErrorCallback,
                     base::Unretained(this)));

  base::RunLoop().Run();

  auto* filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_EQ("le", *filter->transport);
  EXPECT_EQ(-15, *filter->rssi);
  EXPECT_EQ(nullptr, filter->pathloss.get());
  std::vector<std::string> uuids = *filter->uuids;
  EXPECT_TRUE(base::Contains(uuids, "1000"));

  df = new BluetoothDiscoveryFilter(device::BLUETOOTH_TRANSPORT_LE);
  df->SetRSSI(-60);
  device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter2;
  device_filter2.uuids.insert(BluetoothUUID("1020"));
  device_filter2.uuids.insert(BluetoothUUID("1001"));
  df->AddDeviceFilter(device_filter2);
  discovery_filter = std::unique_ptr<BluetoothDiscoveryFilter>(df);

  adapter_->StartDiscoverySessionWithFilter(
      std::move(discovery_filter),
      base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallback,
                     base::Unretained(this)),
      base::BindOnce(&BluetoothBlueZTest::ErrorCallback,
                     base::Unretained(this)));

  base::RunLoop().Run();

  filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_EQ("le", *filter->transport);
  EXPECT_EQ(-60, *filter->rssi);
  EXPECT_EQ(nullptr, filter->pathloss.get());
  uuids = *filter->uuids;
  EXPECT_TRUE(base::Contains(uuids, "1000"));
  EXPECT_TRUE(base::Contains(uuids, "1001"));
  EXPECT_TRUE(base::Contains(uuids, "1020"));

  BluetoothDiscoveryFilter* df3 =
      new BluetoothDiscoveryFilter(device::BLUETOOTH_TRANSPORT_CLASSIC);
  df3->SetRSSI(-65);
  device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter3;
  device_filter3.uuids.insert(BluetoothUUID("1020"));
  device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter4;
  device_filter4.uuids.insert(BluetoothUUID("1003"));
  df3->AddDeviceFilter(device_filter3);
  df3->AddDeviceFilter(device_filter4);
  std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter3(df3);

  adapter_->StartDiscoverySessionWithFilter(
      std::move(discovery_filter3),
      base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallback,
                     base::Unretained(this)),
      base::BindOnce(&BluetoothBlueZTest::ErrorCallback,
                     base::Unretained(this)));

  base::RunLoop().Run();

  filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_EQ("auto", *filter->transport);
  EXPECT_EQ(-65, *filter->rssi);
  EXPECT_EQ(nullptr, filter->pathloss.get());
  uuids = *filter->uuids;
  EXPECT_TRUE(base::Contains(uuids, "1000"));
  EXPECT_TRUE(base::Contains(uuids, "1001"));
  EXPECT_TRUE(base::Contains(uuids, "1003"));
  EXPECT_TRUE(base::Contains(uuids, "1020"));

  // start additionally classic scan
  adapter_->StartDiscoverySession(
      base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallback,
                     base::Unretained(this)),
      base::BindOnce(&BluetoothBlueZTest::ErrorCallback,
                     base::Unretained(this)));

  base::RunLoop().Run();

  filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_EQ("auto", *filter->transport);
  EXPECT_EQ(nullptr, filter->rssi.get());
  EXPECT_EQ(nullptr, filter->pathloss.get());
  EXPECT_EQ(nullptr, filter->uuids.get());
  discovery_sessions_.clear();
}

TEST_F(BluetoothBlueZTest, DeviceProperties) {
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());

  int idx = GetDeviceIndexByAddress(
      devices, bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_NE(-1, idx);
  ASSERT_EQ(bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[idx]->GetAddress());

  // Verify the other device properties.
  EXPECT_EQ(
      base::UTF8ToUTF16(bluez::FakeBluetoothDeviceClient::kPairedDeviceName),
      devices[idx]->GetNameForDisplay());
  EXPECT_EQ(BluetoothDeviceType::COMPUTER, devices[idx]->GetDeviceType());
  EXPECT_TRUE(devices[idx]->IsPaired());
  EXPECT_FALSE(devices[idx]->IsConnected());
  EXPECT_FALSE(devices[idx]->IsConnecting());

  // Non HID devices are always connectable.
  EXPECT_TRUE(devices[idx]->IsConnectable());

  BluetoothDevice::UUIDSet uuids = devices[idx]->GetUUIDs();
  EXPECT_EQ(2U, uuids.size());
  EXPECT_TRUE(base::Contains(uuids, BluetoothUUID("1800")));
  EXPECT_TRUE(base::Contains(uuids, BluetoothUUID("1801")));

  EXPECT_EQ(BluetoothDevice::VENDOR_ID_USB, devices[idx]->GetVendorIDSource());
  EXPECT_EQ(0x05ac, devices[idx]->GetVendorID());
  EXPECT_EQ(0x030d, devices[idx]->GetProductID());
  EXPECT_EQ(0x0306, devices[idx]->GetDeviceID());
}

TEST_F(BluetoothBlueZTest, DeviceClassChanged) {
  // Simulate a change of class of a device, as sometimes occurs
  // during discovery.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());

  int idx = GetDeviceIndexByAddress(
      devices, bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_NE(-1, idx);
  ASSERT_EQ(BluetoothDeviceType::COMPUTER, devices[idx]->GetDeviceType());

  // Install an observer; expect the DeviceChanged method to be called when
  // we change the class of the device.
  TestBluetoothAdapterObserver observer(adapter_);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));

  properties->bluetooth_class.ReplaceValue(0x002580);

  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());

  EXPECT_EQ(BluetoothDeviceType::MOUSE, devices[idx]->GetDeviceType());
}

TEST_F(BluetoothBlueZTest, DeviceAppearance) {
  // Simulate a device with appearance.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());

  int idx = GetDeviceIndexByAddress(
      devices, bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_NE(-1, idx);
  ASSERT_EQ(BluetoothDeviceType::COMPUTER, devices[idx]->GetDeviceType());

  // Install an observer; expect the DeviceChanged method to be called when
  // we change the appearance of the device.
  TestBluetoothAdapterObserver observer(adapter_);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));

  // Let the device come without bluetooth_class.
  properties->appearance.ReplaceValue(0);  // DeviceChanged method called
  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());

  // Set the device appearance as keyboard (961).
  properties->appearance.ReplaceValue(961);  // DeviceChanged method called
  properties->appearance.set_valid(true);
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());
  EXPECT_EQ(961, devices[idx]->GetAppearance());
  // When discovery is over, the value should be invalidated.
  properties->appearance.set_valid(false);
  // DeviceChanged method called by NotifyPropertyChanged()
  properties->NotifyPropertyChanged(properties->appearance.name());
  EXPECT_EQ(3, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());
  EXPECT_EQ((int)BluetoothDevice::kAppearanceNotPresent,
            devices[idx]->GetAppearance());

  // Change the device appearance to mouse (962).
  properties->appearance.ReplaceValue(962);  // DeviceChanged method called
  properties->appearance.set_valid(true);
  EXPECT_EQ(4, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());
  EXPECT_EQ(962, devices[idx]->GetAppearance());
  // When discovery is over, the value should be invalidated.
  properties->appearance.set_valid(false);
  // DeviceChanged method called by NotifyPropertyChanged()
  properties->NotifyPropertyChanged(properties->appearance.name());
  EXPECT_EQ(5, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());
  EXPECT_EQ((int)BluetoothDevice::kAppearanceNotPresent,
            devices[idx]->GetAppearance());
}

TEST_F(BluetoothBlueZTest, DeviceTypebyAppearanceNotBluetoothClass) {
  // Test device type of a device with appearance but without bluetooth class.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());

  int idx = GetDeviceIndexByAddress(
      devices, bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_NE(-1, idx);
  ASSERT_EQ(BluetoothDeviceType::COMPUTER, devices[idx]->GetDeviceType());

  // Install an observer; expect the DeviceChanged method to be called when
  // we change the appearance of the device.
  TestBluetoothAdapterObserver observer(adapter_);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));

  // Let the device come without bluetooth_class.
  properties->bluetooth_class.ReplaceValue(0);  // DeviceChanged method called
  properties->appearance.ReplaceValue(0);       // DeviceChanged method called
  EXPECT_EQ(BluetoothDeviceType::UNKNOWN, devices[idx]->GetDeviceType());
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());

  // Set the device appearance as keyboard.
  properties->appearance.ReplaceValue(961);  // DeviceChanged method called
  properties->appearance.set_valid(true);
  EXPECT_EQ(BluetoothDeviceType::KEYBOARD, devices[idx]->GetDeviceType());
  EXPECT_EQ(3, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());
  // When discovery is over, the value should be invalidated.
  properties->appearance.set_valid(false);
  // DeviceChanged method called by NotifyPropertyChanged()
  properties->NotifyPropertyChanged(properties->appearance.name());
  EXPECT_EQ(4, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());
  EXPECT_EQ(BluetoothDeviceType::UNKNOWN, devices[idx]->GetDeviceType());

  // Change the device appearance to mouse.
  properties->appearance.ReplaceValue(962);  // DeviceChanged method called
  properties->appearance.set_valid(true);
  EXPECT_EQ(BluetoothDeviceType::MOUSE, devices[idx]->GetDeviceType());
  EXPECT_EQ(5, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());
  // When discovery is over, the value should be invalidated.
  properties->appearance.set_valid(false);
  // DeviceChanged method called by NotifyPropertyChanged()
  properties->NotifyPropertyChanged(properties->appearance.name());
  EXPECT_EQ(6, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());
  EXPECT_EQ(BluetoothDeviceType::UNKNOWN, devices[idx]->GetDeviceType());
}

TEST_F(BluetoothBlueZTest, DeviceNameChanged) {
  // Simulate a change of name of a device.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());

  int idx = GetDeviceIndexByAddress(
      devices, bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_NE(-1, idx);
  ASSERT_EQ(bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[idx]->GetAddress());
  ASSERT_EQ(
      base::UTF8ToUTF16(bluez::FakeBluetoothDeviceClient::kPairedDeviceName),
      devices[idx]->GetNameForDisplay());

  // Install an observer; expect the DeviceChanged method to be called when
  // we change the alias of the device.
  TestBluetoothAdapterObserver observer(adapter_);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));

  const std::string new_name("New Device Name");
  properties->name.ReplaceValue(new_name);

  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());

  EXPECT_EQ(base::UTF8ToUTF16(new_name), devices[idx]->GetNameForDisplay());
}

TEST_F(BluetoothBlueZTest, DeviceAddressChanged) {
  // Simulate a change of address of a device.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());

  int idx = GetDeviceIndexByAddress(
      devices, bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_NE(-1, idx);
  ASSERT_EQ(bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[idx]->GetAddress());
  ASSERT_EQ(
      base::UTF8ToUTF16(bluez::FakeBluetoothDeviceClient::kPairedDeviceName),
      devices[idx]->GetNameForDisplay());

  // Install an observer; expect the DeviceAddressChanged method to be called
  // when we change the alias of the device.
  TestBluetoothAdapterObserver observer(adapter_);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));

  static const char* kNewAddress = "D9:1F:FC:11:22:33";
  properties->address.ReplaceValue(kNewAddress);

  EXPECT_EQ(1, observer.device_address_changed_count());
  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());

  EXPECT_EQ(std::string(kNewAddress), devices[idx]->GetAddress());
}

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
TEST_F(BluetoothBlueZTest, DevicePairedChanged) {
  // Simulate a change of paired state of a device.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());

  int idx = GetDeviceIndexByAddress(
      devices, bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_NE(-1, idx);
  ASSERT_EQ(bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[idx]->GetAddress());
  ASSERT_EQ(true, devices[idx]->IsPaired());

  // Install an observer; expect the DevicePairedChanged method to be called
  // when we change the paired state of the device.
  TestBluetoothAdapterObserver observer(adapter_);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));

  properties->paired.ReplaceValue(false);

  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(1, observer.device_paired_changed_count());
  EXPECT_FALSE(observer.device_new_paired_status());
  EXPECT_EQ(devices[idx], observer.last_device());

  // Change the paired state back to true to examine the consistent behavior of
  // DevicePairedChanged method.
  properties->paired.ReplaceValue(true);

  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_EQ(2, observer.device_paired_changed_count());
  EXPECT_TRUE(observer.device_new_paired_status());
  EXPECT_EQ(devices[idx], observer.last_device());
}

TEST_F(BluetoothBlueZTest, DeviceMTUChanged) {
  // Simulate a change of MTU of a device.
  GetAdapter();

  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  // Install an observer; expect DeviceMTUChanged method to be called with the
  // updated MTU values.
  TestBluetoothAdapterObserver observer(adapter_);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(0, observer.device_mtu_changed_count());

  properties->mtu.ReplaceValue(258);
  EXPECT_EQ(1, observer.device_mtu_changed_count());
  EXPECT_EQ(258, observer.last_mtu_value());

  properties->mtu.ReplaceValue(128);
  EXPECT_EQ(2, observer.device_mtu_changed_count());
  EXPECT_EQ(128, observer.last_mtu_value());
}

TEST_F(BluetoothBlueZTest, DeviceAdvertisementReceived_LowEnergyDeviceAdded) {
  // Simulate reception of advertisement from a low energy device.
  GetAdapter();

  // Install an observer; expect DeviceAdvertisementReceived method to be called
  // with the EIR and RSSI.
  TestBluetoothAdapterObserver observer(adapter_);

  ASSERT_EQ(0, observer.device_advertisement_received_count());

  // A low energy device has a valid RSSI and EIR and should trigger the
  // advertisement callback.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  EXPECT_EQ(1, observer.device_advertisement_received_count());

  // A classic device does not have a valid RSSI and EIR and should not trigger
  // the advertisement callback.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kJustWorksPath));
  EXPECT_EQ(1, observer.device_advertisement_received_count());
}

TEST_F(BluetoothBlueZTest, DeviceAdvertisementReceived_PropertyChanged) {
  // Simulate reception of advertisement from a device.
  GetAdapter();

  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  // Install an observer; expect DeviceAdvertisementReceived method to be called
  // with the EIR and RSSI.
  TestBluetoothAdapterObserver observer(adapter_);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(0, observer.device_advertisement_received_count());

  properties->rssi.ReplaceValue(-73);
  EXPECT_EQ(1, observer.device_advertisement_received_count());

  std::vector<uint8_t> eir = {0x01, 0x02, 0x03};
  properties->eir.ReplaceValue(eir);
  EXPECT_EQ(2, observer.device_advertisement_received_count());
  EXPECT_EQ(eir, observer.device_eir());
}

TEST_F(BluetoothBlueZTest, DeviceConnectedStateChanged) {
  GetAdapter();

  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  // Install an observer; expect DeviceConnectedStateChanged method to be
  // called.
  TestBluetoothAdapterObserver observer(adapter_);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));

  // The device starts out disconnected.
  EXPECT_FALSE(device->IsConnected());

  properties->connected.ReplaceValue(true);
  EXPECT_EQ(1u, observer.device_connected_state_changed_values().size());
  EXPECT_TRUE(observer.device_connected_state_changed_values()[0]);

  properties->connected.ReplaceValue(false);
  EXPECT_EQ(2u, observer.device_connected_state_changed_values().size());
  EXPECT_FALSE(observer.device_connected_state_changed_values()[1]);
}
#endif

TEST_F(BluetoothBlueZTest, DeviceUuidsChanged) {
  // Simulate a change of advertised services of a device.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());

  int idx = GetDeviceIndexByAddress(
      devices, bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_NE(-1, idx);
  ASSERT_EQ(bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[idx]->GetAddress());

  BluetoothDevice::UUIDSet uuids = devices[idx]->GetUUIDs();
  ASSERT_EQ(2U, uuids.size());
  ASSERT_TRUE(base::Contains(uuids, BluetoothUUID("1800")));
  ASSERT_TRUE(base::Contains(uuids, BluetoothUUID("1801")));

  // Install an observer; expect the DeviceChanged method to be called when
  // we change the class of the device.
  TestBluetoothAdapterObserver observer(adapter_);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));

  std::vector<std::string> new_uuids;
  new_uuids.push_back(BluetoothUUID("1800").canonical_value());
  new_uuids.push_back(BluetoothUUID("1801").canonical_value());
  new_uuids.push_back("0000110c-0000-1000-8000-00805f9b34fb");
  new_uuids.push_back("0000110e-0000-1000-8000-00805f9b34fb");
  new_uuids.push_back("0000110a-0000-1000-8000-00805f9b34fb");

  properties->uuids.ReplaceValue(new_uuids);

  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());

  // Fetching the value should give the new one.
  uuids = devices[idx]->GetUUIDs();
  EXPECT_EQ(5U, uuids.size());
  EXPECT_TRUE(base::Contains(uuids, BluetoothUUID("1800")));
  EXPECT_TRUE(base::Contains(uuids, BluetoothUUID("1801")));
  EXPECT_TRUE(base::Contains(uuids, BluetoothUUID("110c")));
  EXPECT_TRUE(base::Contains(uuids, BluetoothUUID("110e")));
  EXPECT_TRUE(base::Contains(uuids, BluetoothUUID("110a")));
}

TEST_F(BluetoothBlueZTest, DeviceInquiryRSSIInvalidated) {
  // Simulate invalidation of inquiry RSSI of a device, as it occurs
  // when discovery is finished.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());

  int idx = GetDeviceIndexByAddress(
      devices, bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_NE(-1, idx);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));

  // During discovery, rssi is a valid value (-75)
  properties->rssi.ReplaceValue(-75);
  properties->rssi.set_valid(true);

  ASSERT_EQ(-75, devices[idx]->GetInquiryRSSI().value());

  properties->rssi.ReplaceValue(INT8_MAX + 1);
  ASSERT_EQ(INT8_MAX, devices[idx]->GetInquiryRSSI().value());

  properties->rssi.ReplaceValue(INT8_MIN - 1);
  ASSERT_EQ(INT8_MIN, devices[idx]->GetInquiryRSSI().value());

  // Install an observer; expect the DeviceChanged method to be called when
  // we invalidate the RSSI of the device.
  TestBluetoothAdapterObserver observer(adapter_);

  // When discovery is over, the value should be invalidated.
  properties->rssi.set_valid(false);
  properties->NotifyPropertyChanged(properties->rssi.name());

  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());

  EXPECT_FALSE(devices[idx]->GetInquiryRSSI());
}

TEST_F(BluetoothBlueZTest, DeviceInquiryTxPowerInvalidated) {
  // Simulate invalidation of inquiry TxPower of a device, as it occurs
  // when discovery is finished.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());

  int idx = GetDeviceIndexByAddress(
      devices, bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_NE(-1, idx);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));

  // During discovery, tx_power is a valid value (0)
  properties->tx_power.ReplaceValue(0);
  properties->tx_power.set_valid(true);

  ASSERT_EQ(0, devices[idx]->GetInquiryTxPower().value());

  properties->tx_power.ReplaceValue(INT8_MAX + 1);
  ASSERT_EQ(INT8_MAX, devices[idx]->GetInquiryTxPower().value());

  properties->tx_power.ReplaceValue(INT8_MIN - 1);
  ASSERT_EQ(INT8_MIN, devices[idx]->GetInquiryTxPower().value());

  // Install an observer; expect the DeviceChanged method to be called when
  // we invalidate the tx_power of the device.
  TestBluetoothAdapterObserver observer(adapter_);

  // When discovery is over, the value should be invalidated.
  properties->tx_power.set_valid(false);
  properties->NotifyPropertyChanged(properties->tx_power.name());

  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(devices[idx], observer.last_device());

  EXPECT_FALSE(devices[idx]->GetInquiryTxPower());
}

TEST_F(BluetoothBlueZTest, ForgetDevice) {
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());

  int idx = GetDeviceIndexByAddress(
      devices, bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_NE(-1, idx);
  ASSERT_EQ(bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[idx]->GetAddress());

  std::string address = devices[idx]->GetAddress();

  // Install an observer; expect the DeviceRemoved method to be called
  // with the device we remove.
  TestBluetoothAdapterObserver observer(adapter_);

  devices[idx]->Forget(base::DoNothing(), GetErrorCallback());
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.device_removed_count());
  EXPECT_EQ(address, observer.last_device_address());

  // GetDevices shouldn't return the device either.
  devices = adapter_->GetDevices();
  ASSERT_EQ(1U, devices.size());
}

TEST_F(BluetoothBlueZTest, ForgetUnpairedDevice) {
  GetAdapter();
  DiscoverDevices();

  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kConnectUnpairableAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  // Connect the device so it becomes trusted and remembered.
  device->Connect(nullptr, GetOnceCallback(),
                  base::BindOnce(&BluetoothBlueZTest::ConnectErrorCallback,
                                 base::Unretained(this)));

  ASSERT_EQ(1, callback_count_);
  ASSERT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(device->IsConnected());
  ASSERT_FALSE(device->IsConnecting());

  // Pause discovery to connect without pair.
  EXPECT_EQ(1, fake_bluetooth_adapter_client_->GetPauseCount());
  EXPECT_EQ(1, fake_bluetooth_adapter_client_->GetUnpauseCount());

  // Make sure the trusted property has been set to true.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kConnectUnpairablePath));
  ASSERT_TRUE(properties->trusted.value());

  // Install an observer; expect the DeviceRemoved method to be called
  // with the device we remove.
  TestBluetoothAdapterObserver observer(adapter_);

  device->Forget(base::DoNothing(), GetErrorCallback());
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.device_removed_count());
  EXPECT_EQ(bluez::FakeBluetoothDeviceClient::kConnectUnpairableAddress,
            observer.last_device_address());

  // GetDevices shouldn't return the device either.
  device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kConnectUnpairableAddress);
  EXPECT_FALSE(device != nullptr);
}

TEST_F(BluetoothBlueZTest, ConnectPairedDevice) {
  GetAdapter();

  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_TRUE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  // Connect without a pairing delegate; since the device is already Paired
  // this should succeed and the device should become connected.
  device->Connect(nullptr, GetOnceCallback(),
                  base::BindOnce(&BluetoothBlueZTest::ConnectErrorCallback,
                                 base::Unretained(this)));

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one for connected and one for for trusted
  // after connecting.
  EXPECT_EQ(4, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  // Pause discovery to connect without pair.
  EXPECT_EQ(1, fake_bluetooth_adapter_client_->GetPauseCount());
  EXPECT_EQ(1, fake_bluetooth_adapter_client_->GetUnpauseCount());
}

TEST_F(BluetoothBlueZTest, ConnectUnpairableDevice) {
  GetAdapter();
  DiscoverDevices();

  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kConnectUnpairableAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  // Connect without a pairing delegate; since the device does not require
  // pairing, this should succeed and the device should become connected.
  device->Connect(nullptr, GetOnceCallback(),
                  base::BindOnce(&BluetoothBlueZTest::ConnectErrorCallback,
                                 base::Unretained(this)));

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one for connected, one for for trusted after
  // connection, and one for the reconnect mode (IsConnectable).
  EXPECT_EQ(5, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  // Pause discovery to connect without pair.
  EXPECT_EQ(1, fake_bluetooth_adapter_client_->GetPauseCount());
  EXPECT_EQ(1, fake_bluetooth_adapter_client_->GetUnpauseCount());

  // Make sure the trusted property has been set to true.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kConnectUnpairablePath));
  EXPECT_TRUE(properties->trusted.value());

  // Verify is a HID device and is not connectable.
  BluetoothDevice::UUIDSet uuids = device->GetUUIDs();
  EXPECT_EQ(1U, uuids.size());
  EXPECT_TRUE(base::Contains(uuids, BluetoothUUID("1124")));
  EXPECT_FALSE(device->IsConnectable());
}

TEST_F(BluetoothBlueZTest, ConnectConnectedDevice) {
  GetAdapter();

  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_TRUE(device->IsPaired());

  device->Connect(nullptr, GetOnceCallback(),
                  base::BindOnce(&BluetoothBlueZTest::ConnectErrorCallback,
                                 base::Unretained(this)));

  ASSERT_EQ(1, callback_count_);
  ASSERT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(device->IsConnected());

  // Pause discovery to connect without pair.
  EXPECT_EQ(1, fake_bluetooth_adapter_client_->GetPauseCount());
  EXPECT_EQ(1, fake_bluetooth_adapter_client_->GetUnpauseCount());

  // Connect again; since the device is already Connected, this shouldn't do
  // anything to initiate the connection.
  TestBluetoothAdapterObserver observer(adapter_);

  device->Connect(nullptr, GetOnceCallback(),
                  base::BindOnce(&BluetoothBlueZTest::ConnectErrorCallback,
                                 base::Unretained(this)));

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // The observer will be called because Connecting will toggle true and false,
  // and the trusted property will be updated to true.
  EXPECT_EQ(3, observer.device_changed_count());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  // Pause discovery to connect without pair.
  EXPECT_EQ(2, fake_bluetooth_adapter_client_->GetPauseCount());
  EXPECT_EQ(2, fake_bluetooth_adapter_client_->GetUnpauseCount());
}

TEST_F(BluetoothBlueZTest, ConnectDeviceFails) {
  GetAdapter();
  DiscoverDevices();

  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kLegacyAutopairAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  // Connect without a pairing delegate; since the device requires pairing,
  // this should fail with an error.
  device->Connect(nullptr, GetOnceCallback(),
                  base::BindOnce(&BluetoothBlueZTest::ConnectErrorCallback,
                                 base::Unretained(this)));

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_FAILED, last_connect_error_);

  EXPECT_EQ(2, observer.device_changed_count());

  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  // Pause discovery to connect without pair.
  EXPECT_EQ(1, fake_bluetooth_adapter_client_->GetPauseCount());
  EXPECT_EQ(1, fake_bluetooth_adapter_client_->GetUnpauseCount());
}

// Tests that discovery is unpaused if the device gets removed during a
// connection.
TEST_F(BluetoothBlueZTest, RemoveDeviceDuringConnection) {
  GetAdapter();

  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != nullptr);

  fake_bluetooth_device_client_->LeaveConnectionsPending();
  device->Connect(nullptr, GetOnceCallback(),
                  base::BindOnce(&BluetoothBlueZTest::ConnectErrorCallback,
                                 base::Unretained(this)));
  // We pause discovery before connecting.
  EXPECT_EQ(1, fake_bluetooth_adapter_client_->GetPauseCount());
  EXPECT_EQ(0, fake_bluetooth_adapter_client_->GetUnpauseCount());

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_FALSE(device->IsConnected());
  EXPECT_TRUE(device->IsConnecting());

  // Install an observer; expect the DeviceRemoved method to be called
  // with the device we remove.
  TestBluetoothAdapterObserver observer(adapter_);

  device->Forget(base::DoNothing(), GetErrorCallback());
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.device_removed_count());

  // If the device gets removed, we should still unpause discovery.
  EXPECT_EQ(1, fake_bluetooth_adapter_client_->GetPauseCount());
  EXPECT_EQ(1, fake_bluetooth_adapter_client_->GetUnpauseCount());
}

TEST_F(BluetoothBlueZTest, DisconnectDevice) {
  GetAdapter();

  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_TRUE(device->IsPaired());

  device->Connect(nullptr, GetOnceCallback(),
                  base::BindOnce(&BluetoothBlueZTest::ConnectErrorCallback,
                                 base::Unretained(this)));

  ASSERT_EQ(1, callback_count_);
  ASSERT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(device->IsConnected());
  ASSERT_FALSE(device->IsConnecting());

  // Pause discovery to connect without pair.
  EXPECT_EQ(1, fake_bluetooth_adapter_client_->GetPauseCount());
  EXPECT_EQ(1, fake_bluetooth_adapter_client_->GetUnpauseCount());

  // Disconnect the device, we should see the observer method fire and the
  // device get dropped.
  TestBluetoothAdapterObserver observer(adapter_);

  device->Disconnect(GetCallback(), GetErrorCallback());

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_FALSE(device->IsConnected());
}

TEST_F(BluetoothBlueZTest, DisconnectUnconnectedDevice) {
  GetAdapter();

  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_TRUE(device->IsPaired());
  ASSERT_FALSE(device->IsConnected());

  // Disconnect the device, we should see the observer method fire and the
  // device get dropped.
  TestBluetoothAdapterObserver observer(adapter_);

  device->Disconnect(GetCallback(), GetErrorCallback());

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);

  EXPECT_EQ(0, observer.device_changed_count());

  EXPECT_FALSE(device->IsConnected());
}

TEST_F(BluetoothBlueZTest, PairTrustedDevice) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);
  GetAdapter();

  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::
                           kConnectedTrustedNotPairedDevicePath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::
                              kConnectedTrustedNotPairedDeviceAddress);
  ASSERT_TRUE(device != nullptr);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::
                               kConnectedTrustedNotPairedDevicePath));
  EXPECT_FALSE(properties->paired.value());
  EXPECT_TRUE(properties->trusted.value());
  ASSERT_FALSE(device->IsPaired());

  // The |kConnectedTrustedNotPairedDevicePath| requests a passkey confirmation.
  // Obs.: This is the flow when CrOS triggers pairing with a iOS device.
  TestBluetoothAdapterObserver observer(adapter_);
  TestPairingDelegate pairing_delegate;
  adapter_->AddPairingDelegate(
      &pairing_delegate, BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);
  device->Pair(&pairing_delegate, GetOnceCallback(),
               base::BindOnce(&BluetoothBlueZTest::ConnectErrorCallback,
                              base::Unretained(this)));
  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.confirm_passkey_count_);
  EXPECT_EQ(123456U, pairing_delegate.last_passkey_);

  // Confirm the passkey.
  device->ConfirmPairing();
  base::RunLoop().Run();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Make sure the paired property has been set to true.
  properties = fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
      bluez::FakeBluetoothDeviceClient::kConnectedTrustedNotPairedDevicePath));
  EXPECT_TRUE(properties->paired.value());
}

TEST_F(BluetoothBlueZTest, PairAlreadyPairedDevice) {
  GetAdapter();

  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kPairedDevicePath));
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != nullptr);

  // On the DBus level a device can be trusted but not paired.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));
  EXPECT_TRUE(properties->paired.value());
  EXPECT_TRUE(properties->trusted.value());
  ASSERT_TRUE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);
  TestPairingDelegate pairing_delegate;
  adapter_->AddPairingDelegate(
      &pairing_delegate, BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);
  device->Pair(&pairing_delegate, GetOnceCallback(),
               base::BindOnce(&BluetoothBlueZTest::ConnectErrorCallback,
                              base::Unretained(this)));

  // For already paired devices a call to |Pair| should succeed without calling
  // the pairing delegate.
  EXPECT_EQ(0, pairing_delegate.call_count_);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}

TEST_F(BluetoothBlueZTest, PairLegacyAutopair) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // The Legacy Autopair device requires no PIN or Passkey to pair because
  // the daemon provides 0000 to the device for us.
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kLegacyAutopairAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetOnceCallback(),
                  base::BindOnce(&BluetoothBlueZTest::ConnectErrorCallback,
                                 base::Unretained(this)));

  EXPECT_EQ(0, pairing_delegate.call_count_);
  EXPECT_TRUE(device->IsConnecting());

  base::RunLoop().Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one change for connected, one for paired,
  // two for trusted (after pairing and connection), and one for the reconnect
  // mode (IsConnectable).
  EXPECT_EQ(7, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  // Connect after pair.
  EXPECT_EQ(0, fake_bluetooth_adapter_client_->GetPauseCount());
  EXPECT_EQ(1, fake_bluetooth_adapter_client_->GetUnpauseCount());

  EXPECT_TRUE(device->IsPaired());

  // Verify is a HID device and is connectable.
  BluetoothDevice::UUIDSet uuids = device->GetUUIDs();
  EXPECT_EQ(1U, uuids.size());
  EXPECT_TRUE(base::Contains(uuids, BluetoothUUID("1124")));
  EXPECT_TRUE(device->IsConnectable());

  // Make sure the trusted property has been set to true.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kLegacyAutopairPath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothBlueZTest, PairDisplayPinCode) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Requires that we display a randomly generated PIN on the screen.
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kDisplayPinCodeAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetOnceCallback(),
                  base::BindOnce(&BluetoothBlueZTest::ConnectErrorCallback,
                                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.display_pincode_count_);
  EXPECT_EQ("123456", pairing_delegate.last_pincode_);
  EXPECT_TRUE(device->IsConnecting());

  base::RunLoop().Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one change for connected, one for paired,
  // two for trusted (after pairing and connection), and one for the reconnect
  // mode (IsConnectable).
  EXPECT_EQ(7, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  // Connect after pair.
  EXPECT_EQ(0, fake_bluetooth_adapter_client_->GetPauseCount());
  EXPECT_EQ(1, fake_bluetooth_adapter_client_->GetUnpauseCount());

  EXPECT_TRUE(device->IsPaired());

  // Verify is a HID device and is connectable.
  BluetoothDevice::UUIDSet uuids = device->GetUUIDs();
  EXPECT_EQ(1U, uuids.size());
  EXPECT_TRUE(base::Contains(uuids, BluetoothUUID("1124")));
  EXPECT_TRUE(device->IsConnectable());

  // Make sure the trusted property has been set to true.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kDisplayPinCodePath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothBlueZTest, PairDisplayPasskey) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Requires that we display a randomly generated Passkey on the screen,
  // and notifies us as it's typed in.
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kDisplayPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetOnceCallback(),
                  base::BindOnce(&BluetoothBlueZTest::ConnectErrorCallback,
                                 base::Unretained(this)));

  // One call for DisplayPasskey() and one for KeysEntered().
  EXPECT_EQ(2, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.display_passkey_count_);
  EXPECT_EQ(123456U, pairing_delegate.last_passkey_);
  EXPECT_EQ(1, pairing_delegate.keys_entered_count_);
  EXPECT_EQ(0U, pairing_delegate.last_entered_);

  EXPECT_TRUE(device->IsConnecting());

  // One call to KeysEntered() for each key, including [enter].
  for (int i = 1; i <= 7; ++i) {
    base::RunLoop().Run();

    EXPECT_EQ(2 + i, pairing_delegate.call_count_);
    EXPECT_EQ(1 + i, pairing_delegate.keys_entered_count_);
    EXPECT_EQ(static_cast<uint32_t>(i), pairing_delegate.last_entered_);
  }

  base::RunLoop().Run();

  // 8 KeysEntered notifications (0 to 7, inclusive) and one aditional call for
  // DisplayPasskey().
  EXPECT_EQ(9, pairing_delegate.call_count_);
  EXPECT_EQ(8, pairing_delegate.keys_entered_count_);
  EXPECT_EQ(7U, pairing_delegate.last_entered_);

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one change for connected, one for paired,
  // two for trusted (after pairing and connection), and one for the reconnect
  // mode (IsConnectable).
  EXPECT_EQ(7, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  // Connect after pair.
  EXPECT_EQ(0, fake_bluetooth_adapter_client_->GetPauseCount());
  EXPECT_EQ(1, fake_bluetooth_adapter_client_->GetUnpauseCount());

  EXPECT_TRUE(device->IsPaired());

  // Verify is a HID device.
  BluetoothDevice::UUIDSet uuids = device->GetUUIDs();
  EXPECT_EQ(1U, uuids.size());
  EXPECT_TRUE(base::Contains(uuids, BluetoothUUID("1124")));

  // And usually not connectable.
  EXPECT_FALSE(device->IsConnectable());

  // Make sure the trusted property has been set to true.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kDisplayPasskeyPath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothBlueZTest, PairRequestPinCode) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);
  GetAdapter();
  DiscoverDevices();

  // Requires that the user enters a PIN for them.
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kRequestPinCodeAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetOnceCallback(),
                  base::BindOnce(&BluetoothBlueZTest::ConnectErrorCallback,
                                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_pincode_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Set the PIN.
  device->SetPinCode("1234");
  base::RunLoop().Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one change for connected, one for paired and
  // two for trusted (after pairing and connection).
  EXPECT_EQ(6, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  // Connect after pair.
  EXPECT_EQ(0, fake_bluetooth_adapter_client_->GetPauseCount());
  EXPECT_EQ(1, fake_bluetooth_adapter_client_->GetUnpauseCount());

  EXPECT_TRUE(device->IsPaired());

  // Verify is not a HID device.
  BluetoothDevice::UUIDSet uuids = device->GetUUIDs();
  EXPECT_EQ(0U, uuids.size());

  // Non HID devices are always connectable.
  EXPECT_TRUE(device->IsConnectable());

  // Make sure the trusted property has been set to true.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kRequestPinCodePath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothBlueZTest, PairConfirmPasskey) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Requests that we confirm a displayed passkey.
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kConfirmPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetOnceCallback(),
                  base::BindOnce(&BluetoothBlueZTest::ConnectErrorCallback,
                                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.confirm_passkey_count_);
  EXPECT_EQ(123456U, pairing_delegate.last_passkey_);
  EXPECT_TRUE(device->IsConnecting());

  // Confirm the passkey.
  device->ConfirmPairing();
  base::RunLoop().Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one change for connected, one for paired and
  // two for trusted (after pairing and connection).
  EXPECT_EQ(6, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  // Connect after pair.
  EXPECT_EQ(0, fake_bluetooth_adapter_client_->GetPauseCount());
  EXPECT_EQ(1, fake_bluetooth_adapter_client_->GetUnpauseCount());

  EXPECT_TRUE(device->IsPaired());

  // Non HID devices are always connectable.
  EXPECT_TRUE(device->IsConnectable());

  // Make sure the trusted property has been set to true.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kConfirmPasskeyPath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothBlueZTest, PairRequestPasskey) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Requires that the user enters a Passkey, this would be some kind of
  // device that has a display, but doesn't use "just works" - maybe a car?
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kRequestPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetOnceCallback(),
                  base::BindOnce(&BluetoothBlueZTest::ConnectErrorCallback,
                                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_passkey_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Set the Passkey.
  device->SetPasskey(1234);
  base::RunLoop().Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one change for connected, one for paired and
  // two for trusted (after pairing and connection).
  EXPECT_EQ(6, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  // Connect after pair.
  EXPECT_EQ(0, fake_bluetooth_adapter_client_->GetPauseCount());
  EXPECT_EQ(1, fake_bluetooth_adapter_client_->GetUnpauseCount());

  EXPECT_TRUE(device->IsPaired());

  // Non HID devices are always connectable.
  EXPECT_TRUE(device->IsConnectable());

  // Make sure the trusted property has been set to true.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kRequestPasskeyPath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothBlueZTest, PairJustWorks) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Uses just-works pairing, since this is an outgoing pairing, no delegate
  // interaction is required.
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kJustWorksAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetOnceCallback(),
                  base::BindOnce(&BluetoothBlueZTest::ConnectErrorCallback,
                                 base::Unretained(this)));

  EXPECT_EQ(0, pairing_delegate.call_count_);

  base::RunLoop().Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one change for connected, one for paired and
  // two for trusted (after pairing and connection).
  EXPECT_EQ(6, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  // Connect after pair.
  EXPECT_EQ(0, fake_bluetooth_adapter_client_->GetPauseCount());
  EXPECT_EQ(1, fake_bluetooth_adapter_client_->GetUnpauseCount());

  EXPECT_TRUE(device->IsPaired());

  // Non HID devices are always connectable.
  EXPECT_TRUE(device->IsConnectable());

  // Make sure the trusted property has been set to true.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kJustWorksPath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothBlueZTest, PairUnpairableDeviceFails) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevice(bluez::FakeBluetoothDeviceClient::kUnconnectableDeviceAddress);

  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kUnpairableDeviceAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetOnceCallback(),
                  base::BindOnce(&BluetoothBlueZTest::ConnectErrorCallback,
                                 base::Unretained(this)));

  EXPECT_EQ(0, pairing_delegate.call_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Run the loop to get the error..
  base::RunLoop().Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);

  EXPECT_EQ(BluetoothDevice::ERROR_FAILED, last_connect_error_);

  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());

  // No connection.
  EXPECT_EQ(0, fake_bluetooth_adapter_client_->GetPauseCount());
  EXPECT_EQ(0, fake_bluetooth_adapter_client_->GetUnpauseCount());
}

TEST_F(BluetoothBlueZTest, PairingFails) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevice(bluez::FakeBluetoothDeviceClient::kVanishingDeviceAddress);

  // The vanishing device times out during pairing
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kVanishingDeviceAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetOnceCallback(),
                  base::BindOnce(&BluetoothBlueZTest::ConnectErrorCallback,
                                 base::Unretained(this)));

  EXPECT_EQ(0, pairing_delegate.call_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Run the loop to get the error..
  base::RunLoop().Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);

  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_TIMEOUT, last_connect_error_);

  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());

  // No connection.
  EXPECT_EQ(0, fake_bluetooth_adapter_client_->GetPauseCount());
  EXPECT_EQ(0, fake_bluetooth_adapter_client_->GetUnpauseCount());
}

TEST_F(BluetoothBlueZTest, PairingFailsAtConnection) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Everything seems to go according to plan with the unconnectable device;
  // it pairs, but then you can't make connections to it after.
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kUnconnectableDeviceAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetOnceCallback(),
                  base::BindOnce(&BluetoothBlueZTest::ConnectErrorCallback,
                                 base::Unretained(this)));

  EXPECT_EQ(0, pairing_delegate.call_count_);
  EXPECT_TRUE(device->IsConnecting());

  base::RunLoop().Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_FAILED, last_connect_error_);

  // Two changes for connecting, one for paired and one for trusted after
  // pairing. The device should not be connected.
  EXPECT_EQ(4, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  // Connect after pair.
  EXPECT_EQ(0, fake_bluetooth_adapter_client_->GetPauseCount());
  EXPECT_EQ(1, fake_bluetooth_adapter_client_->GetUnpauseCount());

  EXPECT_TRUE(device->IsPaired());

  // Make sure the trusted property has been set to true still (since pairing
  // worked).
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kUnconnectableDevicePath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothBlueZTest, PairingRejectedAtPinCode) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Reject the pairing after we receive a request for the PIN code.
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kRequestPinCodeAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetOnceCallback(),
                  base::BindOnce(&BluetoothBlueZTest::ConnectErrorCallback,
                                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_pincode_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Reject the pairing.
  device->RejectPairing();
  base::RunLoop().Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_REJECTED, last_connect_error_);

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());

  // No connection.
  EXPECT_EQ(0, fake_bluetooth_adapter_client_->GetPauseCount());
  EXPECT_EQ(0, fake_bluetooth_adapter_client_->GetUnpauseCount());
}

TEST_F(BluetoothBlueZTest, PairingCancelledAtPinCode) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Cancel the pairing after we receive a request for the PIN code.
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kRequestPinCodeAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetOnceCallback(),
                  base::BindOnce(&BluetoothBlueZTest::ConnectErrorCallback,
                                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_pincode_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Cancel the pairing.
  device->CancelPairing();
  base::RunLoop().Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_CANCELED, last_connect_error_);

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());

  // No connection.
  EXPECT_EQ(0, fake_bluetooth_adapter_client_->GetPauseCount());
  EXPECT_EQ(0, fake_bluetooth_adapter_client_->GetUnpauseCount());
}

TEST_F(BluetoothBlueZTest, PairingRejectedAtPasskey) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Reject the pairing after we receive a request for the passkey.
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kRequestPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetOnceCallback(),
                  base::BindOnce(&BluetoothBlueZTest::ConnectErrorCallback,
                                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_passkey_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Reject the pairing.
  device->RejectPairing();
  base::RunLoop().Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_REJECTED, last_connect_error_);

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());

  // No connection.
  EXPECT_EQ(0, fake_bluetooth_adapter_client_->GetPauseCount());
  EXPECT_EQ(0, fake_bluetooth_adapter_client_->GetUnpauseCount());
}

TEST_F(BluetoothBlueZTest, PairingCancelledAtPasskey) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Cancel the pairing after we receive a request for the passkey.
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kRequestPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetOnceCallback(),
                  base::BindOnce(&BluetoothBlueZTest::ConnectErrorCallback,
                                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_passkey_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Cancel the pairing.
  device->CancelPairing();
  base::RunLoop().Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_CANCELED, last_connect_error_);

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());

  // No connection.
  EXPECT_EQ(0, fake_bluetooth_adapter_client_->GetPauseCount());
  EXPECT_EQ(0, fake_bluetooth_adapter_client_->GetUnpauseCount());
}

TEST_F(BluetoothBlueZTest, PairingRejectedAtConfirmation) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Reject the pairing after we receive a request for passkey confirmation.
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kConfirmPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetOnceCallback(),
                  base::BindOnce(&BluetoothBlueZTest::ConnectErrorCallback,
                                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.confirm_passkey_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Reject the pairing.
  device->RejectPairing();
  base::RunLoop().Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_REJECTED, last_connect_error_);

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());

  // No connection.
  EXPECT_EQ(0, fake_bluetooth_adapter_client_->GetPauseCount());
  EXPECT_EQ(0, fake_bluetooth_adapter_client_->GetUnpauseCount());
}

TEST_F(BluetoothBlueZTest, PairingCancelledAtConfirmation) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Cancel the pairing after we receive a request for the passkey.
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kConfirmPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetOnceCallback(),
                  base::BindOnce(&BluetoothBlueZTest::ConnectErrorCallback,
                                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.confirm_passkey_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Cancel the pairing.
  device->CancelPairing();
  base::RunLoop().Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_CANCELED, last_connect_error_);

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());

  // No connection.
  EXPECT_EQ(0, fake_bluetooth_adapter_client_->GetPauseCount());
  EXPECT_EQ(0, fake_bluetooth_adapter_client_->GetUnpauseCount());
}

TEST_F(BluetoothBlueZTest, PairingCancelledInFlight) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Cancel the pairing while we're waiting for the remote host.
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kLegacyAutopairAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetOnceCallback(),
                  base::BindOnce(&BluetoothBlueZTest::ConnectErrorCallback,
                                 base::Unretained(this)));

  EXPECT_EQ(0, pairing_delegate.call_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Cancel the pairing.
  device->CancelPairing();
  base::RunLoop().Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_CANCELED, last_connect_error_);

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());

  // No connection.
  EXPECT_EQ(0, fake_bluetooth_adapter_client_->GetPauseCount());
  EXPECT_EQ(0, fake_bluetooth_adapter_client_->GetUnpauseCount());
}

TEST_F(BluetoothBlueZTest, IncomingPairRequestPinCode) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  TestPairingDelegate pairing_delegate;
  adapter_->AddPairingDelegate(
      &pairing_delegate, BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);

  // Requires that we provide a PIN code.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kRequestPinCodePath));
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kRequestPinCodeAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kRequestPinCodePath),
      true, GetCallback(),
      base::BindOnce(&BluetoothBlueZTest::DBusErrorCallback,
                     base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_pincode_count_);

  // Set the PIN.
  device->SetPinCode("1234");
  base::RunLoop().Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // One change for paired, and one for trusted.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsPaired());

  // Make sure the trusted property has been set to true.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kRequestPinCodePath));
  ASSERT_TRUE(properties->trusted.value());

  // No pairing context should remain on the device.
  BluetoothDeviceBlueZ* device_bluez =
      static_cast<BluetoothDeviceBlueZ*>(device);
  EXPECT_TRUE(device_bluez->GetPairing() == nullptr);
}

TEST_F(BluetoothBlueZTest, IncomingPairConfirmPasskey) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  TestPairingDelegate pairing_delegate;
  adapter_->AddPairingDelegate(
      &pairing_delegate, BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);

  // Requests that we confirm a displayed passkey.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kConfirmPasskeyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kConfirmPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kConfirmPasskeyPath),
      true, GetCallback(),
      base::BindOnce(&BluetoothBlueZTest::DBusErrorCallback,
                     base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.confirm_passkey_count_);
  EXPECT_EQ(123456U, pairing_delegate.last_passkey_);

  // Confirm the passkey.
  device->ConfirmPairing();
  base::RunLoop().Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // One change for paired, and one for trusted.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsPaired());

  // Make sure the trusted property has been set to true.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kConfirmPasskeyPath));
  ASSERT_TRUE(properties->trusted.value());

  // No pairing context should remain on the device.
  BluetoothDeviceBlueZ* device_bluez =
      static_cast<BluetoothDeviceBlueZ*>(device);
  EXPECT_TRUE(device_bluez->GetPairing() == nullptr);
}

TEST_F(BluetoothBlueZTest, IncomingPairRequestPasskey) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  TestPairingDelegate pairing_delegate;
  adapter_->AddPairingDelegate(
      &pairing_delegate, BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);

  // Requests that we provide a Passkey.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kRequestPasskeyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kRequestPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kRequestPasskeyPath),
      true, GetCallback(),
      base::BindOnce(&BluetoothBlueZTest::DBusErrorCallback,
                     base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_passkey_count_);

  // Set the Passkey.
  device->SetPasskey(1234);
  base::RunLoop().Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // One change for paired, and one for trusted.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsPaired());

  // Make sure the trusted property has been set to true.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kRequestPasskeyPath));
  ASSERT_TRUE(properties->trusted.value());

  // No pairing context should remain on the device.
  BluetoothDeviceBlueZ* device_bluez =
      static_cast<BluetoothDeviceBlueZ*>(device);
  EXPECT_TRUE(device_bluez->GetPairing() == nullptr);
}

TEST_F(BluetoothBlueZTest, IncomingPairJustWorks) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  TestPairingDelegate pairing_delegate;
  adapter_->AddPairingDelegate(
      &pairing_delegate, BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);

  // Uses just-works pairing so, sinec this an incoming pairing, require
  // authorization from the user.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kJustWorksPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kJustWorksAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kJustWorksPath), true,
      GetCallback(),
      base::BindOnce(&BluetoothBlueZTest::DBusErrorCallback,
                     base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.authorize_pairing_count_);

  // Confirm the pairing.
  device->ConfirmPairing();
  base::RunLoop().Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // One change for paired, and one for trusted.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsPaired());

  // Make sure the trusted property has been set to true.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kJustWorksPath));
  ASSERT_TRUE(properties->trusted.value());

  // No pairing context should remain on the device.
  BluetoothDeviceBlueZ* device_bluez =
      static_cast<BluetoothDeviceBlueZ*>(device);
  EXPECT_TRUE(device_bluez->GetPairing() == nullptr);
}

TEST_F(BluetoothBlueZTest, IncomingPairRequestPinCodeWithoutDelegate) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  // Requires that we provide a PIN Code, without a pairing delegate,
  // that will be rejected.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kRequestPinCodePath));
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kRequestPinCodeAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kRequestPinCodePath),
      true, GetCallback(),
      base::BindOnce(&BluetoothBlueZTest::DBusErrorCallback,
                     base::Unretained(this)));

  base::RunLoop().Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(bluetooth_device::kErrorAuthenticationRejected, last_client_error_);

  // No changes should be observer.
  EXPECT_EQ(0, observer.device_changed_count());

  EXPECT_FALSE(device->IsPaired());

  // No pairing context should remain on the device.
  BluetoothDeviceBlueZ* device_bluez =
      static_cast<BluetoothDeviceBlueZ*>(device);
  EXPECT_TRUE(device_bluez->GetPairing() == nullptr);
}

TEST_F(BluetoothBlueZTest, IncomingPairConfirmPasskeyWithoutDelegate) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  // Requests that we confirm a displayed passkey, without a pairing delegate,
  // that will be rejected.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kConfirmPasskeyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kConfirmPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kConfirmPasskeyPath),
      true, GetCallback(),
      base::BindOnce(&BluetoothBlueZTest::DBusErrorCallback,
                     base::Unretained(this)));

  base::RunLoop().Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(bluetooth_device::kErrorAuthenticationRejected, last_client_error_);

  // No changes should be observer.
  EXPECT_EQ(0, observer.device_changed_count());

  EXPECT_FALSE(device->IsPaired());

  // No pairing context should remain on the device.
  BluetoothDeviceBlueZ* device_bluez =
      static_cast<BluetoothDeviceBlueZ*>(device);
  EXPECT_TRUE(device_bluez->GetPairing() == nullptr);
}

TEST_F(BluetoothBlueZTest, IncomingPairRequestPasskeyWithoutDelegate) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  // Requests that we provide a displayed passkey, without a pairing delegate,
  // that will be rejected.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kRequestPasskeyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kRequestPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kRequestPasskeyPath),
      true, GetCallback(),
      base::BindOnce(&BluetoothBlueZTest::DBusErrorCallback,
                     base::Unretained(this)));

  base::RunLoop().Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(bluetooth_device::kErrorAuthenticationRejected, last_client_error_);

  // No changes should be observer.
  EXPECT_EQ(0, observer.device_changed_count());

  EXPECT_FALSE(device->IsPaired());

  // No pairing context should remain on the device.
  BluetoothDeviceBlueZ* device_bluez =
      static_cast<BluetoothDeviceBlueZ*>(device);
  EXPECT_TRUE(device_bluez->GetPairing() == nullptr);
}

TEST_F(BluetoothBlueZTest, IncomingPairJustWorksWithoutDelegate) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  // Uses just-works pairing and thus requires authorization for incoming
  // pairings, without a pairing delegate, that will be rejected.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kJustWorksPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kJustWorksAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kJustWorksPath), true,
      GetCallback(),
      base::BindOnce(&BluetoothBlueZTest::DBusErrorCallback,
                     base::Unretained(this)));

  base::RunLoop().Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(bluetooth_device::kErrorAuthenticationRejected, last_client_error_);

  // No changes should be observer.
  EXPECT_EQ(0, observer.device_changed_count());

  EXPECT_FALSE(device->IsPaired());

  // No pairing context should remain on the device.
  BluetoothDeviceBlueZ* device_bluez =
      static_cast<BluetoothDeviceBlueZ*>(device);
  EXPECT_TRUE(device_bluez->GetPairing() == nullptr);
}

TEST_F(BluetoothBlueZTest, RemovePairingDelegateDuringPairing) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  TestPairingDelegate pairing_delegate;
  adapter_->AddPairingDelegate(
      &pairing_delegate, BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);

  // Requests that we provide a Passkey.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kRequestPasskeyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kRequestPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kRequestPasskeyPath),
      true, GetCallback(),
      base::BindOnce(&BluetoothBlueZTest::DBusErrorCallback,
                     base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_passkey_count_);

  // A pairing context should now be set on the device.
  BluetoothDeviceBlueZ* device_bluez =
      static_cast<BluetoothDeviceBlueZ*>(device);
  ASSERT_TRUE(device_bluez->GetPairing() != nullptr);

  // Removing the pairing delegate should remove that pairing context.
  adapter_->RemovePairingDelegate(&pairing_delegate);

  EXPECT_TRUE(device_bluez->GetPairing() == nullptr);

  // Set the Passkey, this should now have no effect since the pairing has
  // been, in-effect, cancelled
  device->SetPasskey(1234);

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(0, observer.device_changed_count());

  EXPECT_FALSE(device->IsPaired());
}

TEST_F(BluetoothBlueZTest, DeviceId) {
  GetAdapter();

  // Use the built-in paired device for this test, grab its Properties
  // structure so we can adjust the underlying modalias property.
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));

  ASSERT_TRUE(device != nullptr);
  ASSERT_TRUE(properties != nullptr);

  // Valid USB IF-assigned identifier.
  ASSERT_EQ("usb:v05ACp030Dd0306", properties->modalias.value());

  EXPECT_EQ(BluetoothDevice::VENDOR_ID_USB, device->GetVendorIDSource());
  EXPECT_EQ(0x05ac, device->GetVendorID());
  EXPECT_EQ(0x030d, device->GetProductID());
  EXPECT_EQ(0x0306, device->GetDeviceID());

  // Valid Bluetooth SIG-assigned identifier.
  properties->modalias.ReplaceValue("bluetooth:v00E0p2400d0400");

  EXPECT_EQ(BluetoothDevice::VENDOR_ID_BLUETOOTH, device->GetVendorIDSource());
  EXPECT_EQ(0x00e0, device->GetVendorID());
  EXPECT_EQ(0x2400, device->GetProductID());
  EXPECT_EQ(0x0400, device->GetDeviceID());

  // Invalid USB IF-assigned identifier.
  properties->modalias.ReplaceValue("usb:x00E0p2400d0400");

  EXPECT_EQ(BluetoothDevice::VENDOR_ID_UNKNOWN, device->GetVendorIDSource());
  EXPECT_EQ(0, device->GetVendorID());
  EXPECT_EQ(0, device->GetProductID());
  EXPECT_EQ(0, device->GetDeviceID());

  // Invalid Bluetooth SIG-assigned identifier.
  properties->modalias.ReplaceValue("bluetooth:x00E0p2400d0400");

  EXPECT_EQ(BluetoothDevice::VENDOR_ID_UNKNOWN, device->GetVendorIDSource());
  EXPECT_EQ(0, device->GetVendorID());
  EXPECT_EQ(0, device->GetProductID());
  EXPECT_EQ(0, device->GetDeviceID());

  // Unknown vendor specification identifier.
  properties->modalias.ReplaceValue("chrome:v00E0p2400d0400");

  EXPECT_EQ(BluetoothDevice::VENDOR_ID_UNKNOWN, device->GetVendorIDSource());
  EXPECT_EQ(0, device->GetVendorID());
  EXPECT_EQ(0, device->GetProductID());
  EXPECT_EQ(0, device->GetDeviceID());
}

TEST_F(BluetoothBlueZTest, GetConnectionInfoForDisconnectedDevice) {
  GetAdapter();
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);

  // Calling GetConnectionInfo for an unconnected device should return a result
  // in which all fields are filled with BluetoothDevice::kUnknownPower.
  BluetoothDevice::ConnectionInfo conn_info(0, 0, 0);
  device->GetConnectionInfo(base::Bind(&SaveConnectionInfo, &conn_info));
  int unknown_power = BluetoothDevice::kUnknownPower;
  EXPECT_NE(0, unknown_power);
  EXPECT_EQ(unknown_power, conn_info.rssi);
  EXPECT_EQ(unknown_power, conn_info.transmit_power);
  EXPECT_EQ(unknown_power, conn_info.max_transmit_power);
}

TEST_F(BluetoothBlueZTest, GetConnectionInfoForConnectedDevice) {
  GetAdapter();
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);

  device->Connect(nullptr, GetCallback(),
                  base::BindOnce(&BluetoothBlueZTest::ConnectErrorCallback,
                                 base::Unretained(this)));
  EXPECT_TRUE(device->IsConnected());

  // Calling GetConnectionInfo for a connected device should return valid
  // results.
  fake_bluetooth_device_client_->UpdateConnectionInfo(-10, 3, 4);
  BluetoothDevice::ConnectionInfo conn_info;
  device->GetConnectionInfo(base::Bind(&SaveConnectionInfo, &conn_info));
  EXPECT_EQ(-10, conn_info.rssi);
  EXPECT_EQ(3, conn_info.transmit_power);
  EXPECT_EQ(4, conn_info.max_transmit_power);

  // Pause discovery to connect without pair.
  EXPECT_EQ(1, fake_bluetooth_adapter_client_->GetPauseCount());
  EXPECT_EQ(1, fake_bluetooth_adapter_client_->GetUnpauseCount());
}

TEST_F(BluetoothBlueZTest, GetDiscoverableTimeout) {
  constexpr uint32_t kShortDiscoverableTimeout = 30;
  constexpr uint32_t kLongDiscoverableTimeout = 240;
  GetAdapter();
  BluetoothAdapterBlueZ* adapter_bluez =
      static_cast<BluetoothAdapterBlueZ*>(adapter_.get());

  fake_bluetooth_adapter_client_->SetDiscoverableTimeout(
      kShortDiscoverableTimeout);
  EXPECT_EQ(kShortDiscoverableTimeout, adapter_bluez->GetDiscoverableTimeout());

  fake_bluetooth_adapter_client_->SetDiscoverableTimeout(
      kLongDiscoverableTimeout);
  EXPECT_EQ(kLongDiscoverableTimeout, adapter_bluez->GetDiscoverableTimeout());
}

// Verifies Shutdown shuts down the adapter as expected.
TEST_F(BluetoothBlueZTest, Shutdown) {
  // Set up adapter. Set powered & discoverable, start discovery.
  GetAdapter();
  fake_bluetooth_adapter_client_->SetUUIDs(
      {kGapUuid, kGattUuid, kPnpUuid, kHeadsetUuid});
  adapter_->SetPowered(true, GetCallback(), GetErrorCallback());
  adapter_->SetDiscoverable(true, GetCallback(), GetErrorCallback());
  adapter_->StartDiscoverySession(
      base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallback,
                     base::Unretained(this)),
      GetErrorCallback());
  base::RunLoop().Run();
  ASSERT_EQ(3, callback_count_);
  ASSERT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  TestPairingDelegate pairing_delegate;
  adapter_->AddPairingDelegate(
      &pairing_delegate, BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);

  // Validate running adapter state.
  EXPECT_NE("", adapter_->GetAddress());
  EXPECT_NE("", adapter_->GetName());
  EXPECT_EQ(4U, adapter_->GetUUIDs().size());
  EXPECT_TRUE(adapter_->IsInitialized());
  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_TRUE(adapter_->IsPowered());
  EXPECT_TRUE(adapter_->IsDiscoverable());
  EXPECT_TRUE(IsAdapterDiscovering());
  EXPECT_EQ(2U, adapter_->GetDevices().size());
  EXPECT_NE(nullptr,
            adapter_->GetDevice(
                bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress));
  EXPECT_NE(dbus::ObjectPath(""),
            static_cast<BluetoothAdapterBlueZ*>(adapter_.get())->object_path());

  // Shutdown
  adapter_->Shutdown();

  // Validate post shutdown state by calling all BluetoothAdapterBlueZ
  // members, in declaration order:

  // DeleteOnCorrectThread omitted as we don't want to delete in this test.
  {
    TestBluetoothAdapterObserver observer(adapter_);  // Calls AddObserver
  }  // ~TestBluetoothAdapterObserver calls RemoveObserver.
  EXPECT_EQ("", adapter_->GetAddress());
  EXPECT_EQ("", adapter_->GetName());
  EXPECT_EQ(0U, adapter_->GetUUIDs().size());

  adapter_->SetName("", GetCallback(), GetErrorCallback());
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_--) << "SetName error";

  EXPECT_TRUE(adapter_->IsInitialized());
  EXPECT_FALSE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->IsPowered());

  adapter_->SetPowered(true, GetCallback(), GetErrorCallback());
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_--) << "SetPowered error";

  EXPECT_FALSE(adapter_->IsDiscoverable());

  adapter_->SetDiscoverable(true, GetCallback(), GetErrorCallback());
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_--) << "SetDiscoverable error";

  EXPECT_FALSE(IsAdapterDiscovering());
  // CreateRfcommService will DCHECK after Shutdown().
  // CreateL2capService will DCHECK after Shutdown().

  BluetoothAdapterBlueZ* adapter_bluez =
      static_cast<BluetoothAdapterBlueZ*>(adapter_.get());
  EXPECT_EQ(nullptr, adapter_bluez->GetDeviceWithPath(dbus::ObjectPath("")));

  // Notify methods presume objects exist that are owned by the adapter and
  // destroyed in Shutdown(). Mocks are not attempted here that won't exist,
  // as verified below by EXPECT_EQ(0U, adapter_->GetDevices().size());
  // NotifyDeviceChanged
  // NotifyGattServiceAdded
  // NotifyGattServiceRemoved
  // NotifyGattServiceChanged
  // NotifyGattDiscoveryComplete
  // NotifyGattCharacteristicAdded
  // NotifyGattCharacteristicRemoved
  // NotifyGattDescriptorAdded
  // NotifyGattDescriptorRemoved
  // NotifyGattCharacteristicValueChanged
  // NotifyGattDescriptorValueChanged

  EXPECT_EQ(dbus::ObjectPath(""), adapter_bluez->object_path());

  adapter_profile_ = nullptr;

  FakeBluetoothProfileServiceProviderDelegate profile_delegate;
  adapter_bluez->UseProfile(
      BluetoothUUID(), dbus::ObjectPath(""),
      bluez::BluetoothProfileManagerClient::Options(), &profile_delegate,
      base::Bind(&BluetoothBlueZTest::ProfileRegisteredCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothBlueZTest::ErrorCompletionCallback,
                 base::Unretained(this)));

  EXPECT_FALSE(adapter_profile_) << "UseProfile error";
  EXPECT_EQ(0, callback_count_) << "UseProfile error";
  EXPECT_EQ(1, error_callback_count_--) << "UseProfile error";

  // Protected and private methods:

  adapter_bluez->RemovePairingDelegateInternal(&pairing_delegate);
  // AdapterAdded() invalid post Shutdown because it calls SetAdapter.
  adapter_bluez->AdapterRemoved(dbus::ObjectPath("x"));
  adapter_bluez->AdapterPropertyChanged(dbus::ObjectPath("x"), "");
  adapter_bluez->DeviceAdded(dbus::ObjectPath(""));
  adapter_bluez->DeviceRemoved(dbus::ObjectPath(""));
  adapter_bluez->DevicePropertyChanged(dbus::ObjectPath(""), "");
  adapter_bluez->InputPropertyChanged(dbus::ObjectPath(""), "");
  // bluez::BluetoothAgentServiceProvider::Delegate omitted, dbus will be
  // shutdown,
  //   with the exception of Released.
  adapter_bluez->Released();

  adapter_bluez->OnRegisterAgent();
  adapter_bluez->OnRegisterAgentError("", "");
  adapter_bluez->OnRequestDefaultAgent();
  adapter_bluez->OnRequestDefaultAgentError("", "");

  // GetPairing will DCHECK after Shutdown().
  // SetAdapter will DCHECK after Shutdown().
  // SetDefaultAdapterName will DCHECK after Shutdown().
  adapter_bluez->NotifyAdapterPoweredChanged(false);
  adapter_bluez->DiscoverableChanged(false);
  adapter_bluez->DiscoveringChanged(false);
  adapter_bluez->PresentChanged(false);

  adapter_bluez->OnSetDiscoverable(GetCallback(), GetErrorCallback(), true);
  EXPECT_EQ(0, callback_count_) << "OnSetDiscoverable error";
  EXPECT_EQ(1, error_callback_count_--) << "OnSetDiscoverable error";

  adapter_bluez->OnPropertyChangeCompleted(GetCallback(), GetErrorCallback(),
                                           true);
  EXPECT_EQ(0, callback_count_) << "OnPropertyChangeCompleted error";
  EXPECT_EQ(1, error_callback_count_--) << "OnPropertyChangeCompleted error";

  adapter_bluez->StartDiscoverySession(
      base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallback,
                     base::Unretained(this)),
      GetErrorCallback());
  EXPECT_EQ(0, callback_count_) << "AddDiscoverySession error";
  EXPECT_EQ(1, error_callback_count_--) << "AddDiscoverySession error";

  // OnStartDiscovery tested in Shutdown_OnStartDiscovery
  // OnStartDiscoveryError tested in Shutdown_OnStartDiscoveryError

  adapter_profile_ = nullptr;

  // OnRegisterProfile SetProfileDelegate, OnRegisterProfileError, require
  // UseProfile to be set first, do so again here just before calling them.
  adapter_bluez->UseProfile(
      BluetoothUUID(), dbus::ObjectPath(""),
      bluez::BluetoothProfileManagerClient::Options(), &profile_delegate,
      base::Bind(&BluetoothBlueZTest::ProfileRegisteredCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothBlueZTest::ErrorCompletionCallback,
                 base::Unretained(this)));

  EXPECT_FALSE(adapter_profile_) << "UseProfile error";
  EXPECT_EQ(0, callback_count_) << "UseProfile error";
  EXPECT_EQ(1, error_callback_count_--) << "UseProfile error";

  adapter_bluez->SetProfileDelegate(
      BluetoothUUID(), dbus::ObjectPath(""), &profile_delegate,
      base::Bind(&BluetoothBlueZTest::ProfileRegisteredCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothBlueZTest::ErrorCompletionCallback,
                 base::Unretained(this)));
  EXPECT_EQ(0, callback_count_) << "SetProfileDelegate error";
  EXPECT_EQ(1, error_callback_count_--) << "SetProfileDelegate error";

  adapter_bluez->OnRegisterProfileError(BluetoothUUID(), "", "");
  EXPECT_EQ(0, callback_count_) << "OnRegisterProfileError error";
  EXPECT_EQ(0, error_callback_count_) << "OnRegisterProfileError error";

  // From BluetoothAdapater:

  adapter_->StartDiscoverySession(
      base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallback,
                     base::Unretained(this)),
      GetErrorCallback());
  EXPECT_EQ(0, callback_count_) << "StartDiscoverySession error";
  EXPECT_EQ(1, error_callback_count_--) << "StartDiscoverySession error";

  EXPECT_EQ(0U, adapter_->GetDevices().size());
  EXPECT_EQ(nullptr,
            adapter_->GetDevice(
                bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress));
  TestPairingDelegate pairing_delegate2;
  adapter_->AddPairingDelegate(
      &pairing_delegate2, BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);
  adapter_->RemovePairingDelegate(&pairing_delegate2);
}

TEST_F(BluetoothBlueZTest, StartDiscovery_DiscoveringStopped_StartAgain) {
  GetAdapter();
  BluetoothAdapterBlueZ* adapter_bluez =
      static_cast<BluetoothAdapterBlueZ*>(adapter_.get());
  {
    base::RunLoop loop;
    adapter_->StartDiscoverySession(
        base::BindLambdaForTesting(
            [&](std::unique_ptr<BluetoothDiscoverySession> session) {
              loop.Quit();
            }),
        base::BindOnce([]() {
          ADD_FAILURE() << "Unexpected discovery session start error.";
        }));
    loop.Run();
  }
  adapter_bluez->DiscoveringChanged(false);
  {
    base::RunLoop loop;
    adapter_->StartDiscoverySession(
        base::BindLambdaForTesting(
            [&](std::unique_ptr<BluetoothDiscoverySession> session) {
              loop.Quit();
            }),
        base::BindOnce([]() {
          ADD_FAILURE() << "Unexpected discovery session start error.";
        }));
    loop.Run();
  }
}

// Verifies post-Shutdown of discovery sessions and OnStartDiscovery.
TEST_F(BluetoothBlueZTest, Shutdown_OnStartDiscovery) {
  const int kNumberOfDiscoverySessions = 10;
  GetAdapter();
  BluetoothAdapterBlueZ* adapter_bluez =
      static_cast<BluetoothAdapterBlueZ*>(adapter_.get());

  for (int i = 0; i < kNumberOfDiscoverySessions; i++) {
    adapter_bluez->StartDiscoverySession(
        base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallback,
                       base::Unretained(this)),
        GetErrorCallback());
  }
  adapter_->Shutdown();
  base::RunLoop().Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(kNumberOfDiscoverySessions, error_callback_count_);
}

// Verifies post-Shutdown of discovery sessions and OnStartDiscoveryError.
TEST_F(BluetoothBlueZTest, Shutdown_OnStartDiscoveryError) {
  const int kNumberOfDiscoverySessions = 10;
  GetAdapter();
  BluetoothAdapterBlueZ* adapter_bluez =
      static_cast<BluetoothAdapterBlueZ*>(adapter_.get());

  for (int i = 0; i < kNumberOfDiscoverySessions; i++) {
    adapter_bluez->StartDiscoverySession(
        base::BindOnce(&BluetoothBlueZTest::DiscoverySessionCallback,
                       base::Unretained(this)),
        GetErrorCallback());
  }
  adapter_->Shutdown();
  base::RunLoop().Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(kNumberOfDiscoverySessions, error_callback_count_);
}

TEST_F(BluetoothBlueZTest, StartDiscoveryError_ThenStartAgain) {
  GetAdapter();
  fake_bluetooth_adapter_client_->MakeStartDiscoveryFail();

  {
    base::RunLoop loop;
    adapter_->StartDiscoverySession(
        base::BindOnce([](std::unique_ptr<BluetoothDiscoverySession> session) {
          ADD_FAILURE() << "Unexpected discovery session start success.";
        }),
        loop.QuitClosure());
    loop.Run();
  }

  {
    base::RunLoop loop;
    adapter_->StartDiscoverySession(
        base::BindLambdaForTesting(
            [&](std::unique_ptr<BluetoothDiscoverySession> session) {
              loop.Quit();
            }),
        base::BindOnce([]() {
          ADD_FAILURE() << "Unexpected discovery session start error.";
        }));
    loop.Run();
  }
}

TEST_F(BluetoothBlueZTest, ManufacturerDataChanged) {
  const BluetoothDevice::ManufacturerId kManufacturerId1 = 0x1234;
  const BluetoothDevice::ManufacturerId kManufacturerId2 = 0x2345;
  const BluetoothDevice::ManufacturerId kManufacturerId3 = 0x3456;

  // Simulate a change of manufacturer data of a device.
  GetAdapter();

  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);

  // Install an observer; expect the DeviceChanged method to be called
  // when we change the service data.
  TestBluetoothAdapterObserver observer(adapter_);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));

  properties->manufacturer_data.set_valid(true);

  // Check that ManufacturerDataChanged is correctly invoke.
  properties->manufacturer_data.ReplaceValue({{kManufacturerId1, {1, 2, 3}}});
  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());
  EXPECT_EQ(
      BluetoothDevice::ManufacturerDataMap({{kManufacturerId1, {1, 2, 3}}}),
      device->GetManufacturerData());
  EXPECT_EQ(BluetoothDevice::ManufacturerIDSet({kManufacturerId1}),
            device->GetManufacturerDataIDs());
  EXPECT_EQ(std::vector<uint8_t>({1, 2, 3}),
            *(device->GetManufacturerDataForID(kManufacturerId1)));

  // Check that we can update service data with same uuid / add more uuid.
  properties->manufacturer_data.ReplaceValue(
      {{kManufacturerId1, {3, 2, 1}}, {kManufacturerId2, {1}}});
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_EQ(BluetoothDevice::ManufacturerDataMap(
                {{kManufacturerId1, {3, 2, 1}}, {kManufacturerId2, {1}}}),
            device->GetManufacturerData());
  EXPECT_EQ(
      BluetoothDevice::ManufacturerIDSet({kManufacturerId1, kManufacturerId2}),
      device->GetManufacturerDataIDs());
  EXPECT_EQ(std::vector<uint8_t>({3, 2, 1}),
            *(device->GetManufacturerDataForID(kManufacturerId1)));
  EXPECT_EQ(std::vector<uint8_t>({1}),
            *(device->GetManufacturerDataForID(kManufacturerId2)));

  // Check that we can remove uuid / change uuid with same data.
  properties->manufacturer_data.ReplaceValue({{kManufacturerId3, {3, 2, 1}}});
  EXPECT_EQ(3, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_EQ(
      BluetoothDevice::ManufacturerDataMap({{kManufacturerId3, {3, 2, 1}}}),
      device->GetManufacturerData());
  EXPECT_EQ(BluetoothDevice::ManufacturerIDSet({kManufacturerId3}),
            device->GetManufacturerDataIDs());
  EXPECT_EQ(std::vector<uint8_t>({3, 2, 1}),
            *(device->GetManufacturerDataForID(kManufacturerId3)));
  EXPECT_EQ(nullptr, device->GetManufacturerDataForID(kManufacturerId1));
  EXPECT_EQ(nullptr, device->GetManufacturerDataForID(kManufacturerId2));
}

TEST_F(BluetoothBlueZTest, AdvertisingDataFlagsChanged) {
  // Simulate a change of advertising data flags of a device.
  GetAdapter();

  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);

  // Install an observer; expect the DeviceChanged method to be called
  // when we change the service data.
  TestBluetoothAdapterObserver observer(adapter_);

  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kPairedDevicePath));

  properties->advertising_data_flags.set_valid(true);

  // Check that AdvertisingDataFlagsChanged is correctly invoke.
  properties->advertising_data_flags.ReplaceValue(std::vector<uint8_t>({0x12}));
  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());
  EXPECT_TRUE(device->GetAdvertisingDataFlags().has_value());
  EXPECT_EQ(0x12u, device->GetAdvertisingDataFlags().value());

  // Check that we can update advertising data flags.
  properties->advertising_data_flags.ReplaceValue(std::vector<uint8_t>({0x23}));
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());
  EXPECT_TRUE(device->GetAdvertisingDataFlags().has_value());
  EXPECT_EQ(0x23u, device->GetAdvertisingDataFlags().value());
}

TEST_F(BluetoothBlueZTest, SetConnectionLatency) {
  GetAdapter();
  DiscoverDevices();

  // SetConnectionLatency is supported on LE devices.
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(
          bluez::FakeBluetoothDeviceClient::kConnectUnpairablePath));
  properties->type.ReplaceValue(BluetoothDeviceClient::kTypeLe);
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kConnectUnpairableAddress);
  ASSERT_TRUE(device);

  device->SetConnectionLatency(
      BluetoothDevice::ConnectionLatency::CONNECTION_LATENCY_LOW, GetCallback(),
      GetErrorCallback());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  device->SetConnectionLatency(
      BluetoothDevice::ConnectionLatency::CONNECTION_LATENCY_HIGH,
      GetCallback(), GetErrorCallback());
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Dual mode devices should be supported as well.
  properties->type.ReplaceValue(BluetoothDeviceClient::kTypeDual);
  device->SetConnectionLatency(
      BluetoothDevice::ConnectionLatency::CONNECTION_LATENCY_MEDIUM,
      GetCallback(), GetErrorCallback());
  EXPECT_EQ(3, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // This API is not supported for BR/EDR devices.
  properties->type.ReplaceValue(BluetoothDeviceClient::kTypeBredr);
  device->SetConnectionLatency(
      BluetoothDevice::ConnectionLatency::CONNECTION_LATENCY_MEDIUM,
      GetCallback(), GetErrorCallback());
  EXPECT_EQ(3, callback_count_);
  EXPECT_EQ(1, error_callback_count_);

  // Return an error if the type is not valid.
  properties->type.set_valid(false);
  device->SetConnectionLatency(
      BluetoothDevice::ConnectionLatency::CONNECTION_LATENCY_MEDIUM,
      GetCallback(), GetErrorCallback());
  EXPECT_EQ(3, callback_count_);
  EXPECT_EQ(2, error_callback_count_);
}

}  // namespace bluez
