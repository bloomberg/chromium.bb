// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/pcie_peripheral/pcie_peripheral_manager.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/dbus/pciguard/fake_pciguard_client.h"
#include "chromeos/dbus/pciguard/pciguard_client.h"
#include "chromeos/dbus/typecd/fake_typecd_client.h"
#include "chromeos/dbus/typecd/typecd_client.h"
#include "services/device/public/cpp/test/fake_usb_device_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const int kUsbConfigWithInterfaces = 1;
const int kBillboardDeviceClassCode = 17;
const int kNonBillboardDeviceClassCode = 16;
constexpr char thunderbolt_path_for_testing[] =
    "/tmp/tbt/sys/bus/thunderbolt/devices/0-0";
constexpr char root_prefix_for_testing[] = "/tmp/tbt";
}  // namespace

namespace ash {

class FakeObserver : public PciePeripheralManager::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_limited_performance_notification_calls() const {
    return num_limited_performance_notification_calls_;
  }

  size_t num_guest_notification_calls() const {
    return num_guest_notification_calls_;
  }

  size_t num_peripheral_blocked_notification_calls() const {
    return num_peripheral_blocked_notification_calls_;
  }

  size_t num_billboard_notification_calls() const {
    return num_billboard_notification_calls_;
  }

  bool is_current_guest_device_tbt_only() const {
    return is_current_guest_device_tbt_only_;
  }

  // PciePeripheralManager::Observer:
  void OnLimitedPerformancePeripheralReceived() override {
    ++num_limited_performance_notification_calls_;
  }

  void OnGuestModeNotificationReceived(bool is_thunderbolt_only) override {
    is_current_guest_device_tbt_only_ = is_thunderbolt_only;
    ++num_guest_notification_calls_;
  }

  void OnPeripheralBlockedReceived() override {
    ++num_peripheral_blocked_notification_calls_;
  }

  void OnBillboardDeviceConnected() override {
    ++num_billboard_notification_calls_;
  }

 private:
  size_t num_limited_performance_notification_calls_ = 0u;
  size_t num_guest_notification_calls_ = 0u;
  size_t num_peripheral_blocked_notification_calls_ = 0u;
  size_t num_billboard_notification_calls_ = 0u;
  bool is_current_guest_device_tbt_only_ = false;
};

class PciePeripheralManagerTest : public AshTestBase {
 protected:
  PciePeripheralManagerTest() = default;
  PciePeripheralManagerTest(const PciePeripheralManagerTest&) = delete;
  PciePeripheralManagerTest& operator=(const PciePeripheralManagerTest&) =
      delete;
  ~PciePeripheralManagerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();

    chromeos::TypecdClient::InitializeFake();
    fake_typecd_client_ =
        static_cast<chromeos::FakeTypecdClient*>(chromeos::TypecdClient::Get());

    chromeos::PciguardClient::InitializeFake();
    fake_pciguard_client_ = static_cast<chromeos::FakePciguardClient*>(
        chromeos::PciguardClient::Get());

    base::DeletePathRecursively(base::FilePath(thunderbolt_path_for_testing));
  }

  void InitializeManager(bool is_guest_session,
                         bool is_pcie_tunneling_allowed) {
    PciePeripheralManager::Initialize(is_guest_session,
                                      is_pcie_tunneling_allowed);
    manager_ = PciePeripheralManager::Get();

    manager_->AddObserver(&fake_observer_);
    manager_->SetRootPrefixForTesting(root_prefix_for_testing);
  }

  void TearDown() override {
    AshTestBase::TearDown();

    manager_->RemoveObserver(&fake_observer_);
    PciePeripheralManager::Shutdown();
    chromeos::TypecdClient::Shutdown();
    chromeos::PciguardClient::Shutdown();
    base::DeletePathRecursively(base::FilePath(thunderbolt_path_for_testing));
  }

  chromeos::FakeTypecdClient* fake_typecd_client() {
    return fake_typecd_client_;
  }

  chromeos::FakePciguardClient* fake_pciguard_client() {
    return fake_pciguard_client_;
  }

  size_t GetNumLimitedPerformanceObserverCalls() {
    return fake_observer_.num_limited_performance_notification_calls();
  }

  size_t GetNumGuestModeNotificationObserverCalls() {
    return fake_observer_.num_guest_notification_calls();
  }

  size_t GetNumPeripheralBlockedNotificationObserverCalls() {
    return fake_observer_.num_peripheral_blocked_notification_calls();
  }

  size_t GetNumBillboardNotificationObserverCalls() {
    return fake_observer_.num_billboard_notification_calls();
  }

  bool GetIsCurrentGuestDeviceTbtOnly() {
    return fake_observer_.is_current_guest_device_tbt_only();
  }

  base::HistogramTester histogram_tester_;

 private:
  chromeos::FakeTypecdClient* fake_typecd_client_;
  chromeos::FakePciguardClient* fake_pciguard_client_;
  PciePeripheralManager* manager_ = nullptr;
  FakeObserver fake_observer_;
};

