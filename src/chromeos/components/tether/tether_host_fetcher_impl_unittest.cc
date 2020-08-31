// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/tether_host_fetcher_impl.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "chromeos/components/multidevice/remote_device.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/components/multidevice/software_feature.h"
#include "chromeos/components/multidevice/software_feature_state.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace tether {

namespace {

const size_t kNumTestDevices = 5;

class TestObserver : public TetherHostFetcher::Observer {
 public:
  TestObserver() = default;
  virtual ~TestObserver() = default;

  size_t num_updates() { return num_updates_; }

  // TetherHostFetcher::Observer:
  void OnTetherHostsUpdated() override { ++num_updates_; }

 private:
  size_t num_updates_ = 0;
};

// This should be identical to TetherHostSource in tether_host_fetcher_impl.cc.
enum class TetherHostSource {
  UNKNOWN,
  MULTIDEVICE_SETUP_CLIENT,
  DEVICE_SYNC_CLIENT,
  REMOTE_DEVICE_PROVIDER
};

}  // namespace

class TetherHostFetcherImplTest : public testing::Test {
 public:
  TetherHostFetcherImplTest()
      : test_remote_device_list_(CreateTestRemoteDeviceList()),
        test_remote_device_ref_list_(
            CreateTestRemoteDeviceRefList(test_remote_device_list_)) {}

  void SetUp() override {
    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_multidevice_setup_client_ = std::make_unique<
        chromeos::multidevice_setup::FakeMultiDeviceSetupClient>();
  }

  void InitializeTest() {
    SetSyncedDevices(test_remote_device_list_);

    tether_host_fetcher_ = TetherHostFetcherImpl::Factory::Create(
        fake_device_sync_client_.get(), fake_multidevice_setup_client_.get());

    fake_device_sync_client_->NotifyReady();
    test_observer_ = std::make_unique<TestObserver>();
    tether_host_fetcher_->AddObserver(test_observer_.get());
  }

  void OnTetherHostListFetched(
      const multidevice::RemoteDeviceRefList& device_list) {
    last_fetched_list_ = device_list;
  }

  void OnSingleTetherHostFetched(
      base::Optional<multidevice::RemoteDeviceRef> device) {
    last_fetched_single_host_ = device;
  }

  void VerifyAllTetherHosts(
      const multidevice::RemoteDeviceRefList expected_list) {
    tether_host_fetcher_->FetchAllTetherHosts(
        base::Bind(&TetherHostFetcherImplTest::OnTetherHostListFetched,
                   base::Unretained(this)));
    EXPECT_EQ(expected_list, last_fetched_list_);
  }

  void VerifySingleTetherHost(
      const std::string& device_id,
      base::Optional<multidevice::RemoteDeviceRef> expected_device) {
    tether_host_fetcher_->FetchTetherHost(
        device_id,
        base::Bind(&TetherHostFetcherImplTest::OnSingleTetherHostFetched,
                   base::Unretained(this)));
    if (expected_device)
      EXPECT_EQ(expected_device, last_fetched_single_host_);
    else
      EXPECT_EQ(base::nullopt, last_fetched_single_host_);
  }

  multidevice::RemoteDeviceList CreateTestRemoteDeviceList() {
    multidevice::RemoteDeviceList list =
        multidevice::CreateRemoteDeviceListForTest(kNumTestDevices);
    for (auto& device : list) {
      device.software_features
          [chromeos::multidevice::SoftwareFeature::kInstantTetheringHost] =
          multidevice::SoftwareFeatureState::kSupported;
    }

    // Mark the first device enabled instead of supported.
    list[0].software_features
        [chromeos::multidevice::SoftwareFeature::kInstantTetheringHost] =
        multidevice::SoftwareFeatureState::kEnabled;
    list[0].software_features
        [chromeos::multidevice::SoftwareFeature::kBetterTogetherHost] =
        multidevice::SoftwareFeatureState::kEnabled;

    return list;
  }

