// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_service_impl.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/sync_util.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/driver/data_type_manager_impl.h"
#include "components/sync/driver/fake_data_type_controller.h"
#include "components/sync/driver/fake_sync_api_component_factory.h"
#include "components/sync/driver/mock_trusted_vault_client.h"
#include "components/sync/driver/sync_client_mock.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/sync_service_impl_bundle.h"
#include "components/sync/driver/sync_service_observer.h"
#include "components/sync/driver/sync_service_utils.h"
#include "components/sync/driver/sync_token_status.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "components/sync/invalidations/switches.h"
#include "components/sync/test/engine/fake_sync_engine.h"
#include "components/version_info/version_info_values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::ByMove;
using testing::Eq;
using testing::Not;
using testing::Return;

namespace syncer {

namespace {

constexpr char kTestUser[] = "test_user@gmail.com";

class TestSyncServiceObserver : public SyncServiceObserver {
 public:
  TestSyncServiceObserver() = default;

  void OnStateChanged(SyncService* sync) override {
    setup_in_progress_ = sync->IsSetupInProgress();
    auth_error_ = sync->GetAuthError();
  }

  bool setup_in_progress() const { return setup_in_progress_; }
  GoogleServiceAuthError auth_error() const { return auth_error_; }

 private:
  bool setup_in_progress_ = false;
  GoogleServiceAuthError auth_error_;
};

// A test harness that uses a real SyncServiceImpl and in most cases a
// FakeSyncEngine.
//
// This is useful if we want to test the SyncServiceImpl and don't care about
// testing the SyncEngine.
class SyncServiceImplTest : public ::testing::Test {
 protected:
  SyncServiceImplTest() = default;
  ~SyncServiceImplTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSyncDeferredStartupTimeoutSeconds, "0");
  }

  void TearDown() override {
    // Kill the service before the profile.
    ShutdownAndDeleteService();
  }

  void SignIn() {
    identity_test_env()->MakePrimaryAccountAvailable(
        kTestUser, signin::ConsentLevel::kSync);
  }

  void CreateService(SyncServiceImpl::StartBehavior behavior,
                     policy::PolicyService* policy_service = nullptr,
                     std::vector<std::pair<ModelType, bool>>
                         registered_types_and_transport_mode_support = {
                             {BOOKMARKS, false},
                             {DEVICE_INFO, true}}) {
    DCHECK(!service_);

    // Default includes a regular controller and a transport-mode controller.
    DataTypeController::TypeVector controllers;
    for (const auto& type_and_transport_mode_support :
         registered_types_and_transport_mode_support) {
      ModelType type = type_and_transport_mode_support.first;
      bool transport_mode_support = type_and_transport_mode_support.second;
      auto controller = std::make_unique<FakeDataTypeController>(
          type, transport_mode_support);
      // Hold a raw pointer to directly interact with the controller.
      controller_map_[type] = controller.get();
      controllers.push_back(std::move(controller));
    }

    std::unique_ptr<SyncClientMock> sync_client =
        sync_service_impl_bundle_.CreateSyncClientMock();
    sync_client_ = sync_client.get();
    ON_CALL(*sync_client, CreateDataTypeControllers)
        .WillByDefault(Return(ByMove(std::move(controllers))));

    auto init_params = sync_service_impl_bundle_.CreateBasicInitParams(
        behavior, std::move(sync_client));
    init_params.policy_service = policy_service;

    service_ = std::make_unique<SyncServiceImpl>(std::move(init_params));
  }

  void CreateServiceWithLocalSyncBackend() {
    DCHECK(!service_);

    // Include a regular controller and a transport-mode controller.
    DataTypeController::TypeVector controllers;
    controllers.push_back(std::make_unique<FakeDataTypeController>(BOOKMARKS));
    controllers.push_back(std::make_unique<FakeDataTypeController>(
        DEVICE_INFO, /*enable_transport_only_modle=*/true));

    std::unique_ptr<SyncClientMock> sync_client =
        sync_service_impl_bundle_.CreateSyncClientMock();
    sync_client_ = sync_client.get();
    ON_CALL(*sync_client, CreateDataTypeControllers)
        .WillByDefault(Return(ByMove(std::move(controllers))));

    SyncServiceImpl::InitParams init_params =
        sync_service_impl_bundle_.CreateBasicInitParams(
            SyncServiceImpl::AUTO_START, std::move(sync_client));

    prefs()->SetBoolean(prefs::kEnableLocalSyncBackend, true);
    init_params.identity_manager = nullptr;

    service_ = std::make_unique<SyncServiceImpl>(std::move(init_params));
  }

  void ShutdownAndDeleteService() {
    if (service_)
      service_->Shutdown();
    service_.reset();
  }

  void PopulatePrefsForNthSync() {
    component_factory()->set_first_time_sync_configure_done(true);
    // Set first sync time before initialize to simulate a complete sync setup.
    SyncPrefs sync_prefs(prefs());
    sync_prefs.SetSyncRequested(true);
    sync_prefs.SetSelectedTypes(
        /*keep_everything_synced=*/true,
        /*registered_types=*/UserSelectableTypeSet::All(),
        /*selected_types=*/UserSelectableTypeSet::All());
    sync_prefs.SetFirstSetupComplete();
  }

