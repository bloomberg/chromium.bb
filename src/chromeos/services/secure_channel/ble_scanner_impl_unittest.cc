// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_scanner_impl.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/test/bind_test_util.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/services/secure_channel/ble_constants.h"
#include "chromeos/services/secure_channel/connection_role.h"
#include "chromeos/services/secure_channel/fake_ble_scanner.h"
#include "chromeos/services/secure_channel/fake_ble_service_data_helper.h"
#include "chromeos/services/secure_channel/fake_ble_synchronizer.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace secure_channel {

namespace {

// Extends device::MockBluetoothDevice, adding the ability to set service data
// to be returned.
class FakeBluetoothDevice : public device::MockBluetoothDevice {
 public:
  FakeBluetoothDevice(const std::string& service_data,
                      device::MockBluetoothAdapter* adapter)
      : device::MockBluetoothDevice(adapter,
                                    0u /* bluetooth_class */,
                                    "name",
                                    "address",
                                    false /* paired */,
                                    false /* connected */) {
    // Convert |service_data| from a std::string to a std::vector<uint8_t>.
    std::transform(service_data.begin(), service_data.end(),
                   std::back_inserter(service_data_vector_),
                   [](char character) { return character; });
  }

  const std::vector<uint8_t>* service_data() { return &service_data_vector_; }

 private:
  std::vector<uint8_t> service_data_vector_;

  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothDevice);
};

// const size_t kMinNumBytesInServiceData = 2;

}  // namespace

class SecureChannelBleScannerImplTest : public testing::Test {
 protected:
  class FakeServiceDataProvider : public BleScannerImpl::ServiceDataProvider {
   public:
    FakeServiceDataProvider() = default;
    ~FakeServiceDataProvider() override = default;

    // ServiceDataProvider:
    const std::vector<uint8_t>* ExtractProximityAuthServiceData(
        device::BluetoothDevice* bluetooth_device) override {
      FakeBluetoothDevice* mock_device =
          static_cast<FakeBluetoothDevice*>(bluetooth_device);
      return mock_device->service_data();
    }
  };

  SecureChannelBleScannerImplTest()
      : test_devices_(multidevice::CreateRemoteDeviceRefListForTest(3)) {}
  ~SecureChannelBleScannerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_delegate_ = std::make_unique<FakeBleScannerDelegate>();
    fake_ble_service_data_helper_ =
        std::make_unique<FakeBleServiceDataHelper>();
    fake_ble_synchronizer_ = std::make_unique<FakeBleSynchronizer>();

    mock_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();

    ble_scanner_ = BleScannerImpl::Factory::Get()->BuildInstance(
        fake_delegate_.get(), fake_ble_service_data_helper_.get(),
        fake_ble_synchronizer_.get(), mock_adapter_);

    auto fake_service_data_provider =
        std::make_unique<FakeServiceDataProvider>();
    fake_service_data_provider_ = fake_service_data_provider.get();

    BleScannerImpl* ble_scanner_derived =
        static_cast<BleScannerImpl*>(ble_scanner_.get());
    ble_scanner_derived->SetServiceDataProviderForTesting(
        std::move(fake_service_data_provider));

