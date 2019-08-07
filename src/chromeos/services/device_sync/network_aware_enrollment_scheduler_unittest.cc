// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/network_aware_enrollment_scheduler.h"

#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test_helper.h"
#include "chromeos/services/device_sync/fake_cryptauth_enrollment_scheduler.h"
#include "chromeos/services/device_sync/persistent_enrollment_scheduler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace device_sync {

namespace {

const char kWifiServiceGuid[] = "wifiGuid";

enum class NetworkConnectionStatus { kDisconnected, kConnecting, kConnected };

class FakePersistentEnrollmentSchedulerFactory
    : public PersistentEnrollmentScheduler::Factory {
 public:
  FakePersistentEnrollmentSchedulerFactory() = default;
  ~FakePersistentEnrollmentSchedulerFactory() override = default;

  FakeCryptAuthEnrollmentScheduler* instance() { return instance_; }

 private:
  // PersistentEnrollmentScheduler::Factory:
  std::unique_ptr<CryptAuthEnrollmentScheduler> BuildInstance(
      CryptAuthEnrollmentScheduler::Delegate* delegate,
      PrefService* pref_service,
      base::Clock* clock,
      std::unique_ptr<base::OneShotTimer> timer,
      scoped_refptr<base::TaskRunner> task_runner) override {
    EXPECT_FALSE(instance_);
    auto fake_network_unaware_scheduler =
        std::make_unique<FakeCryptAuthEnrollmentScheduler>(delegate);
    instance_ = fake_network_unaware_scheduler.get();
    return std::move(fake_network_unaware_scheduler);
  }

  FakeCryptAuthEnrollmentScheduler* instance_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakePersistentEnrollmentSchedulerFactory);
};

}  // namespace

class DeviceSyncNetworkAwareEnrollmentSchedulerTest : public testing::Test {
 protected:
  DeviceSyncNetworkAwareEnrollmentSchedulerTest() = default;
  ~DeviceSyncNetworkAwareEnrollmentSchedulerTest() override = default;

  void SetUp() override {
    fake_persistent_enrollment_scheduler_factory_ =
        std::make_unique<FakePersistentEnrollmentSchedulerFactory>();
    PersistentEnrollmentScheduler::Factory::SetFactoryForTesting(
        fake_persistent_enrollment_scheduler_factory_.get());

    fake_delegate_ =
        std::make_unique<FakeCryptAuthEnrollmentSchedulerDelegate>();
    scheduler_ = NetworkAwareEnrollmentScheduler::Factory::Get()->BuildInstance(
        fake_delegate_.get(), nullptr /* pref_service */,
        helper_.network_state_handler());
  }

  void TearDown() override {
    PersistentEnrollmentScheduler::Factory::SetFactoryForTesting(nullptr);
  }

  void AddDisconnectedWifiNetwork() {
    EXPECT_TRUE(wifi_network_service_path_.empty());

    std::stringstream ss;
    ss << "{"
       << "  \"GUID\": \"" << kWifiServiceGuid << "\","
       << "  \"Type\": \"" << shill::kTypeWifi << "\","
       << "  \"State\": \"" << shill::kStateIdle << "\""
       << "}";

    wifi_network_service_path_ = helper_.ConfigureService(ss.str());
  }

  void SetWifiNetworkStatus(NetworkConnectionStatus connection_status) {
    std::string shill_connection_status;
    switch (connection_status) {
      case NetworkConnectionStatus::kDisconnected:
        shill_connection_status = shill::kStateIdle;
        break;
      case NetworkConnectionStatus::kConnecting:
        shill_connection_status = shill::kStateAssociation;
        break;
      case NetworkConnectionStatus::kConnected:
        shill_connection_status = shill::kStateReady;
        break;
    }

    helper_.SetServiceProperty(wifi_network_service_path_,
                               shill::kStateProperty,
                               base::Value(shill_connection_status));
    base::RunLoop().RunUntilIdle();
  }

  void RemoveWifiNetwork() {
    EXPECT_TRUE(!wifi_network_service_path_.empty());

    helper_.service_test()->RemoveService(wifi_network_service_path_);
    base::RunLoop().RunUntilIdle();

    wifi_network_service_path_.clear();
  }

  const std::vector<base::Optional<cryptauthv2::PolicyReference>>&
  delegate_policy_references() const {
    return fake_delegate_->policy_references_from_enrollment_requests();
  }

  CryptAuthEnrollmentScheduler* scheduler() { return scheduler_.get(); }