  void InitializeForNthSync(bool run_until_idle = true) {
    PopulatePrefsForNthSync();
    service_->Initialize();
    if (run_until_idle) {
      task_environment_.RunUntilIdle();
    }
  }

  void InitializeForFirstSync(bool run_until_idle = true) {
    service_->Initialize();
    if (run_until_idle) {
      task_environment_.RunUntilIdle();
    }
  }

  void TriggerPassphraseRequired() {
    service_->GetEncryptionObserverForTest()->OnPassphraseRequired(
        KeyDerivationParams::CreateForPbkdf2(), sync_pb::EncryptedData());
  }

  void TriggerDataTypeStartRequest() {
    service_->OnDataTypeRequestsSyncStartup(BOOKMARKS);
  }

  signin::IdentityManager* identity_manager() {
    return sync_service_impl_bundle_.identity_manager();
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return sync_service_impl_bundle_.identity_test_env();
  }

  SyncServiceImpl* service() { return service_.get(); }

  SyncClientMock* sync_client() { return sync_client_; }

  TestingPrefServiceSimple* prefs() {
    return sync_service_impl_bundle_.pref_service();
  }

  FakeSyncApiComponentFactory* component_factory() {
    return sync_service_impl_bundle_.component_factory();
  }

  DataTypeManagerImpl* data_type_manager() {
    return component_factory()->last_created_data_type_manager();
  }

  FakeSyncEngine* engine() {
    return component_factory()->last_created_engine();
  }

  MockSyncInvalidationsService* sync_invalidations_service() {
    return sync_service_impl_bundle_.sync_invalidations_service();
  }

  MockTrustedVaultClient* trusted_vault_client() {
    return sync_service_impl_bundle_.trusted_vault_client();
  }

  FakeDataTypeController* get_controller(ModelType type) {
    return controller_map_[type];
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  SyncServiceImplBundle sync_service_impl_bundle_;
  std::unique_ptr<SyncServiceImpl> service_;
  raw_ptr<SyncClientMock> sync_client_;  // Owned by |service_|.
  // The controllers are owned by |service_|.
  std::map<ModelType, FakeDataTypeController*> controller_map_;
};

class SyncServiceImplTestWithSyncInvalidationsServiceCreated
    : public SyncServiceImplTest {
 public:
  SyncServiceImplTestWithSyncInvalidationsServiceCreated() {
    override_features_.InitAndEnableFeature(
        switches::kSyncSendInterestedDataTypes);
  }

  ~SyncServiceImplTestWithSyncInvalidationsServiceCreated() override = default;

 private:
  base::test::ScopedFeatureList override_features_;
};

// Verify that the server URLs are sane.
TEST_F(SyncServiceImplTest, InitialState) {
  CreateService(SyncServiceImpl::MANUAL_START);
  InitializeForNthSync();
  const std::string& url = service()->GetSyncServiceUrlForDebugging().spec();
  EXPECT_TRUE(url == internal::kSyncServerUrl ||
              url == internal::kSyncDevServerUrl);
}

TEST_F(SyncServiceImplTest, SuccessfulInitialization) {
  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START);
  InitializeForNthSync();
  EXPECT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
}

TEST_F(SyncServiceImplTest, SuccessfulLocalBackendInitialization) {
  CreateServiceWithLocalSyncBackend();
  InitializeForNthSync();
  EXPECT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
}

// Verify that an initialization where first setup is not complete does not
// start up Sync-the-feature.
TEST_F(SyncServiceImplTest, NeedsConfirmation) {
  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START);

  // Mimic a sync cycle (transport-only) having completed earlier.
  SyncPrefs sync_prefs(prefs());
  sync_prefs.SetSyncRequested(true);
  sync_prefs.SetSelectedTypes(
      /*keep_everything_synced=*/true,
      /*registered_types=*/UserSelectableTypeSet::All(),
      /*selected_types=*/UserSelectableTypeSet::All());
  service()->Initialize();

  EXPECT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());

  // Sync should immediately start up in transport mode.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_FALSE(service()->IsSyncFeatureActive());
  EXPECT_FALSE(service()->IsSyncFeatureEnabled());
}

TEST_F(SyncServiceImplTest, ModelTypesForTransportMode) {
  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START);
  InitializeForNthSync();

  // Disable sync-the-feature.
  service()->GetUserSettings()->SetSyncRequested(false);
  ASSERT_FALSE(service()->IsSyncFeatureActive());
  ASSERT_FALSE(service()->IsSyncFeatureEnabled());

  // Sync-the-transport should become active again.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // ModelTypes for sync-the-feature are not configured.
  EXPECT_FALSE(service()->GetActiveDataTypes().Has(BOOKMARKS));

  // ModelTypes for sync-the-transport are configured.
  EXPECT_TRUE(service()->GetActiveDataTypes().Has(DEVICE_INFO));
}

// Verify that the SetSetupInProgress function call updates state
// and notifies observers.
TEST_F(SyncServiceImplTest, SetupInProgress) {
  CreateService(SyncServiceImpl::MANUAL_START);
  InitializeForFirstSync();

  TestSyncServiceObserver observer;
  service()->AddObserver(&observer);

  auto sync_blocker = service()->GetSetupInProgressHandle();
  EXPECT_TRUE(observer.setup_in_progress());
  sync_blocker.reset();
  EXPECT_FALSE(observer.setup_in_progress());

  service()->RemoveObserver(&observer);
}

