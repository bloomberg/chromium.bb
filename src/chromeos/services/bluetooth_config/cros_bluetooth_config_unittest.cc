// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/cros_bluetooth_config.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/services/bluetooth_config/bluetooth_power_controller_impl.h"
#include "chromeos/services/bluetooth_config/device_name_manager_impl.h"
#include "chromeos/services/bluetooth_config/fake_bluetooth_device_status_observer.h"
#include "chromeos/services/bluetooth_config/fake_bluetooth_discovery_delegate.h"
#include "chromeos/services/bluetooth_config/fake_fast_pair_delegate.h"
#include "chromeos/services/bluetooth_config/fake_system_properties_observer.h"
#include "chromeos/services/bluetooth_config/initializer_impl.h"
#include "components/session_manager/core/session_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace bluetooth_config {

// Tests initialization and bindings in for CrosBluetoothConfig. Note that this
// test is not meant to be exhaustive of the API, since these details are tested
// within the unit tests for the other classes owned by CrosBluetoothConfig.
class CrosBluetoothConfigTest : public testing::Test {
 protected:
  CrosBluetoothConfigTest() = default;
  CrosBluetoothConfigTest(const CrosBluetoothConfigTest&) = delete;
  CrosBluetoothConfigTest& operator=(const CrosBluetoothConfigTest&) = delete;
  ~CrosBluetoothConfigTest() override = default;

  // testing::Test:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(ash::features::kBluetoothRevamp);

    DeviceNameManagerImpl::RegisterLocalStatePrefs(
        test_pref_service_.registry());
    BluetoothPowerControllerImpl::RegisterLocalStatePrefs(
        test_pref_service_.registry());

    auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
    fake_user_manager_ = fake_user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

    mock_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    fake_fast_pair_delegate_ = std::make_unique<FakeFastPairDelegate>();
    InitializerImpl initializer;
    cros_bluetooth_config_ = std::make_unique<CrosBluetoothConfig>(
        initializer, mock_adapter_, fake_fast_pair_delegate_.get());
    cros_bluetooth_config_->SetPrefs(/*logged_in_profile_prefs=*/nullptr,
                                     &test_pref_service_);

    // CrosBluetoothConfig ctor should set the device name manager for the
    // delegate.
    EXPECT_TRUE(fake_fast_pair_delegate_->device_name_manager() != nullptr);
  }

  mojo::Remote<mojom::CrosBluetoothConfig> BindToInterface() {
    mojo::Remote<mojom::CrosBluetoothConfig> remote;
    cros_bluetooth_config_->BindPendingReceiver(
        remote.BindNewPipeAndPassReceiver());
    return remote;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  user_manager::FakeUserManager* fake_user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  session_manager::SessionManager session_manager_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  std::unique_ptr<FakeFastPairDelegate> fake_fast_pair_delegate_;
  sync_preferences::TestingPrefServiceSyncable test_pref_service_;

  std::unique_ptr<CrosBluetoothConfig> cros_bluetooth_config_;
};

TEST_F(CrosBluetoothConfigTest, BindMultipleClients) {
  mojo::Remote<mojom::CrosBluetoothConfig> remote1 = BindToInterface();
  mojo::Remote<mojom::CrosBluetoothConfig> remote2 = BindToInterface();
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrosBluetoothConfigTest, CallApiFunction) {
  mojo::Remote<mojom::CrosBluetoothConfig> remote = BindToInterface();
  FakeSystemPropertiesObserver fake_observer;
  remote->ObserveSystemProperties(fake_observer.GeneratePendingRemote());
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrosBluetoothConfigTest, CallPowerFunction) {
  mojo::Remote<mojom::CrosBluetoothConfig> remote = BindToInterface();
  remote->SetBluetoothEnabledState(true);
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrosBluetoothConfigTest, CallPairingFunction) {
  mojo::Remote<mojom::CrosBluetoothConfig> remote = BindToInterface();
  auto delegate = std::make_unique<FakeBluetoothDiscoveryDelegate>();
  remote->StartDiscovery(delegate->GeneratePendingRemote());
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrosBluetoothConfigTest, CallDeviceManagementFunctions) {
  mojo::Remote<mojom::CrosBluetoothConfig> remote = BindToInterface();
  const std::string device_id = "device_id";
  absl::optional<bool> result;

  remote->Connect(device_id,
                  base::BindLambdaForTesting(
                      [&result](bool success) { result = success; }));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(result.value());
  result.reset();

  remote->Disconnect(device_id,
                     base::BindLambdaForTesting(
                         [&result](bool success) { result = success; }));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(result.value());
  result.reset();

  remote->Forget(device_id, base::BindLambdaForTesting(
                                [&result](bool success) { result = success; }));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(result.value());
}

TEST_F(CrosBluetoothConfigTest, CallNamingFunction) {
  mojo::Remote<mojom::CrosBluetoothConfig> remote = BindToInterface();
  remote->SetDeviceNickname("device_id", "nickname");
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrosBluetoothConfigTest, CallBluetoothDeviceStatusApiFunction) {
  mojo::Remote<mojom::CrosBluetoothConfig> remote = BindToInterface();
  FakeBluetoothDeviceStatusObserver fake_observer;
  remote->ObserveDeviceStatusChanges(fake_observer.GeneratePendingRemote());
  base::RunLoop().RunUntilIdle();
}

}  // namespace bluetooth_config
}  // namespace chromeos