scoped_refptr<device::FakeUsbDeviceInfo> CreateTestDeviceOfClass(
    uint8_t device_class) {
  auto config = device::mojom::UsbConfigurationInfo::New();
  config->configuration_value = kUsbConfigWithInterfaces;

  auto alternate = device::mojom::UsbAlternateInterfaceInfo::New();
  alternate->alternate_setting = 0;
  alternate->class_code = device_class;
  alternate->subclass_code = 0xff;
  alternate->protocol_code = 0xff;

  auto interface = device::mojom::UsbInterfaceInfo::New();
  interface->interface_number = 0;
  interface->alternates.push_back(std::move(alternate));

  config->interfaces.push_back(std::move(interface));

  std::vector<device::mojom::UsbConfigurationInfoPtr> configs;
  configs.push_back(std::move(config));

  scoped_refptr<device::FakeUsbDeviceInfo> device =
      base::MakeRefCounted<device::FakeUsbDeviceInfo>(
          /*vendor_id=*/0, /*product_id=*/1, device_class, std::move(configs));
  device->SetActiveConfig(kUsbConfigWithInterfaces);
  return device;
}

TEST_F(PciePeripheralManagerTest, InitialTest) {
  InitializeManager(/*is_guest_profile=*/false,
                    /*is_pcie_tunneling_allowed=*/false);
  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_EQ(0u, GetNumBillboardNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
}

TEST_F(PciePeripheralManagerTest, LimitedPerformanceNotification) {
  InitializeManager(/*is_guest_profile=*/false,
                    /*is_pcie_tunneling_allowed=*/false);

  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.PciePeripheral.ConnectivityResults",
      PciePeripheralManager::PciePeripheralConnectivityResults::
          kAltModeFallbackDueToPciguard,
      0);

  // Simulate emitting D-Bus signal.
  fake_typecd_client()->EmitThunderboltDeviceConnectedSignal(
      /*is_thunderbolt_only=*/false);
  // pcie tunneling is not allowed and a alt-mode device has been plugged in.
  // Expect the notification observer to be called.
  EXPECT_EQ(1u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.PciePeripheral.ConnectivityResults",
      PciePeripheralManager::PciePeripheralConnectivityResults::
          kAltModeFallbackDueToPciguard,
      1);
}

TEST_F(PciePeripheralManagerTest, NoNotificationShown) {
  InitializeManager(/*is_guest_profile=*/false,
                    /*is_pcie_tunneling_allowed=*/true);

  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.PciePeripheral.ConnectivityResults",
      PciePeripheralManager::PciePeripheralConnectivityResults::
          kTBTSupportedAndAllowed,
      0);

  // Simulate emitting D-Bus signal.
  fake_typecd_client()->EmitThunderboltDeviceConnectedSignal(
      /*is_thunderbolt_only=*/false);
  // Pcie tunneling allowed, we do not show any notifications for this case.
  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.PciePeripheral.ConnectivityResults",
      PciePeripheralManager::PciePeripheralConnectivityResults::
          kTBTSupportedAndAllowed,
      1);

  // Simulate emitting a new D-Bus signal, this time with |is_thunderbolt_only|
  // set to true.
  fake_typecd_client()->EmitThunderboltDeviceConnectedSignal(
      /*is_thunderbolt_only=*/true);
  // Pcie tunneling allowed, we do not show any notifications for this case.
  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  // No observer was called, therefore don't expect this to be updated.
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.PciePeripheral.ConnectivityResults",
      PciePeripheralManager::PciePeripheralConnectivityResults::
          kTBTSupportedAndAllowed,
      2);
}

TEST_F(PciePeripheralManagerTest, TBTOnlyAndBlockedByPciguard) {
  InitializeManager(/*is_guest_profile=*/false,
                    /*is_pcie_tunneling_allowed=*/false);

  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.PciePeripheral.ConnectivityResults",
      PciePeripheralManager::PciePeripheralConnectivityResults::
          kTBTOnlyAndBlockedByPciguard,
      0);

  // Simulate emitting D-Bus signal.
  fake_typecd_client()->EmitThunderboltDeviceConnectedSignal(
      /*is_thunderbolt_only=*/true);
  // Pcie tunneling allowed, we do not show any notifications for this case.
  EXPECT_EQ(1u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.PciePeripheral.ConnectivityResults",
      PciePeripheralManager::PciePeripheralConnectivityResults::
          kTBTOnlyAndBlockedByPciguard,
      1);
}