// Verify that we wait for policies to load before starting the sync engine.
TEST_F(SyncServiceImplTest, WaitForPoliciesToStart) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(switches::kSyncRequiresPoliciesLoaded);
  std::unique_ptr<policy::PolicyServiceImpl> policy_service =
      policy::PolicyServiceImpl::CreateWithThrottledInitialization(
          policy::PolicyServiceImpl::Providers());

  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START, policy_service.get());
  InitializeForNthSync();
  EXPECT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::START_DEFERRED,
            service()->GetTransportState());

  EXPECT_EQ(
      syncer::UploadState::INITIALIZING,
      syncer::GetUploadToGoogleState(service(), syncer::ModelType::BOOKMARKS));

  policy_service->UnthrottleInitialization();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
}

// Verify that disable by enterprise policy works.
TEST_F(SyncServiceImplTest, DisabledByPolicyBeforeInit) {
  prefs()->SetManagedPref(prefs::kSyncManaged,
                          std::make_unique<base::Value>(true));
  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START);
  InitializeForNthSync();
  // Sync was disabled due to the policy, setting SyncRequested to false and
  // causing DISABLE_REASON_USER_CHOICE.
  EXPECT_EQ(SyncService::DisableReasonSet(
                SyncService::DISABLE_REASON_ENTERPRISE_POLICY,
                SyncService::DISABLE_REASON_USER_CHOICE),
            service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());
}

TEST_F(SyncServiceImplTest, DisabledByPolicyBeforeInitThenPolicyRemoved) {
  prefs()->SetManagedPref(prefs::kSyncManaged,
                          std::make_unique<base::Value>(true));
  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START);
  InitializeForNthSync();
  // Sync was disabled due to the policy, setting SyncRequested to false and
  // causing DISABLE_REASON_USER_CHOICE.
  EXPECT_EQ(SyncService::DisableReasonSet(
                SyncService::DISABLE_REASON_ENTERPRISE_POLICY,
                SyncService::DISABLE_REASON_USER_CHOICE),
            service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());

  // Remove the policy. Sync-the-feature is still disabled, sync-the-transport
  // can run.
  prefs()->SetManagedPref(prefs::kSyncManaged,
                          std::make_unique<base::Value>(false));
  EXPECT_EQ(
      SyncService::DisableReasonSet(SyncService::DISABLE_REASON_USER_CHOICE),
      service()->GetDisableReasons());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // Once we mark first setup complete again (it was cleared by the policy) and
  // set SyncRequested to true, sync starts up.
  service()->GetUserSettings()->SetSyncRequested(true);
  service()->GetUserSettings()->SetFirstSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_TRUE(service()->GetDisableReasons().Empty());
}

// Verify that disable by enterprise policy works even after the backend has
// been initialized.
TEST_F(SyncServiceImplTest, DisabledByPolicyAfterInit) {
  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START);
  InitializeForNthSync();

  ASSERT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  prefs()->SetManagedPref(prefs::kSyncManaged,
                          std::make_unique<base::Value>(true));

  // Sync was disabled due to the policy, setting SyncRequested to false and
  // causing DISABLE_REASON_USER_CHOICE.
  EXPECT_EQ(SyncService::DisableReasonSet(
                SyncService::DISABLE_REASON_ENTERPRISE_POLICY,
                SyncService::DISABLE_REASON_USER_CHOICE),
            service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());
}

TEST_F(SyncServiceImplTest,
       ShouldDisableSyncFeatureWhenSyncDisallowedByPlatform) {
  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START);
  InitializeForNthSync();

  ASSERT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  service()->SetSyncAllowedByPlatform(false);
  EXPECT_FALSE(service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(service()->IsSyncFeatureActive());
  // Sync-the-transport should become active again.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
}

// Exercises the SyncServiceImpl's code paths related to getting shut down
// before the backend initialize call returns.
TEST_F(SyncServiceImplTest, AbortedByShutdown) {
  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START);
  component_factory()->AllowFakeEngineInitCompletion(false);

  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::INITIALIZING,
            service()->GetTransportState());

  ShutdownAndDeleteService();
}

// Test SetSyncRequested(false) before we've initialized the backend.
TEST_F(SyncServiceImplTest, EarlyRequestStop) {
  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START);
  // Set up a fake sync engine that will not immediately finish initialization.
  component_factory()->AllowFakeEngineInitCompletion(false);
  InitializeForNthSync();

  ASSERT_EQ(SyncService::TransportState::INITIALIZING,
            service()->GetTransportState());

  // Request stop. This should immediately restart the service in standalone
  // transport mode.
  component_factory()->AllowFakeEngineInitCompletion(true);
  service()->GetUserSettings()->SetSyncRequested(false);
  EXPECT_EQ(
      SyncService::DisableReasonSet(SyncService::DISABLE_REASON_USER_CHOICE),
      service()->GetDisableReasons());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_FALSE(service()->IsSyncFeatureActive());
  EXPECT_FALSE(service()->IsSyncFeatureEnabled());

  // Request start. Now Sync-the-feature should start again.
  service()->GetUserSettings()->SetSyncRequested(true);
  EXPECT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_TRUE(service()->IsSyncFeatureActive());
  EXPECT_TRUE(service()->IsSyncFeatureEnabled());
}

