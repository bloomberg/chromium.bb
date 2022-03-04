// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/apps_access_manager_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/eche_app_ui/apps_access_setup_operation.h"
#include "ash/webui/eche_app_ui/fake_eche_connector.h"
#include "ash/webui/eche_app_ui/fake_eche_message_receiver.h"
#include "ash/webui/eche_app_ui/fake_feature_status_provider.h"
#include "ash/webui/eche_app_ui/pref_names.h"
#include "ash/webui/eche_app_ui/proto/exo_messages.pb.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace eche_app {
namespace {
class FakeObserver : public AppsAccessManager::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_calls() const { return num_calls_; }

  // AppsAccessManager::Observer:
  void OnAppsAccessChanged() override { ++num_calls_; }

 private:
  size_t num_calls_ = 0;
};

class FakeOperationDelegate : public AppsAccessSetupOperation::Delegate {
 public:
  FakeOperationDelegate() = default;
  ~FakeOperationDelegate() override = default;

  AppsAccessSetupOperation::Status status() const { return status_; }

  // AppsAccessSetupOperation::Delegate:
  void OnAppsStatusChange(
      AppsAccessSetupOperation::Status new_status) override {
    status_ = new_status;
  }

 private:
  AppsAccessSetupOperation::Status status_ =
      AppsAccessSetupOperation::Status::kConnecting;
};
}  // namespace

class AppsAccessManagerImplTest : public testing::Test {
 protected:
  AppsAccessManagerImplTest() = default;
  AppsAccessManagerImplTest(const AppsAccessManagerImplTest&) = delete;
  AppsAccessManagerImplTest& operator=(const AppsAccessManagerImplTest&) =
      delete;
  ~AppsAccessManagerImplTest() override = default;

  void SetUp() override {
    AppsAccessManagerImpl::RegisterPrefs(pref_service_.registry());
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kEcheSWA,
                              chromeos::features::
                                  kEchePhoneHubPermissionsOnboarding},
        /*disabled_features=*/{});

    fake_eche_connector_ = std::make_unique<FakeEcheConnector>();
    fake_eche_message_receiver_ = std::make_unique<FakeEcheMessageReceiver>();
    fake_feature_status_provider_ = std::make_unique<FakeFeatureStatusProvider>(
        FeatureStatus::kDependentFeature);
  }

  void TearDown() override {
    apps_access_manager_->RemoveObserver(&fake_observer_);
    apps_access_manager_.reset();
    fake_eche_connector_.reset();
    fake_eche_message_receiver_.reset();
    fake_feature_status_provider_.reset();
  }

  void Initialize(AppsAccessManager::AccessStatus expected_status) {
    pref_service_.SetInteger(prefs::kAppsAccessStatus,
                             static_cast<int>(expected_status));
    apps_access_manager_ = std::make_unique<AppsAccessManagerImpl>(
        fake_eche_connector_.get(), fake_eche_message_receiver_.get(),
        fake_feature_status_provider_.get(), &pref_service_);
    apps_access_manager_->AddObserver(&fake_observer_);
  }

  void FakeGetAppsAccessStateResponse(eche_app::proto::Result result,
                                      eche_app::proto::AppsAccessState status) {
    fake_eche_message_receiver_->FakeGetAppsAccessStateResponse(result, status);
  }

  void FakeSendAppsSetupResponse(eche_app::proto::Result result,
                                 eche_app::proto::AppsAccessState status) {
    fake_eche_message_receiver_->FakeSendAppsSetupResponse(result, status);
  }

  void SetFeatureStatus(FeatureStatus status) {
    fake_feature_status_provider_->SetStatus(status);
  }

  FeatureStatus GetFeatureStatus() {
    return fake_feature_status_provider_->GetStatus();
  }

  void VerifyAppsAccessGrantedState(
      AppsAccessManager::AccessStatus expected_status) {
    EXPECT_EQ(static_cast<int>(expected_status),
              pref_service_.GetInteger(prefs::kAppsAccessStatus));
    EXPECT_EQ(expected_status, apps_access_manager_->GetAccessStatus());
  }

  AppsAccessSetupOperation::Status GetAppsAccessSetupOperationStatus() {
    return fake_delegate_.status();
  }

  std::unique_ptr<AppsAccessSetupOperation> StartSetupOperation() {
    return apps_access_manager_->AttemptAppsAccessSetup(&fake_delegate_);
  }

  bool IsSetupOperationInProgress() {
    return apps_access_manager_->IsSetupOperationInProgress();
  }

  size_t GetNumObserverCalls() const { return fake_observer_.num_calls(); }

  size_t GetAppsSetupRequestCount() const {
    return fake_eche_connector_->send_apps_setup_request_count();
  }

  size_t GetAppsAccessStateRequestCount() const {
    return fake_eche_connector_->get_apps_access_state_request_count();
  }

  size_t GetAttemptNearbyConnectionCount() const {
    return fake_eche_connector_->attempt_nearby_connection_count();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  FakeObserver fake_observer_;
  FakeOperationDelegate fake_delegate_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<AppsAccessManager> apps_access_manager_;
  std::unique_ptr<FakeEcheConnector> fake_eche_connector_;
  std::unique_ptr<FakeEcheMessageReceiver> fake_eche_message_receiver_;
  std::unique_ptr<FakeFeatureStatusProvider> fake_feature_status_provider_;
};