  multidevice::RemoteDeviceRefList CreateTestRemoteDeviceRefList(
      multidevice::RemoteDeviceList remote_device_list) {
    multidevice::RemoteDeviceRefList list;
    for (const auto& device : remote_device_list) {
      list.push_back(multidevice::RemoteDeviceRef(
          std::make_shared<multidevice::RemoteDevice>(device)));
    }
    return list;
  }

  void SetSyncedDevices(multidevice::RemoteDeviceList devices) {
    fake_device_sync_client_->set_synced_devices(
        CreateTestRemoteDeviceRefList(devices));

    if (devices.empty()) {
      fake_multidevice_setup_client_->SetHostStatusWithDevice(
          std::make_pair(multidevice_setup::mojom::HostStatus::kNoEligibleHosts,
                         base::nullopt /* host_device */));
      fake_multidevice_setup_client_->SetFeatureState(
          multidevice_setup::mojom::Feature::kInstantTethering,
          multidevice_setup::mojom::FeatureState::kUnavailableNoVerifiedHost);
      return;
    }

    fake_multidevice_setup_client_->SetHostStatusWithDevice(std::make_pair(
        multidevice_setup::mojom::HostStatus::kHostVerified,
        multidevice::RemoteDeviceRef(
            std::make_shared<multidevice::RemoteDevice>(devices[0]))));
    fake_multidevice_setup_client_->SetFeatureState(
        multidevice_setup::mojom::Feature::kInstantTethering,
        multidevice_setup::mojom::FeatureState::kEnabledByUser);
  }

  void NotifyNewDevicesSynced() {
    fake_device_sync_client_->NotifyNewDevicesSynced();
  }

  void TestHasSyncedTetherHosts() {
    InitializeTest();

    EXPECT_TRUE(tether_host_fetcher_->HasSyncedTetherHosts());
    EXPECT_EQ(0u, test_observer_->num_updates());

    // Update the list of devices to be empty.
    SetSyncedDevices(multidevice::RemoteDeviceList());
    NotifyNewDevicesSynced();
    EXPECT_FALSE(tether_host_fetcher_->HasSyncedTetherHosts());
    EXPECT_EQ(1u, test_observer_->num_updates());

    // Notify that the list has changed, even though it hasn't. There should be
    // no update.
    NotifyNewDevicesSynced();
    EXPECT_FALSE(tether_host_fetcher_->HasSyncedTetherHosts());
    EXPECT_EQ(1u, test_observer_->num_updates());

    // Update the list to include device 0 only.
    SetSyncedDevices({test_remote_device_list_[0]});
    NotifyNewDevicesSynced();
    EXPECT_TRUE(tether_host_fetcher_->HasSyncedTetherHosts());
    EXPECT_EQ(2u, test_observer_->num_updates());

    // Notify that the list has changed, even though it hasn't. There should be
    // no update.
    NotifyNewDevicesSynced();
    EXPECT_TRUE(tether_host_fetcher_->HasSyncedTetherHosts());
    EXPECT_EQ(2u, test_observer_->num_updates());
  }

  multidevice::RemoteDeviceList test_remote_device_list_;
  multidevice::RemoteDeviceRefList test_remote_device_ref_list_;

  multidevice::RemoteDeviceRefList last_fetched_list_;
  base::Optional<multidevice::RemoteDeviceRef> last_fetched_single_host_;
  std::unique_ptr<TestObserver> test_observer_;

  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<chromeos::multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;

  std::unique_ptr<TetherHostFetcher> tether_host_fetcher_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TetherHostFetcherImplTest);
};