// Test SetSyncRequested(false) after we've initialized the backend.
TEST_F(SyncServiceImplTest, DisableAndEnableSyncTemporarily) {
  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START);
  InitializeForNthSync();

  SyncPrefs sync_prefs(prefs());

  ASSERT_TRUE(sync_prefs.IsSyncRequested());
  ASSERT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  ASSERT_TRUE(service()->IsSyncFeatureActive());
  ASSERT_TRUE(service()->IsSyncFeatureEnabled());

  service()->GetUserSettings()->SetSyncRequested(false);
  EXPECT_FALSE(sync_prefs.IsSyncRequested());
  EXPECT_EQ(
      SyncService::DisableReasonSet(SyncService::DISABLE_REASON_USER_CHOICE),
      service()->GetDisableReasons());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_FALSE(service()->IsSyncFeatureActive());
  EXPECT_FALSE(service()->IsSyncFeatureEnabled());

  service()->GetUserSettings()->SetSyncRequested(true);
  EXPECT_TRUE(sync_prefs.IsSyncRequested());
  EXPECT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_TRUE(service()->IsSyncFeatureActive());
  EXPECT_TRUE(service()->IsSyncFeatureEnabled());
}

// Certain SyncServiceImpl tests don't apply to Chrome OS, for example
// things that deal with concepts like "signing out".
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SyncServiceImplTest, SignOutDisablesSyncTransportAndSyncFeature) {
  // Sign-in and enable sync.
  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START);
  InitializeForNthSync();
  ASSERT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // Sign-out.
  auto* account_mutator = identity_manager()->GetPrimaryAccountMutator();
  DCHECK(account_mutator) << "Account mutator should only be null on ChromeOS.";
  account_mutator->ClearPrimaryAccount(
      signin_metrics::SIGNOUT_TEST,
      signin_metrics::SignoutDelete::kIgnoreMetric);
  // Wait for SyncServiceImpl to be notified.
  base::RunLoop().RunUntilIdle();
  // SyncRequested was set to false, causing DISABLE_REASON_USER_CHOICE.
  EXPECT_EQ(
      SyncService::DisableReasonSet(SyncService::DISABLE_REASON_NOT_SIGNED_IN,
                                    SyncService::DISABLE_REASON_USER_CHOICE),
      service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());
}

TEST_F(SyncServiceImplTest,
       SignOutClearsSyncTransportDataAndSyncTheFeaturePrefs) {
  // Sign-in and enable sync.
  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START);
  InitializeForNthSync();
  ASSERT_TRUE(service()->GetUserSettings()->IsFirstSetupComplete());
  ASSERT_TRUE(service()->GetUserSettings()->IsSyncRequested());
  ASSERT_EQ(0, component_factory()->clear_transport_data_call_count());

  // Sign-out.
  auto* account_mutator = identity_manager()->GetPrimaryAccountMutator();
  DCHECK(account_mutator) << "Account mutator should only be null on ChromeOS.";
  account_mutator->ClearPrimaryAccount(
      signin_metrics::SIGNOUT_TEST,
      signin_metrics::SignoutDelete::kIgnoreMetric);
  // Wait for SyncServiceImpl to be notified.
  base::RunLoop().RunUntilIdle();
  // These are specific to sync-the-feature and should be cleared.
  EXPECT_FALSE(service()->GetUserSettings()->IsFirstSetupComplete());
  EXPECT_FALSE(service()->GetUserSettings()->IsSyncRequested());
  EXPECT_EQ(1, component_factory()->clear_transport_data_call_count());
}

TEST_F(SyncServiceImplTest, SyncRequestedSetToFalseIfStartsSignedOut) {
  // Set up bad state.
  SyncPrefs sync_prefs(prefs());
  sync_prefs.SetSyncRequested(true);

  CreateService(SyncServiceImpl::MANUAL_START);
  service()->Initialize();

  // There's no signed-in user, so SyncRequested should have been set to false.
  EXPECT_FALSE(service()->GetUserSettings()->IsSyncRequested());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(SyncServiceImplTest, GetSyncTokenStatus) {
  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START);
  InitializeForNthSync(/*run_until_idle=*/false);

  // Initial status: The Sync engine startup has not begun yet; no token request
  // has been sent.
  SyncTokenStatus token_status = service()->GetSyncTokenStatusForDebugging();
  ASSERT_EQ(CONNECTION_NOT_ATTEMPTED, token_status.connection_status);
  ASSERT_TRUE(token_status.connection_status_update_time.is_null());
  ASSERT_TRUE(token_status.token_request_time.is_null());
  ASSERT_TRUE(token_status.token_response_time.is_null());
  ASSERT_FALSE(token_status.has_token);

  // Sync engine startup as well as the actual token request take the form of
  // posted tasks. Run them.
  base::RunLoop().RunUntilIdle();

  // Now we should have an access token.
  token_status = service()->GetSyncTokenStatusForDebugging();
  EXPECT_TRUE(token_status.connection_status_update_time.is_null());
  EXPECT_FALSE(token_status.token_request_time.is_null());
  EXPECT_FALSE(token_status.token_response_time.is_null());
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            token_status.last_get_token_error);
  EXPECT_TRUE(token_status.next_token_request_time.is_null());
  EXPECT_TRUE(token_status.has_token);

  // Simulate an auth error.
  service()->OnConnectionStatusChange(CONNECTION_AUTH_ERROR);

  // This should get reflected in the status, and we should have dropped the
  // invalid access token.
  token_status = service()->GetSyncTokenStatusForDebugging();
  EXPECT_EQ(CONNECTION_AUTH_ERROR, token_status.connection_status);
  EXPECT_FALSE(token_status.connection_status_update_time.is_null());
  EXPECT_FALSE(token_status.token_request_time.is_null());
  EXPECT_FALSE(token_status.token_response_time.is_null());
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            token_status.last_get_token_error);
  EXPECT_FALSE(token_status.next_token_request_time.is_null());
  EXPECT_FALSE(token_status.has_token);

  // Simulate successful connection.
  service()->OnConnectionStatusChange(CONNECTION_OK);
  token_status = service()->GetSyncTokenStatusForDebugging();
  EXPECT_EQ(CONNECTION_OK, token_status.connection_status);
}