    ON_CALL(*mock_adapter_, StartScanWithFilter_(testing::_, testing::_))
        .WillByDefault(testing::Invoke(
            [](const device::BluetoothDiscoveryFilter* discovery_filter,
               device::BluetoothAdapter::DiscoverySessionResultCallback&
                   callback) {
              std::move(callback).Run(
                  /*is_error=*/false,
                  device::UMABluetoothDiscoverySessionOutcome::SUCCESS);
            }));
    ON_CALL(*mock_adapter_, StopScan(testing::_))
        .WillByDefault(testing::Invoke(
            [](device::BluetoothAdapter::DiscoverySessionResultCallback
                   callback) {
              std::move(callback).Run(
                  /*is_error=*/false,
                  device::UMABluetoothDiscoverySessionOutcome::SUCCESS);
            }));
  }

  void AddScanFilter(const BleScanner::ScanFilter& scan_filter) {
    EXPECT_FALSE(ble_scanner_->HasScanFilter(scan_filter));
    ble_scanner_->AddScanFilter(scan_filter);
    EXPECT_TRUE(ble_scanner_->HasScanFilter(scan_filter));
  }

  // StartDiscoverySession in the mock adapter mostly for the purpose of
  // creating a DiscoverySession.
  void StartDiscoverySession() {
    mock_adapter_->StartDiscoverySession(
        base::BindLambdaForTesting(
            [&](std::unique_ptr<device::BluetoothDiscoverySession>
                    discovery_session) {
              discovery_session_ = std::move(discovery_session);
              discovery_session_weak_ptr_ = discovery_session_->GetWeakPtr();
            }),
        base::RepeatingClosure());
  }

  void RemoveScanFilter(const BleScanner::ScanFilter& scan_filter) {
    EXPECT_TRUE(ble_scanner_->HasScanFilter(scan_filter));
    ble_scanner_->RemoveScanFilter(scan_filter);
    EXPECT_FALSE(ble_scanner_->HasScanFilter(scan_filter));
  }

  void ProcessScanResultAndVerifyNoDeviceIdentified(
      const std::string& service_data) {
    const FakeBleScannerDelegate::ScannedResultList& results =
        fake_delegate_->handled_scan_results();

    size_t num_results_before_call = results.size();
    SimulateScanResult(service_data);
    EXPECT_EQ(num_results_before_call, results.size());
  }

  void ProcessScanResultAndVerifyDevice(
      const std::string& service_data,
      multidevice::RemoteDeviceRef expected_remote_device,
      bool is_background_advertisement) {
    const FakeBleScannerDelegate::ScannedResultList& results =
        fake_delegate_->handled_scan_results();

    fake_ble_service_data_helper_->SetIdentifiedDevice(
        service_data, expected_remote_device, is_background_advertisement);

    size_t num_results_before_call = results.size();
    std::unique_ptr<FakeBluetoothDevice> fake_bluetooth_device =
        SimulateScanResult(service_data);
    EXPECT_EQ(num_results_before_call + 1u, results.size());

    EXPECT_EQ(expected_remote_device, std::get<0>(results.back()));
    EXPECT_EQ(fake_bluetooth_device.get(), std::get<1>(results.back()));
    EXPECT_EQ(is_background_advertisement ? ConnectionRole::kListenerRole
                                          : ConnectionRole::kInitiatorRole,
              std::get<2>(results.back()));
  }

  void InvokeStartDiscoveryCallback(bool success, size_t command_index) {
    if (!success) {
      fake_ble_synchronizer_->TakeStartDiscoveryErrorCallback(command_index)
          .Run();
      return;
    }

    StartDiscoverySession();
    fake_ble_synchronizer_->TakeStartDiscoveryCallback(command_index)
        .Run(std::move(discovery_session_));
  }

  void InvokeStopDiscoveryCallback(bool success, size_t command_index) {
    if (success) {
      fake_ble_synchronizer_->GetStopDiscoveryCallback(command_index).Run();
    } else {
      fake_ble_synchronizer_->GetStopDiscoveryErrorCallback(command_index)
          .Run();
    }
  }

  size_t GetNumBleCommands() {
    return fake_ble_synchronizer_->GetNumCommands();
  }

  bool discovery_session_is_active() {
    return discovery_session_weak_ptr_.get();
  }

  FakeBleServiceDataHelper* fake_ble_service_data_helper() {
    return fake_ble_service_data_helper_.get();
  }

  const multidevice::RemoteDeviceRefList& test_devices() {
    return test_devices_;
  }

 private:
  std::unique_ptr<FakeBluetoothDevice> SimulateScanResult(
      const std::string& service_data) {
    static const int16_t kFakeRssi = -70;
    static const std::vector<uint8_t> kFakeEir;

    // Scan result should not be received if there is no active discovery
    // session.
    EXPECT_TRUE(discovery_session_is_active());

    auto fake_bluetooth_device = std::make_unique<FakeBluetoothDevice>(
        service_data, mock_adapter_.get());

    // Note: MockBluetoothAdapter provides no way to notify observers, so the
    // observer callback must be invoked directly.
    for (auto& observer : mock_adapter_->GetObservers()) {
      observer.DeviceAdvertisementReceived(mock_adapter_.get(),
                                           fake_bluetooth_device.get(),
                                           kFakeRssi, kFakeEir);
    }

    return fake_bluetooth_device;
  }

  const multidevice::RemoteDeviceRefList test_devices_;

  std::unique_ptr<FakeBleScannerDelegate> fake_delegate_;
  std::unique_ptr<FakeBleServiceDataHelper> fake_ble_service_data_helper_;
  std::unique_ptr<FakeBleSynchronizer> fake_ble_synchronizer_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;

  std::unique_ptr<device::BluetoothDiscoverySession> discovery_session_;
  FakeServiceDataProvider* fake_service_data_provider_ = nullptr;
  base::WeakPtr<device::BluetoothDiscoverySession> discovery_session_weak_ptr_;

  std::unique_ptr<BleScanner> ble_scanner_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SecureChannelBleScannerImplTest);
};