TEST_F(AppsAccessManagerImplTest, InitiallyGranted) {
  Initialize(AppsAccessManager::AccessStatus::kAccessGranted);
  VerifyAppsAccessGrantedState(AppsAccessManager::AccessStatus::kAccessGranted);

  // Cannot start the apps access setup flow if access has already been
  // granted.
  auto operation = StartSetupOperation();
  EXPECT_FALSE(operation);
}

TEST_F(AppsAccessManagerImplTest, OnFeatureStatusChanged) {
  Initialize(AppsAccessManager::AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(
      AppsAccessManager::AccessStatus::kAvailableButNotGranted);

  // Set initial state to disconnected.
  SetFeatureStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(1u, GetAttemptNearbyConnectionCount());
  EXPECT_EQ(0u, GetAppsAccessStateRequestCount());
  EXPECT_EQ(AppsAccessSetupOperation::Status::kConnecting,
            GetAppsAccessSetupOperationStatus());

  // Simulate feature status to be enabled and connected. SetupOperation is also
  // not in progress, so expect no new requests to be sent.
  SetFeatureStatus(FeatureStatus::kConnected);
  EXPECT_EQ(1u, GetAppsAccessStateRequestCount());
  EXPECT_EQ(0u, GetAppsSetupRequestCount());
  EXPECT_EQ(AppsAccessSetupOperation::Status::kConnecting,
            GetAppsAccessSetupOperationStatus());

  // Simulate setup operation is in progress. This will trigger a sent request.
  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);
  EXPECT_EQ(1u, GetAppsSetupRequestCount());
  EXPECT_EQ(AppsAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetAppsAccessSetupOperationStatus());

  // Set another feature status, expect status to be updated.
  SetFeatureStatus(FeatureStatus::kIneligible);
  SetFeatureStatus(FeatureStatus::kConnected);
  SetFeatureStatus(FeatureStatus::kConnecting);
  EXPECT_EQ(2u, GetAppsSetupRequestCount());
  EXPECT_EQ(AppsAccessSetupOperation::Status::kConnectionDisconnected,
            GetAppsAccessSetupOperationStatus());

  auto operation1 = StartSetupOperation();
  EXPECT_TRUE(operation1);
  EXPECT_EQ(2u, GetAppsSetupRequestCount());
  EXPECT_EQ(AppsAccessSetupOperation::Status::kConnecting,
            GetAppsAccessSetupOperationStatus());

  SetFeatureStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(2u, GetAppsSetupRequestCount());
  EXPECT_EQ(AppsAccessSetupOperation::Status::kTimedOutConnecting,
            GetAppsAccessSetupOperationStatus());
}