TEST_F(SyncServiceImplTest, RevokeAccessTokenFromTokenService) {
  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START);
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  const CoreAccountId primary_account_id =
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSync);

  // Make sure the expected account_id was passed to the SyncEngine.
  ASSERT_EQ(primary_account_id, engine()->authenticated_account_id());

  // At this point, the real SyncEngine would try to connect to the server, fail
  // (because it has no access token), and eventually call
  // OnConnectionStatusChange(CONNECTION_AUTH_ERROR). Since our fake SyncEngine
  // doesn't do any of this, call that explicitly here.
  service()->OnConnectionStatusChange(CONNECTION_AUTH_ERROR);

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(service()->GetAccessTokenForTest().empty());

  AccountInfo secondary_account_info =
      identity_test_env()->MakeAccountAvailable("test_user2@gmail.com");
  identity_test_env()->RemoveRefreshTokenForAccount(
      secondary_account_info.account_id);
  EXPECT_FALSE(service()->GetAccessTokenForTest().empty());

  identity_test_env()->RemoveRefreshTokenForPrimaryAccount();
  EXPECT_TRUE(service()->GetAccessTokenForTest().empty());
}

// Checks that CREDENTIALS_REJECTED_BY_CLIENT resets the access token and stops
// Sync. Regression test for https://crbug.com/824791.
TEST_F(SyncServiceImplTest, CredentialsRejectedByClient_StopSync) {
  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START);
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  TestSyncServiceObserver observer;
  service()->AddObserver(&observer);

  const CoreAccountId primary_account_id =
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSync);

  // Make sure the expected account_id was passed to the SyncEngine.
  ASSERT_EQ(primary_account_id, engine()->authenticated_account_id());

  // At this point, the real SyncEngine would try to connect to the server, fail
  // (because it has no access token), and eventually call
  // OnConnectionStatusChange(CONNECTION_AUTH_ERROR). Since our fake SyncEngine
  // doesn't do any of this, call that explicitly here.
  service()->OnConnectionStatusChange(CONNECTION_AUTH_ERROR);

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(service()->GetAccessTokenForTest().empty());
  ASSERT_EQ(GoogleServiceAuthError::AuthErrorNone(), service()->GetAuthError());
  ASSERT_EQ(GoogleServiceAuthError::AuthErrorNone(), observer.auth_error());

  // Simulate the credentials getting locally rejected by the client by setting
  // the refresh token to a special invalid value.
  identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();
  const GoogleServiceAuthError rejected_by_client =
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_CLIENT);
  ASSERT_EQ(rejected_by_client,
            identity_test_env()
                ->identity_manager()
                ->GetErrorStateOfRefreshTokenForAccount(primary_account_id));
  EXPECT_TRUE(service()->GetAccessTokenForTest().empty());

  // The observer should have been notified of the auth error state.
  EXPECT_EQ(rejected_by_client, observer.auth_error());
  // The Sync engine should have been shut down.
  EXPECT_FALSE(service()->IsEngineInitialized());
  EXPECT_EQ(SyncService::TransportState::PAUSED,
            service()->GetTransportState());

  service()->RemoveObserver(&observer);
}

// CrOS does not support signout.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SyncServiceImplTest, SignOutRevokeAccessToken) {
  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START);
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  const CoreAccountId primary_account_id =
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSync);

  // Make sure the expected account_id was passed to the SyncEngine.
  ASSERT_EQ(primary_account_id, engine()->authenticated_account_id());

  // At this point, the real SyncEngine would try to connect to the server, fail
  // (because it has no access token), and eventually call
  // OnConnectionStatusChange(CONNECTION_AUTH_ERROR). Since our fake SyncEngine
  // doesn't do any of this, call that explicitly here.
  service()->OnConnectionStatusChange(CONNECTION_AUTH_ERROR);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(service()->GetAccessTokenForTest().empty());

  auto* account_mutator = identity_manager()->GetPrimaryAccountMutator();

  // GetPrimaryAccountMutator() returns nullptr on ChromeOS only.
  DCHECK(account_mutator);

  account_mutator->ClearPrimaryAccount(
      signin_metrics::SIGNOUT_TEST,
      signin_metrics::SignoutDelete::kIgnoreMetric);
  EXPECT_TRUE(service()->GetAccessTokenForTest().empty());
}
#endif