TEST_F(PciePeripheralManagerTest, GuestNotificationLimitedPerformance) {
  InitializeManager(/*is_guest_profile=*/true,
                    /*is_pcie_tunneling_allowed=*/false);

  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.PciePeripheral.ConnectivityResults",
      PciePeripheralManager::PciePeripheralConnectivityResults::
          kAltModeFallbackInGuestSession,
      0);

  // Simulate emitting D-Bus signal.
  fake_typecd_client()->EmitThunderboltDeviceConnectedSignal(
      /*is_thunderbolt_only=*/false);
  // Pcie tunneling not allowed and user is in guest session. The device
  // supports an alt-mode, expect the notification observer to be called.
  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(1u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.PciePeripheral.ConnectivityResults",
      PciePeripheralManager::PciePeripheralConnectivityResults::
          kAltModeFallbackInGuestSession,
      1);
}

TEST_F(PciePeripheralManagerTest, GuestNotificationRestricted) {
  InitializeManager(/*is_guest_profile=*/true,
                    /*is_pcie_tunneling_allowed=*/false);

  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.PciePeripheral.ConnectivityResults",
      PciePeripheralManager::PciePeripheralConnectivityResults::
          kTBTOnlyAndBlockedInGuestSession,
      0);

  // Simulate emitting D-Bus signal.
  fake_typecd_client()->EmitThunderboltDeviceConnectedSignal(
      /*is_thunderbolt_only=*/true);
  // Pcie tunneling not allowed and user is in guest session. The device
  // does not support alt-mode, expect the notification observer to be called.
  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(1u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_TRUE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.PciePeripheral.ConnectivityResults",
      PciePeripheralManager::PciePeripheralConnectivityResults::
          kTBTOnlyAndBlockedInGuestSession,
      1);
}

TEST_F(PciePeripheralManagerTest, BlockedDeviceReceived) {
  InitializeManager(/*is_guest_profile=*/false,
                    /*is_pcie_tunneling_allowed=*/true);

  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_EQ(0u, GetNumPeripheralBlockedNotificationObserverCalls());

  // Simulate emitting D-Bus signal for a blocked device received.
  fake_pciguard_client()->EmitDeviceBlockedSignal(/*device_name=*/"test");

  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_EQ(1u, GetNumPeripheralBlockedNotificationObserverCalls());

  histogram_tester_.ExpectBucketCount(
      "Ash.PciePeripheral.ConnectivityResults",
      PciePeripheralManager::PciePeripheralConnectivityResults::
          kPeripheralBlocked,
      1);
}

TEST_F(PciePeripheralManagerTest, BillboardDevice) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPcieBillboardNotification);

  InitializeManager(/*is_guest_profile=*/false,
                    /*is_pcie_tunneling_allowed=*/true);

  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_EQ(0u, GetNumPeripheralBlockedNotificationObserverCalls());
  EXPECT_EQ(0u, GetNumBillboardNotificationObserverCalls());

  // Simulate connecting a billboard device.
  const auto fake_device = CreateTestDeviceOfClass(kBillboardDeviceClassCode);
  const auto device = fake_device->GetDeviceInfo().Clone();
  PciePeripheralManager::Get()->OnDeviceConnected(device.get());

  task_environment()->RunUntilIdle();

  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_EQ(0u, GetNumPeripheralBlockedNotificationObserverCalls());
  EXPECT_EQ(1u, GetNumBillboardNotificationObserverCalls());

  histogram_tester_.ExpectBucketCount(
      "Ash.PciePeripheral.ConnectivityResults",
      PciePeripheralManager::PciePeripheralConnectivityResults::
          kBillboardDevice,
      1);

  // Connect a non-billboard device. There should be no notification.
  const auto fake_device_1 =
      CreateTestDeviceOfClass(kNonBillboardDeviceClassCode);
  const auto device_1 = fake_device_1->GetDeviceInfo().Clone();
  PciePeripheralManager::Get()->OnDeviceConnected(device_1.get());

  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_EQ(0u, GetNumPeripheralBlockedNotificationObserverCalls());
  EXPECT_EQ(1u, GetNumBillboardNotificationObserverCalls());

  // Fake a board that supports Thunderbolt.
  auto thunderbolt_directory = std::make_unique<base::ScopedTempDir>();
  EXPECT_TRUE(thunderbolt_directory->CreateUniqueTempDirUnderPath(
      base::FilePath(thunderbolt_path_for_testing)));

  // Connect a billboard device. There should be no notification.
  const auto fake_device_2 = CreateTestDeviceOfClass(kBillboardDeviceClassCode);
  const auto device_2 = fake_device_2->GetDeviceInfo().Clone();
  PciePeripheralManager::Get()->OnDeviceConnected(device_2.get());

  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_EQ(0u, GetNumPeripheralBlockedNotificationObserverCalls());
  EXPECT_EQ(1u, GetNumBillboardNotificationObserverCalls());

  histogram_tester_.ExpectBucketCount(
      "Ash.PciePeripheral.ConnectivityResults",
      PciePeripheralManager::PciePeripheralConnectivityResults::
          kBillboardDevice,
      1);
}

}  // namespace ash