TEST_F(SecureChannelBleScannerImplTest, UnrelatedScanResults) {
  BleScanner::ScanFilter filter(DeviceIdPair(test_devices()[0].GetDeviceId(),
                                             test_devices()[1].GetDeviceId()),
                                ConnectionRole::kListenerRole);

  AddScanFilter(filter);
  InvokeStartDiscoveryCallback(true /* success */, 0u /* command_index */);
  EXPECT_TRUE(discovery_session_is_active());

  ProcessScanResultAndVerifyNoDeviceIdentified("unrelatedServiceData");

  RemoveScanFilter(filter);
  InvokeStopDiscoveryCallback(true /* success */, 1u /* command_index */);
  EXPECT_FALSE(discovery_session_is_active());
}

TEST_F(SecureChannelBleScannerImplTest, IncorrectRole) {
  BleScanner::ScanFilter filter(DeviceIdPair(test_devices()[0].GetDeviceId(),
                                             test_devices()[1].GetDeviceId()),
                                ConnectionRole::kListenerRole);

  AddScanFilter(filter);
  InvokeStartDiscoveryCallback(true /* success */, 0u /* command_index */);
  EXPECT_TRUE(discovery_session_is_active());

  // Set the device to be a foreground advertisement, even though the registered
  // role is listener.
  fake_ble_service_data_helper()->SetIdentifiedDevice(
      "wrongRoleServiceData", test_devices()[0],
      false /* is_background_advertisement */);

  ProcessScanResultAndVerifyNoDeviceIdentified("wrongRoleServiceData");

  RemoveScanFilter(filter);
  InvokeStopDiscoveryCallback(true /* success */, 1u /* command_index */);
  EXPECT_FALSE(discovery_session_is_active());
}

TEST_F(SecureChannelBleScannerImplTest, IdentifyDevice_Background) {
  BleScanner::ScanFilter filter(DeviceIdPair(test_devices()[0].GetDeviceId(),
                                             test_devices()[1].GetDeviceId()),
                                ConnectionRole::kListenerRole);

  AddScanFilter(filter);
  InvokeStartDiscoveryCallback(true /* success */, 0u /* command_index */);
  EXPECT_TRUE(discovery_session_is_active());

  ProcessScanResultAndVerifyDevice("device0ServiceData", test_devices()[0],
                                   true /* is_background_advertisement */);

  RemoveScanFilter(filter);
  InvokeStopDiscoveryCallback(true /* success */, 1u /* command_index */);
  EXPECT_FALSE(discovery_session_is_active());
}

TEST_F(SecureChannelBleScannerImplTest, IdentifyDevice_Foreground) {
  BleScanner::ScanFilter filter(DeviceIdPair(test_devices()[0].GetDeviceId(),
                                             test_devices()[1].GetDeviceId()),
                                ConnectionRole::kInitiatorRole);

  AddScanFilter(filter);
  InvokeStartDiscoveryCallback(true /* success */, 0u /* command_index */);
  EXPECT_TRUE(discovery_session_is_active());

  ProcessScanResultAndVerifyDevice("device0ServiceData", test_devices()[0],
                                   false /* is_background_advertisement */);

  RemoveScanFilter(filter);
  InvokeStopDiscoveryCallback(true /* success */, 1u /* command_index */);
  EXPECT_FALSE(discovery_session_is_active());
}