TEST_F(SyncServiceImplTest, StopAndClearWillClearDataAndSwitchToTransportMode) {
  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START);
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  ASSERT_EQ(0, component_factory()->clear_transport_data_call_count());

  service()->StopAndClear();

  // Even though Sync-the-feature is disabled, there's still an (unconsented)
  // signed-in account, so Sync-the-transport should still be running.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_FALSE(service()->IsSyncFeatureEnabled());
  EXPECT_EQ(1, component_factory()->clear_transport_data_call_count());
}

// Verify that sync transport data is cleared when the service is initializing
// and account is signed out.
TEST_F(SyncServiceImplTest, ClearTransportDataOnInitializeWhenSignedOut) {
  // Clearing prefs can be triggered only after `IdentityManager` finishes
  // loading the list of accounts, so wait for it to complete.
  identity_test_env()->WaitForRefreshTokensLoaded();

  // Don't sign-in before creating the service.
  CreateService(SyncServiceImpl::MANUAL_START);

  ASSERT_EQ(0, component_factory()->clear_transport_data_call_count());

  // Initialize when signed out to trigger clearing of prefs.
  InitializeForNthSync();

  EXPECT_EQ(1, component_factory()->clear_transport_data_call_count());
}

TEST_F(SyncServiceImplTest, StopSyncAndClearTwiceDoesNotCrash) {
  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START);
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // Disable sync.
  service()->StopAndClear();
  EXPECT_FALSE(service()->IsSyncFeatureEnabled());

  // Calling StopAndClear while already stopped should not crash. This may
  // (under some circumstances) happen when the user enables sync again but hits
  // the cancel button at the end of the process.
  ASSERT_FALSE(service()->GetUserSettings()->IsSyncRequested());
  service()->StopAndClear();
  EXPECT_FALSE(service()->IsSyncFeatureEnabled());
}

// Verify that credential errors get returned from GetAuthError().
TEST_F(SyncServiceImplTest, CredentialErrorReturned) {
  // This test needs to manually send access tokens (or errors), so disable
  // automatic replies to access token requests.
  identity_test_env()->SetAutomaticIssueOfAccessTokens(false);

  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START);
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  const CoreAccountId primary_account_id =
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSync);

  // Make sure the expected account_id was passed to the SyncEngine.
  ASSERT_EQ(primary_account_id, engine()->authenticated_account_id());

  TestSyncServiceObserver observer;
  service()->AddObserver(&observer);

  // At this point, the real SyncEngine would try to connect to the server, fail
  // (because it has no access token), and eventually call
  // OnConnectionStatusChange(CONNECTION_AUTH_ERROR). Since our fake SyncEngine
  // doesn't do any of this, call that explicitly here.
  service()->OnConnectionStatusChange(CONNECTION_AUTH_ERROR);

  // Wait for SyncServiceImpl to send an access token request.
  base::RunLoop().RunUntilIdle();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      primary_account_id, "access token", base::Time::Max());
  ASSERT_FALSE(service()->GetAccessTokenForTest().empty());
  ASSERT_EQ(GoogleServiceAuthError::NONE, service()->GetAuthError().state());

  // Emulate Chrome receiving a new, invalid LST. This happens when the user
  // signs out of the content area.
  identity_test_env()->SetRefreshTokenForPrimaryAccount();
  // Again, wait for SyncServiceImpl to be notified.
  base::RunLoop().RunUntilIdle();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  // Check that the invalid token is returned from sync.
  EXPECT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            service()->GetAuthError().state());
  EXPECT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            observer.auth_error().state());
  // The overall state should remain ACTIVE.
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  service()->RemoveObserver(&observer);
}

// Verify that credential errors get cleared when a new token is fetched
// successfully.
TEST_F(SyncServiceImplTest, CredentialErrorClearsOnNewToken) {
  // This test needs to manually send access tokens (or errors), so disable
  // automatic replies to access token requests.
  identity_test_env()->SetAutomaticIssueOfAccessTokens(false);

  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START);
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  const CoreAccountId primary_account_id =
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSync);

  // Make sure the expected account_id was passed to the SyncEngine.
  ASSERT_EQ(primary_account_id, engine()->authenticated_account_id());

  TestSyncServiceObserver observer;
  service()->AddObserver(&observer);

  // At this point, the real SyncEngine would try to connect to the server, fail
  // (because it has no access token), and eventually call
  // OnConnectionStatusChange(CONNECTION_AUTH_ERROR). Since our fake SyncEngine
  // doesn't do any of this, call that explicitly here.
  service()->OnConnectionStatusChange(CONNECTION_AUTH_ERROR);

  // Wait for SyncServiceImpl to send an access token request.
  base::RunLoop().RunUntilIdle();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      primary_account_id, "access token", base::Time::Max());
  ASSERT_FALSE(service()->GetAccessTokenForTest().empty());
  ASSERT_EQ(GoogleServiceAuthError::NONE, service()->GetAuthError().state());

  // Emulate Chrome receiving a new, invalid LST. This happens when the user
  // signs out of the content area.
  identity_test_env()->SetRefreshTokenForPrimaryAccount();
  // Wait for SyncServiceImpl to be notified of the changed credentials and
  // send a new access token request.
  base::RunLoop().RunUntilIdle();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  // Check that the invalid token is returned from sync.
  ASSERT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            service()->GetAuthError().state());
  // The overall state should remain ACTIVE.
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // Now emulate Chrome receiving a new, valid LST.
  identity_test_env()->SetRefreshTokenForPrimaryAccount();
  // Again, wait for SyncServiceImpl to be notified.
  base::RunLoop().RunUntilIdle();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "this one works", base::Time::Now() + base::Days(10));

  // Check that sync auth error state cleared.
  EXPECT_EQ(GoogleServiceAuthError::NONE, service()->GetAuthError().state());
  EXPECT_EQ(GoogleServiceAuthError::NONE, observer.auth_error().state());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  service()->RemoveObserver(&observer);
}