  FakeCryptAuthEnrollmentScheduler* fake_network_unaware_scheduler() {
    return fake_persistent_enrollment_scheduler_factory_->instance();
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  NetworkStateTestHelper helper_{false /* use_default_devices_and_services */};

  std::unique_ptr<FakePersistentEnrollmentSchedulerFactory>
      fake_persistent_enrollment_scheduler_factory_;

  std::unique_ptr<FakeCryptAuthEnrollmentSchedulerDelegate> fake_delegate_;
  std::unique_ptr<CryptAuthEnrollmentScheduler> scheduler_;

  std::string wifi_network_service_path_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSyncNetworkAwareEnrollmentSchedulerTest);
};

TEST_F(DeviceSyncNetworkAwareEnrollmentSchedulerTest,
       RequestReceivedWhileOnline) {
  AddDisconnectedWifiNetwork();
  SetWifiNetworkStatus(NetworkConnectionStatus::kConnected);

  fake_network_unaware_scheduler()->RequestEnrollmentNow();
  EXPECT_EQ(1u, delegate_policy_references().size());

  fake_network_unaware_scheduler()->RequestEnrollmentNow();
  EXPECT_EQ(2u, delegate_policy_references().size());
}

TEST_F(DeviceSyncNetworkAwareEnrollmentSchedulerTest,
       RequestReceivedWhileOffline) {
  AddDisconnectedWifiNetwork();
  SetWifiNetworkStatus(NetworkConnectionStatus::kDisconnected);

  // Request enrollment while disconnected using a null PolicyReference; since
  // the network is disconnected, nothing should occur.
  fake_network_unaware_scheduler()->set_client_directive_policy_reference(
      base::nullopt);
  fake_network_unaware_scheduler()->RequestEnrollmentNow();
  EXPECT_TRUE(delegate_policy_references().empty());
  EXPECT_TRUE(fake_network_unaware_scheduler()->IsWaitingForEnrollmentResult());
  EXPECT_FALSE(scheduler()->IsWaitingForEnrollmentResult());

  // Start connecting; no enrollment should have been requested yet.
  SetWifiNetworkStatus(NetworkConnectionStatus::kConnecting);
  EXPECT_TRUE(delegate_policy_references().empty());

  // Try requesting while connecting, this time passing a PolicyReference; since
  // the network is only connecting, nothing should occur.
  static const std::string kPolicyReferenceName = "policyReferenceName";
  cryptauthv2::PolicyReference policy_reference;
  policy_reference.set_name(kPolicyReferenceName);
  fake_network_unaware_scheduler()->set_client_directive_policy_reference(
      policy_reference);
  fake_network_unaware_scheduler()->RequestEnrollmentNow();
  EXPECT_TRUE(delegate_policy_references().empty());
  EXPECT_TRUE(fake_network_unaware_scheduler()->IsWaitingForEnrollmentResult());
  EXPECT_FALSE(scheduler()->IsWaitingForEnrollmentResult());

  // Connect; this should cause the enrollment request to be sent. Because the
  // enrollment request with |policy_reference| was sent after the one with a
  // null PolicyReference, the received request should have a PolicyReference.
  SetWifiNetworkStatus(NetworkConnectionStatus::kConnected);
  EXPECT_EQ(1u, delegate_policy_references().size());
  EXPECT_EQ(kPolicyReferenceName, delegate_policy_references()[0]->name());
  EXPECT_TRUE(fake_network_unaware_scheduler()->IsWaitingForEnrollmentResult());
  EXPECT_TRUE(scheduler()->IsWaitingForEnrollmentResult());

  scheduler()->HandleEnrollmentResult(CryptAuthEnrollmentResult(
      CryptAuthEnrollmentResult::ResultCode::kSuccessNoNewKeysNeeded,
      base::nullopt /* client_directive */));
  EXPECT_FALSE(
      fake_network_unaware_scheduler()->IsWaitingForEnrollmentResult());
  EXPECT_FALSE(scheduler()->IsWaitingForEnrollmentResult());

  // Now, remove the network entirely.
  RemoveWifiNetwork();

  // Requesting enrollment when there is no default network at all should not
  // trigger an enrollment request to be sent.
  fake_network_unaware_scheduler()->RequestEnrollmentNow();
  EXPECT_EQ(1u, delegate_policy_references().size());
}

TEST_F(DeviceSyncNetworkAwareEnrollmentSchedulerTest,
       RequestReceivedWhenNoNetworksExist) {
  fake_network_unaware_scheduler()->RequestEnrollmentNow();
  EXPECT_TRUE(delegate_policy_references().empty());

  // Add a network and connect it; this should cause the pending request to be
  // forwarded.
  AddDisconnectedWifiNetwork();
  SetWifiNetworkStatus(NetworkConnectionStatus::kConnected);
  EXPECT_EQ(1u, delegate_policy_references().size());
}

}  // namespace device_sync

}  // namespace chromeos