TEST_F(AppsAccessManagerImplTest, StartDisconnectedAndNoAccess) {
  // Set initial state to disconnected.
  SetFeatureStatus(FeatureStatus::kDisconnected);

  Initialize(AppsAccessManager::AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(
      AppsAccessManager::AccessStatus::kAvailableButNotGranted);

  // Start a setup operation with enabled but disconnected status and access
  // not granted.
  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);
  EXPECT_EQ(1u, GetAttemptNearbyConnectionCount());

  // Simulate changing states from connecting to connected.
  SetFeatureStatus(FeatureStatus::kConnecting);
  SetFeatureStatus(FeatureStatus::kConnected);

  // Verify that the request message has been sent and our operation status
  // is updated.
  EXPECT_EQ(1u, GetAppsSetupRequestCount());
  EXPECT_EQ(AppsAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetAppsAccessSetupOperationStatus());

  // Simulate getting a response back from the phone.
  FakeSendAppsSetupResponse(eche_app::proto::Result::RESULT_NO_ERROR,
                            eche_app::proto::AppsAccessState::ACCESS_GRANTED);
  VerifyAppsAccessGrantedState(AppsAccessManager::AccessStatus::kAccessGranted);
  EXPECT_EQ(AppsAccessSetupOperation::Status::kCompletedSuccessfully,
            GetAppsAccessSetupOperationStatus());
}