// Verify that the disable sync flag disables sync.
TEST_F(SyncServiceImplTest, DisableSyncFlag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kDisableSync);
  EXPECT_FALSE(switches::IsSyncAllowedByFlag());
}

// Verify that no disable sync flag enables sync.
TEST_F(SyncServiceImplTest, NoDisableSyncFlag) {
  EXPECT_TRUE(switches::IsSyncAllowedByFlag());
}

// Test that when SyncServiceImpl receives actionable error
// RESET_LOCAL_SYNC_DATA it restarts sync.
TEST_F(SyncServiceImplTest, ResetSyncData) {
  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START);
  // Backend should get initialized two times: once during initialization and
  // once when handling actionable error.
  InitializeForNthSync();

  SyncProtocolError client_cmd;
  client_cmd.action = RESET_LOCAL_SYNC_DATA;
  service()->OnActionableError(client_cmd);
}

// Test that when SyncServiceImpl receives actionable error
// DISABLE_SYNC_ON_CLIENT it disables sync and signs out.
TEST_F(SyncServiceImplTest, DisableSyncOnClient) {
  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START);
  InitializeForNthSync();

  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  ASSERT_EQ(0, get_controller(BOOKMARKS)->model()->clear_metadata_call_count());

  EXPECT_CALL(*trusted_vault_client(),
              ClearDataForAccount(Eq(identity_manager()->GetPrimaryAccountInfo(
                  signin::ConsentLevel::kSync))));

  SyncProtocolError client_cmd;
  client_cmd.action = DISABLE_SYNC_ON_CLIENT;
  service()->OnActionableError(client_cmd);

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // ChromeOS does not support signout.
  // TODO(https://crbug.com/1233933): Update this when Lacros profiles support
  //                                  signed-in-but-not-consented-to-sync state.
  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_EQ(
      SyncService::DisableReasonSet(SyncService::DISABLE_REASON_USER_CHOICE),
      service()->GetDisableReasons());
  // Since ChromeOS doesn't support signout and so the account is still there
  // and available, Sync will restart in standalone transport mode.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
#else
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_EQ(
      SyncService::DisableReasonSet(SyncService::DISABLE_REASON_NOT_SIGNED_IN,
                                    SyncService::DISABLE_REASON_USER_CHOICE),
      service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());
  EXPECT_TRUE(service()->GetLastSyncedTimeForDebugging().is_null());
#endif

  EXPECT_EQ(1, get_controller(BOOKMARKS)->model()->clear_metadata_call_count());

  EXPECT_FALSE(service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(service()->IsSyncFeatureActive());
}

// Verify a that local sync mode isn't impacted by sync being disabled.
TEST_F(SyncServiceImplTest, LocalBackendUnimpactedByPolicy) {
  prefs()->SetManagedPref(prefs::kSyncManaged,
                          std::make_unique<base::Value>(false));
  CreateServiceWithLocalSyncBackend();
  InitializeForNthSync();
  EXPECT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  prefs()->SetManagedPref(prefs::kSyncManaged,
                          std::make_unique<base::Value>(true));

  EXPECT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // Note: If standalone transport is enabled, then setting kSyncManaged to
  // false will immediately start up the engine. Otherwise, the RequestStart
  // call below will trigger it.
  prefs()->SetManagedPref(prefs::kSyncManaged,
                          std::make_unique<base::Value>(false));

  service()->GetUserSettings()->SetSyncRequested(true);
  EXPECT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
}