TEST_F(SecureChannelBleScannerImplTest, IdentifyDevice_MultipleScans) {
  BleScanner::ScanFilter filter_1(DeviceIdPair(test_devices()[0].GetDeviceId(),
                                               test_devices()[1].GetDeviceId()),
                                  ConnectionRole::kInitiatorRole);
  BleScanner::ScanFilter filter_2(DeviceIdPair(test_devices()[2].GetDeviceId(),
                                               test_devices()[1].GetDeviceId()),
                                  ConnectionRole::kInitiatorRole);

  AddScanFilter(filter_1);
  AddScanFilter(filter_2);
  InvokeStartDiscoveryCallback(true /* success */, 0u /* command_index */);
  EXPECT_TRUE(discovery_session_is_active());

  // Identify device 0.
  ProcessScanResultAndVerifyDevice("device0ServiceData", test_devices()[0],
                                   false /* is_background_advertisement */);

  // Remove the identified device from the list of scan filters.
  RemoveScanFilter(filter_1);

  // No additional BLE command should have been posted, since the existing scan
  // should not have been stopped.
  EXPECT_EQ(1u, GetNumBleCommands());
  EXPECT_TRUE(discovery_session_is_active());

  // Remove the scan filter, and verify that the scan stopped.
  RemoveScanFilter(filter_2);
  InvokeStopDiscoveryCallback(true /* success */, 1u /* command_index */);
  EXPECT_FALSE(discovery_session_is_active());

  // Add the scan filter back again; this should start the discovery session
  // back up again.
  AddScanFilter(filter_2);
  InvokeStartDiscoveryCallback(true /* success */, 2u /* command_index */);
  EXPECT_TRUE(discovery_session_is_active());

  // Identify device 2.
  ProcessScanResultAndVerifyDevice("device2ServiceData", test_devices()[2],
                                   false /* is_background_advertisement */);

  // Remove the scan filter, and verify that the scan stopped.
  RemoveScanFilter(filter_2);
  InvokeStopDiscoveryCallback(true /* success */, 3u /* command_index */);
  EXPECT_FALSE(discovery_session_is_active());
}

TEST_F(SecureChannelBleScannerImplTest, StartAndStopFailures) {
  BleScanner::ScanFilter filter(DeviceIdPair(test_devices()[0].GetDeviceId(),
                                             test_devices()[1].GetDeviceId()),
                                ConnectionRole::kListenerRole);
  AddScanFilter(filter);

  // A request was made to start discovery; simulate this request failing.
  InvokeStartDiscoveryCallback(false /* success */, 0u /* command_index */);
  EXPECT_FALSE(discovery_session_is_active());

  // BleScanner should have retried this attempt; simulate another failure.
  InvokeStartDiscoveryCallback(false /* success */, 1u /* command_index */);
  EXPECT_FALSE(discovery_session_is_active());

  // Succeed this time.
  InvokeStartDiscoveryCallback(true /* success */, 2u /* command_index */);
  EXPECT_TRUE(discovery_session_is_active());

  // Remove scan filters, which should trigger BleScanner to stop the
  // discovery session.
  RemoveScanFilter(filter);

  // Simulate a failure to stop.
  InvokeStopDiscoveryCallback(false /* success */, 3u /* command_index */);
  EXPECT_TRUE(discovery_session_is_active());

  // Simulate another failure.
  InvokeStopDiscoveryCallback(false /* success */, 4u /* command_index */);
  EXPECT_TRUE(discovery_session_is_active());

  // Succeed this time.
  InvokeStopDiscoveryCallback(true /* success */, 5u /* command_index */);
  EXPECT_FALSE(discovery_session_is_active());
}

TEST_F(SecureChannelBleScannerImplTest, StartAndStop_EdgeCases) {
  BleScanner::ScanFilter filter(DeviceIdPair(test_devices()[0].GetDeviceId(),
                                             test_devices()[1].GetDeviceId()),
                                ConnectionRole::kListenerRole);
  AddScanFilter(filter);

  // Remove scan filters before the start discovery callback succeeds.
  RemoveScanFilter(filter);

  // Complete starting the discovery session.
  InvokeStartDiscoveryCallback(true /* success */, 0u /* command_index */);
  EXPECT_TRUE(discovery_session_is_active());

  // BleScanner should have realized that it should now stop the discovery
  // session. Invoke the pending stop discovery callback.
  InvokeStopDiscoveryCallback(true /* success */, 1u /* command_index */);
  EXPECT_FALSE(discovery_session_is_active());
}

TEST_F(SecureChannelBleScannerImplTest, StartAndStopFailures_EdgeCases) {
  BleScanner::ScanFilter filter(DeviceIdPair(test_devices()[0].GetDeviceId(),
                                             test_devices()[1].GetDeviceId()),
                                ConnectionRole::kListenerRole);
  AddScanFilter(filter);

  // Remove scan filters before the start discovery callback succeeds.
  RemoveScanFilter(filter);

  // Fail the pending call to start a discovery session.
  InvokeStartDiscoveryCallback(false /* success */, 0u /* command_index */);
  EXPECT_FALSE(discovery_session_is_active());

  // No additional BLE command should have been posted.
  EXPECT_EQ(1u, GetNumBleCommands());
}

}  // namespace secure_channel

}  // namespace chromeos