TEST_F(AppsAccessManagerImplTest, StartConnectingAndNoAccess) {
  // Set initial state to connecting.
  SetFeatureStatus(FeatureStatus::kConnecting);

  Initialize(AppsAccessManager::AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(
      AppsAccessManager::AccessStatus::kAvailableButNotGranted);

  // Start a setup operation with enabled and connecting status and access
  // not granted.
  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  // Simulate changing states from connecting to connected.
  SetFeatureStatus(FeatureStatus::kConnected);

  // Verify that the request message has been sent and our operation status
  // is updated.
  EXPECT_EQ(1u, GetAppsSetupRequestCount());
  EXPECT_EQ(AppsAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetAppsAccessSetupOperationStatus());

  // Simulate getting a response back from the phone.
  FakeSendAppsSetupResponse(eche_app::proto::Result::RESULT_NO_ERROR,
                            eche_app::proto::AppsAccessState::ACCESS_GRANTED);
  VerifyAppsAccessGrantedState(AppsAccessManager::AccessStatus::kAccessGranted);
  EXPECT_EQ(AppsAccessSetupOperation::Status::kCompletedSuccessfully,
            GetAppsAccessSetupOperationStatus());
}

TEST_F(AppsAccessManagerImplTest, StartConnectedAndNoAccess) {
  // Set initial state to connected.
  SetFeatureStatus(FeatureStatus::kConnected);

  Initialize(AppsAccessManager::AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(
      AppsAccessManager::AccessStatus::kAvailableButNotGranted);

  // Start a setup operation with enabled and connected status and access
  // not granted.
  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  // Verify that the request message has been sent and our operation status
  // is updated.
  EXPECT_EQ(1u, GetAppsSetupRequestCount());
  EXPECT_EQ(AppsAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetAppsAccessSetupOperationStatus());

  // Simulate getting a response back from the phone.
  FakeSendAppsSetupResponse(eche_app::proto::Result::RESULT_NO_ERROR,
                            eche_app::proto::AppsAccessState::ACCESS_GRANTED);
  VerifyAppsAccessGrantedState(AppsAccessManager::AccessStatus::kAccessGranted);
  EXPECT_EQ(AppsAccessSetupOperation::Status::kCompletedSuccessfully,
            GetAppsAccessSetupOperationStatus());
}

TEST_F(AppsAccessManagerImplTest, SimulateConnectingToDisconnected) {
  Initialize(AppsAccessManager::AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(
      AppsAccessManager::AccessStatus::kAvailableButNotGranted);

  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  // Simulate a disconnection and expect that status has been updated.
  SetFeatureStatus(FeatureStatus::kConnecting);
  SetFeatureStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(AppsAccessSetupOperation::Status::kTimedOutConnecting,
            GetAppsAccessSetupOperationStatus());
}

TEST_F(AppsAccessManagerImplTest, SimulateConnectedToDisconnected) {
  Initialize(AppsAccessManager::AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(
      AppsAccessManager::AccessStatus::kAvailableButNotGranted);

  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  // Simulate connected state.
  SetFeatureStatus(FeatureStatus::kConnected);
  EXPECT_EQ(1u, GetAppsSetupRequestCount());
  // Simulate a disconnection, expect status update.
  SetFeatureStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(AppsAccessSetupOperation::Status::kConnectionDisconnected,
            GetAppsAccessSetupOperationStatus());
}

TEST_F(AppsAccessManagerImplTest, SimulateConnectedToDisabled) {
  Initialize(AppsAccessManager::AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(
      AppsAccessManager::AccessStatus::kAvailableButNotGranted);

  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  // Simulate connected state.
  SetFeatureStatus(FeatureStatus::kConnected);
  EXPECT_EQ(1u, GetAppsSetupRequestCount());

  // Simulate disabling the feature, expect status update.
  SetFeatureStatus(FeatureStatus::kDisabled);
  EXPECT_EQ(AppsAccessSetupOperation::Status::kConnectionDisconnected,
            GetAppsAccessSetupOperationStatus());
}

TEST_F(AppsAccessManagerImplTest, SimulateConnectedToDependentFeature) {
  Initialize(AppsAccessManager::AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(
      AppsAccessManager::AccessStatus::kAvailableButNotGranted);

  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  // Simulate connected state.
  SetFeatureStatus(FeatureStatus::kConnected);
  EXPECT_EQ(1u, GetAppsSetupRequestCount());

  // Simulate disabling the feature, expect status update.
  SetFeatureStatus(FeatureStatus::kDependentFeature);
  EXPECT_EQ(AppsAccessSetupOperation::Status::kConnectionDisconnected,
            GetAppsAccessSetupOperationStatus());
}

TEST_F(AppsAccessManagerImplTest, SimulateConnectedToDependentFeaturePending) {
  Initialize(AppsAccessManager::AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(
      AppsAccessManager::AccessStatus::kAvailableButNotGranted);

  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  // Simulate connected state.
  SetFeatureStatus(FeatureStatus::kConnected);
  EXPECT_EQ(1u, GetAppsSetupRequestCount());

  // Simulate disabling the feature, expect status update.
  SetFeatureStatus(FeatureStatus::kDependentFeaturePending);
  EXPECT_EQ(AppsAccessSetupOperation::Status::kConnectionDisconnected,
            GetAppsAccessSetupOperationStatus());
}

TEST_F(AppsAccessManagerImplTest, FlipAccessNotGrantedToGranted) {
  Initialize(AppsAccessManager::AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(
      AppsAccessManager::AccessStatus::kAvailableButNotGranted);

  // Simulate flipping the access state to no granted.
  FakeGetAppsAccessStateResponse(
      eche_app::proto::Result::RESULT_NO_ERROR,
      eche_app::proto::AppsAccessState::ACCESS_GRANTED);

  VerifyAppsAccessGrantedState(AppsAccessManager::AccessStatus::kAccessGranted);
  EXPECT_EQ(1u, GetNumObserverCalls());
}

TEST_F(AppsAccessManagerImplTest, FlipAccessGrantedToNotGranted) {
  Initialize(AppsAccessManager::AccessStatus::kAccessGranted);
  VerifyAppsAccessGrantedState(AppsAccessManager::AccessStatus::kAccessGranted);

  // Simulate flipping the access state to no granted.
  FakeGetAppsAccessStateResponse(
      eche_app::proto::Result::RESULT_NO_ERROR,
      eche_app::proto::AppsAccessState::ACCESS_NOT_GRANTED);

  VerifyAppsAccessGrantedState(
      AppsAccessManager::AccessStatus::kAvailableButNotGranted);
  EXPECT_EQ(1u, GetNumObserverCalls());
}

TEST_F(AppsAccessManagerImplTest, AccessNotChanged) {
  Initialize(AppsAccessManager::AccessStatus::kAccessGranted);
  VerifyAppsAccessGrantedState(AppsAccessManager::AccessStatus::kAccessGranted);

  // Simulate flipping the access state to granted.
  FakeGetAppsAccessStateResponse(
      eche_app::proto::Result::RESULT_NO_ERROR,
      eche_app::proto::AppsAccessState::ACCESS_GRANTED);

  VerifyAppsAccessGrantedState(AppsAccessManager::AccessStatus::kAccessGranted);
  EXPECT_EQ(0u, GetNumObserverCalls());
}

}  // namespace eche_app
}  // namespace ash