// Test ConfigureDataTypeManagerReason on First and Nth start.
TEST_F(SyncServiceImplTest, ConfigureDataTypeManagerReason) {
  SignIn();

  // First sync.
  CreateService(SyncServiceImpl::MANUAL_START);
  InitializeForFirstSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_EQ(CONFIGURE_REASON_NEW_CLIENT,
            data_type_manager()->last_configure_reason_for_test());

  // Reconfiguration.
  // Trigger a reconfig by grabbing a SyncSetupInProgressHandle and immediately
  // releasing it again (via the temporary unique_ptr going away).
  service()->GetSetupInProgressHandle();
  EXPECT_EQ(CONFIGURE_REASON_RECONFIGURATION,
            data_type_manager()->last_configure_reason_for_test());
  ShutdownAndDeleteService();

  // Nth sync.
  CreateService(SyncServiceImpl::MANUAL_START);
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_EQ(CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE,
            data_type_manager()->last_configure_reason_for_test());

  // Reconfiguration.
  // Trigger a reconfig by grabbing a SyncSetupInProgressHandle and immediately
  // releasing it again (via the temporary unique_ptr going away).
  service()->GetSetupInProgressHandle();
  EXPECT_EQ(CONFIGURE_REASON_RECONFIGURATION,
            data_type_manager()->last_configure_reason_for_test());
  ShutdownAndDeleteService();
}

// Regression test for crbug.com/1043642, can be removed once
// SyncServiceImpl usages after shutdown are addressed.
TEST_F(SyncServiceImplTest, ShouldProvideDisableReasonsAfterShutdown) {
  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START);
  InitializeForFirstSync();
  service()->Shutdown();
  EXPECT_FALSE(service()->GetDisableReasons().Empty());
}

#if defined(OS_ANDROID)
TEST_F(SyncServiceImplTest, DecoupleFromMasterSyncIfInitializedSignedOut) {
  SyncPrefs sync_prefs(prefs());
  CreateService(SyncServiceImpl::MANUAL_START);
  ASSERT_FALSE(sync_prefs.GetDecoupledFromAndroidMasterSync());

  service()->Initialize();
  EXPECT_TRUE(sync_prefs.GetDecoupledFromAndroidMasterSync());
}

TEST_F(SyncServiceImplTest, DecoupleFromMasterSyncIfSignsOut) {
  SyncPrefs sync_prefs(prefs());
  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START);
  InitializeForNthSync();
  ASSERT_FALSE(sync_prefs.GetDecoupledFromAndroidMasterSync());

  // Sign-out.
  auto* account_mutator = identity_manager()->GetPrimaryAccountMutator();
  DCHECK(account_mutator) << "Account mutator should only be null on ChromeOS.";
  account_mutator->ClearPrimaryAccount(
      signin_metrics::SIGNOUT_TEST,
      signin_metrics::SignoutDelete::kIgnoreMetric);
  // Wait for SyncServiceImpl to be notified.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(sync_prefs.GetDecoupledFromAndroidMasterSync());
}
#endif  // defined(OS_ANDROID)

TEST_F(SyncServiceImplTestWithSyncInvalidationsServiceCreated,
       ShouldSendDataTypesToSyncInvalidationsService) {
  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START);
  EXPECT_CALL(*sync_invalidations_service(), SetInterestedDataTypes);
  InitializeForFirstSync();
}

MATCHER(ContainsSessions, "") {
  return arg.Has(SESSIONS);
}

TEST_F(SyncServiceImplTestWithSyncInvalidationsServiceCreated,
       ShouldEnableAndDisableInvalidationsForSessions) {
  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START, nullptr,
                {{SESSIONS, false}, {TYPED_URLS, false}});
  InitializeForNthSync();

  EXPECT_CALL(*sync_invalidations_service(),
              SetInterestedDataTypes(ContainsSessions()));
  service()->SetInvalidationsForSessionsEnabled(true);
  EXPECT_CALL(*sync_invalidations_service(),
              SetInterestedDataTypes(Not(ContainsSessions())));
  service()->SetInvalidationsForSessionsEnabled(false);
}

TEST_F(SyncServiceImplTestWithSyncInvalidationsServiceCreated,
       ShouldActivateSyncInvalidationsServiceWhenSyncIsInitialized) {
  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START);
  EXPECT_CALL(*sync_invalidations_service(), SetActive(true)).Times(0);
  EXPECT_CALL(*sync_invalidations_service(), SetActive(true));
  InitializeForFirstSync();
}

TEST_F(SyncServiceImplTestWithSyncInvalidationsServiceCreated,
       ShouldActivateSyncInvalidationsServiceOnSignIn) {
  CreateService(SyncServiceImpl::MANUAL_START);
  EXPECT_CALL(*sync_invalidations_service(), SetActive(false))
      .Times(AnyNumber());
  InitializeForFirstSync();
  EXPECT_CALL(*sync_invalidations_service(), SetActive(true));
  SignIn();
}

// CrOS does not support signout.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SyncServiceImplTestWithSyncInvalidationsServiceCreated,
       ShouldDectivateSyncInvalidationsServiceOnSignOut) {
  SignIn();
  CreateService(SyncServiceImpl::MANUAL_START);
  EXPECT_CALL(*sync_invalidations_service(), SetActive(true));
  InitializeForFirstSync();

  auto* account_mutator = identity_manager()->GetPrimaryAccountMutator();
  // GetPrimaryAccountMutator() returns nullptr on ChromeOS only.
  DCHECK(account_mutator);

  // Sign out.
  EXPECT_CALL(*sync_invalidations_service(), SetActive(false));
  account_mutator->ClearPrimaryAccount(
      signin_metrics::SIGNOUT_TEST,
      signin_metrics::SignoutDelete::kIgnoreMetric);
}
#endif

}  // namespace
}  // namespace syncer