TEST_F(TetherHostFetcherImplTest, TestHasSyncedTetherHosts) {
  InitializeTest();

  EXPECT_TRUE(tether_host_fetcher_->HasSyncedTetherHosts());
  EXPECT_EQ(0u, test_observer_->num_updates());

  // Update the list of devices to be empty.
  SetSyncedDevices(multidevice::RemoteDeviceList());
  NotifyNewDevicesSynced();
  EXPECT_FALSE(tether_host_fetcher_->HasSyncedTetherHosts());
  EXPECT_EQ(1u, test_observer_->num_updates());

  // Notify that the list has changed, even though it hasn't. There should be
  // no update.
  NotifyNewDevicesSynced();
  EXPECT_FALSE(tether_host_fetcher_->HasSyncedTetherHosts());
  EXPECT_EQ(1u, test_observer_->num_updates());

  // Update the list to include device 0 only.
  SetSyncedDevices({test_remote_device_list_[0]});
  NotifyNewDevicesSynced();
  EXPECT_TRUE(tether_host_fetcher_->HasSyncedTetherHosts());
  EXPECT_EQ(2u, test_observer_->num_updates());

  // Notify that the list has changed, even though it hasn't. There should be
  // no update.
  NotifyNewDevicesSynced();
  EXPECT_TRUE(tether_host_fetcher_->HasSyncedTetherHosts());
  EXPECT_EQ(2u, test_observer_->num_updates());
}

TEST_F(TetherHostFetcherImplTest, TestFetchAllTetherHosts) {
  InitializeTest();

  // Create a list of test devices, only some of which are valid tether hosts.
  // Ensure that only that subset is fetched.
  test_remote_device_list_[3].software_features
      [chromeos::multidevice::SoftwareFeature::kInstantTetheringHost] =
      multidevice::SoftwareFeatureState::kNotSupported;
  test_remote_device_list_[4].software_features
      [chromeos::multidevice::SoftwareFeature::kInstantTetheringHost] =
      multidevice::SoftwareFeatureState::kNotSupported;

  SetSyncedDevices(test_remote_device_list_);
  NotifyNewDevicesSynced();

  multidevice::RemoteDeviceRefList expected_host_device_list;

  expected_host_device_list =
      CreateTestRemoteDeviceRefList({test_remote_device_list_[0]});

  VerifyAllTetherHosts(expected_host_device_list);
}

TEST_F(TetherHostFetcherImplTest, TestSingleTetherHost) {
  InitializeTest();

  VerifySingleTetherHost(test_remote_device_ref_list_[0].GetDeviceId(),
                         test_remote_device_ref_list_[0]);

  // Now, set device 0 as the only device. It should still be returned when
  // requested.
  SetSyncedDevices(multidevice::RemoteDeviceList{test_remote_device_list_[0]});
  NotifyNewDevicesSynced();
  VerifySingleTetherHost(test_remote_device_ref_list_[0].GetDeviceId(),
                         test_remote_device_ref_list_[0]);

  // Now, set another device as the only device, but remove its mobile data
  // support. It should not be returned.
  multidevice::RemoteDevice remote_device = multidevice::RemoteDevice();
  remote_device.software_features
      [chromeos::multidevice::SoftwareFeature::kInstantTetheringHost] =
      multidevice::SoftwareFeatureState::kNotSupported;

  SetSyncedDevices(multidevice::RemoteDeviceList{remote_device});
  NotifyNewDevicesSynced();
  VerifySingleTetherHost(test_remote_device_ref_list_[0].GetDeviceId(),
                         base::nullopt);

  // Update the list; now, there are no more devices.
  SetSyncedDevices(multidevice::RemoteDeviceList());
  NotifyNewDevicesSynced();
  VerifySingleTetherHost(test_remote_device_ref_list_[0].GetDeviceId(),
                         base::nullopt);
}

TEST_F(TetherHostFetcherImplTest,
       TestSingleTetherHost_IdDoesNotCorrespondToDevice) {
  InitializeTest();
  VerifySingleTetherHost("nonexistentId", base::nullopt);
}

}  // namespace tether

}  // namespace chromeos
