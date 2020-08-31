// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/chromeos/arc/arc_optin_uma.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/optin/arc_terms_of_service_oobe_negotiator.h"
#include "chrome/browser/chromeos/arc/session/arc_play_store_enabled_preference_handler.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/test/arc_data_removed_waiter.h"
#include "chrome/browser/chromeos/arc/test/test_arc_session_manager.h"
#include "chrome/browser/chromeos/login/ui/fake_login_display_host.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/policy/powerwash_requirements_checker.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/webui/chromeos/login/arc_terms_of_service_screen_handler.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "components/account_id/account_id.h"
#include "components/arc/arc_features.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_session_runner.h"
#include "components/arc/test/fake_arc_session.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/sync/model/fake_sync_change_processor.h"
#include "components/sync/model/sync_error_factory_mock.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

class ArcInitialStartHandler : public ArcSessionManager::Observer {
 public:
  explicit ArcInitialStartHandler(ArcSessionManager* session_manager)
      : session_manager_(session_manager) {
    session_manager->AddObserver(this);
  }

  ~ArcInitialStartHandler() override { session_manager_->RemoveObserver(this); }

  // ArcSessionManager::Observer:
  void OnArcInitialStart() override {
    DCHECK(!was_called_);
    was_called_ = true;
  }

  bool was_called() const { return was_called_; }

 private:
  bool was_called_ = false;

  ArcSessionManager* const session_manager_;

  DISALLOW_COPY_AND_ASSIGN(ArcInitialStartHandler);
};

class FileExpansionObserver : public ArcSessionManager::Observer {
 public:
  FileExpansionObserver() = default;
  ~FileExpansionObserver() override = default;
  FileExpansionObserver(const FileExpansionObserver&) = delete;
  FileExpansionObserver& operator=(const FileExpansionObserver&) = delete;

  const base::Optional<bool>& property_files_expansion_result() const {
    return property_files_expansion_result_;
  }

  // ArcSessionManager::Observer:
  void OnPropertyFilesExpanded(bool result) override {
    property_files_expansion_result_ = result;
  }

 private:
  base::Optional<bool> property_files_expansion_result_;
};

class ArcSessionManagerInLoginScreenTest : public testing::Test {
 public:
  ArcSessionManagerInLoginScreenTest()
      : user_manager_enabler_(
            std::make_unique<chromeos::FakeChromeUserManager>()) {
    chromeos::SessionManagerClient::InitializeFakeInMemory();

    ArcSessionManager::SetUiEnabledForTesting(false);
    SetArcBlockedDueToIncompatibleFileSystemForTesting(false);

    arc_service_manager_ = std::make_unique<ArcServiceManager>();
    arc_session_manager_ =
        CreateTestArcSessionManager(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));
  }

  ~ArcSessionManagerInLoginScreenTest() override {
    arc_session_manager_->Shutdown();
    arc_session_manager_.reset();
    arc_service_manager_.reset();
    chromeos::SessionManagerClient::Shutdown();
  }

 protected:
  ArcSessionManager* arc_session_manager() {
    return arc_session_manager_.get();
  }

  FakeArcSession* arc_session() {
    return static_cast<FakeArcSession*>(
        arc_session_manager_->GetArcSessionRunnerForTesting()
            ->GetArcSessionForTesting());
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  user_manager::ScopedUserManager user_manager_enabler_;

  DISALLOW_COPY_AND_ASSIGN(ArcSessionManagerInLoginScreenTest);
};

// We expect mini instance starts to run if EmitLoginPromptVisible signal is
// emitted.
TEST_F(ArcSessionManagerInLoginScreenTest, EmitLoginPromptVisible) {
  EXPECT_FALSE(arc_session());

  SetArcAvailableCommandLineForTesting(base::CommandLine::ForCurrentProcess());

  chromeos::SessionManagerClient::Get()->EmitLoginPromptVisible();
  ASSERT_TRUE(arc_session());
  EXPECT_FALSE(arc_session()->is_running());
  EXPECT_EQ(ArcSessionManager::State::NOT_INITIALIZED,
            arc_session_manager()->state());
}

// We expect mini instance does not start on EmitLoginPromptVisible when ARC
// is not available.
TEST_F(ArcSessionManagerInLoginScreenTest, EmitLoginPromptVisible_NoOp) {
  EXPECT_FALSE(arc_session());

  chromeos::SessionManagerClient::Get()->EmitLoginPromptVisible();
  EXPECT_FALSE(arc_session());
  EXPECT_EQ(ArcSessionManager::State::NOT_INITIALIZED,
            arc_session_manager()->state());
}

class ArcSessionManagerTestBase : public testing::Test {
 public:
  ArcSessionManagerTestBase()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        user_manager_enabler_(
            std::make_unique<chromeos::FakeChromeUserManager>()) {}
  ~ArcSessionManagerTestBase() override = default;

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    chromeos::SessionManagerClient::InitializeFakeInMemory();
    chromeos::UpstartClient::InitializeFake();

    SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());
    ArcSessionManager::SetUiEnabledForTesting(false);
    SetArcBlockedDueToIncompatibleFileSystemForTesting(false);

    arc_service_manager_ = std::make_unique<ArcServiceManager>();
    arc_session_manager_ =
        CreateTestArcSessionManager(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("user@gmail.com");
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestArcProfile"));

    profile_ = profile_builder.Build();
    StartPreferenceSyncing();

    ASSERT_FALSE(arc_session_manager_->enable_requested());
  }

  void TearDown() override {
    arc_session_manager_->Shutdown();
    profile_.reset();
    arc_session_manager_.reset();
    arc_service_manager_.reset();
    chromeos::UpstartClient::Shutdown();
    chromeos::SessionManagerClient::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
  }

  chromeos::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<chromeos::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

 protected:
  TestingProfile* profile() { return profile_.get(); }

  ArcSessionManager* arc_session_manager() {
    return arc_session_manager_.get();
  }

  bool WaitForDataRemoved(ArcSessionManager::State expected_state) {
    if (arc_session_manager()->state() !=
        ArcSessionManager::State::REMOVING_DATA_DIR)
      return false;

    base::RunLoop().RunUntilIdle();
    if (arc_session_manager()->state() != expected_state)
      return false;

    return true;
  }

 private:
  void StartPreferenceSyncing() const {
    PrefServiceSyncableFromProfile(profile_.get())
        ->GetSyncableService(syncer::PREFERENCES)
        ->MergeDataAndStartSyncing(
            syncer::PREFERENCES, syncer::SyncDataList(),
            std::make_unique<syncer::FakeSyncChangeProcessor>(),
            std::make_unique<syncer::SyncErrorFactoryMock>());
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  user_manager::ScopedUserManager user_manager_enabler_;
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(ArcSessionManagerTestBase);
};

class ArcSessionManagerTest : public ArcSessionManagerTestBase {
 public:
  ArcSessionManagerTest() = default;

  void SetUp() override {
    ArcSessionManagerTestBase::SetUp();

    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        profile()->GetProfileUserName(), "1234567890"));
    GetFakeUserManager()->AddUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);

    chromeos::CryptohomeClient::InitializeFake();
    chromeos::FakeCryptohomeClient::Get()->set_requires_powerwash(false);
    policy::PowerwashRequirementsChecker::InitializeSynchronouslyForTesting();

    ASSERT_EQ(ArcSessionManager::State::NOT_INITIALIZED,
              arc_session_manager()->state());
  }

  void TearDown() override {
    chromeos::CryptohomeClient::Shutdown();
    ArcSessionManagerTestBase::TearDown();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcSessionManagerTest);
};

TEST_F(ArcSessionManagerTest, BaseWorkflow) {
  EXPECT_TRUE(arc_session_manager()->sign_in_start_time().is_null());
  EXPECT_TRUE(arc_session_manager()->arc_start_time().is_null());

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  // By default ARC is not enabled.
  EXPECT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());

  // Enables ARC. First time, ToS negotiation should start.
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager()->state());

  arc_session_manager()->OnTermsOfServiceNegotiatedForTesting(true);
  ASSERT_EQ(ArcSessionManager::State::CHECKING_ANDROID_MANAGEMENT,
            arc_session_manager()->state());
  EXPECT_TRUE(arc_session_manager()->sign_in_start_time().is_null());
  arc_session_manager()->StartArcForTesting();

  EXPECT_FALSE(arc_session_manager()->sign_in_start_time().is_null());
  EXPECT_FALSE(arc_session_manager()->arc_start_time().is_null());

  ASSERT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  arc_session_manager()->Shutdown();
}

// Tests that tying to enable ARC++ with an incompatible file system fails and
// shows the user a notification to that effect.
TEST_F(ArcSessionManagerTest, MigrationGuideNotification) {
  ArcSessionManager::SetUiEnabledForTesting(true);
  ArcSessionManager::EnableCheckAndroidManagementForTesting(false);
  SetArcBlockedDueToIncompatibleFileSystemForTesting(true);

  NotificationDisplayServiceTester notification_service(profile());

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());
  auto notifications = notification_service.GetDisplayedNotificationsForType(
      NotificationHandler::Type::TRANSIENT);
  ASSERT_EQ(1U, notifications.size());
  EXPECT_EQ("arc_fs_migration/suggest", notifications[0].id());
}

// Tests that OnArcInitialStart is called  after the successful ARC provisioning
// on the first start after OptIn.
TEST_F(ArcSessionManagerTest, ArcInitialStartFirstProvisioning) {
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  ArcInitialStartHandler start_handler(arc_session_manager());
  EXPECT_FALSE(start_handler.was_called());

  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(start_handler.was_called());

  arc_session_manager()->OnTermsOfServiceNegotiatedForTesting(true);
  arc_session_manager()->StartArcForTesting();

  EXPECT_FALSE(start_handler.was_called());

  arc_session_manager()->OnProvisioningFinished(ProvisioningResult::SUCCESS);
  EXPECT_TRUE(start_handler.was_called());

  arc_session_manager()->Shutdown();
}

// Tests that OnArcInitialStart is not called after the successful ARC
// provisioning on the second and next starts after OptIn.
TEST_F(ArcSessionManagerTest, ArcInitialStartNextProvisioning) {
  // Set up the situation that provisioning is successfully done in the
  // previous session. In this case initial start callback is not called.
  PrefService* const prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kArcTermsAccepted, true);
  prefs->SetBoolean(prefs::kArcSignedIn, true);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  ArcInitialStartHandler start_handler(arc_session_manager());

  arc_session_manager()->RequestEnable();
  arc_session_manager()->OnProvisioningFinished(ProvisioningResult::SUCCESS);
  EXPECT_FALSE(start_handler.was_called());

  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, IncompatibleFileSystemBlocksTermsOfService) {
  SetArcBlockedDueToIncompatibleFileSystemForTesting(true);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  // Enables ARC first time. ToS negotiation should NOT happen.
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());

  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, IncompatibleFileSystemBlocksArcStart) {
  SetArcBlockedDueToIncompatibleFileSystemForTesting(true);

  // Set up the situation that provisioning is successfully done in the
  // previous session.
  PrefService* const prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kArcTermsAccepted, true);
  prefs->SetBoolean(prefs::kArcSignedIn, true);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  // Enables ARC second time. ARC should NOT start.
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());

  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, CancelFetchingDisablesArc) {
  SetArcPlayStoreEnabledForProfile(profile(), true);

  // Starts ARC.
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager()->state());

  // Emulate to cancel the ToS UI (e.g. closing the window).
  arc_session_manager()->CancelAuthCode();

  // Google Play Store enabled preference should be set to false, too.
  EXPECT_FALSE(IsArcPlayStoreEnabledForProfile(profile()));

  // Emulate the preference handling.
  const bool enable_requested = arc_session_manager()->enable_requested();
  arc_session_manager()->RequestDisable();
  if (enable_requested)
    arc_session_manager()->RequestArcDataRemoval();

  // Wait until data is removed.
  ASSERT_TRUE(WaitForDataRemoved(ArcSessionManager::State::STOPPED));

  // Correctly stop service.
  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, CloseUIKeepsArcEnabled) {
  // Starts ARC.
  SetArcPlayStoreEnabledForProfile(profile(), true);
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager()->state());
  arc_session_manager()->OnTermsOfServiceNegotiatedForTesting(true);
  ASSERT_EQ(ArcSessionManager::State::CHECKING_ANDROID_MANAGEMENT,
            arc_session_manager()->state());
  arc_session_manager()->StartArcForTesting();
  ASSERT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // When ARC is properly started, closing UI should be no-op.
  arc_session_manager()->CancelAuthCode();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_TRUE(IsArcPlayStoreEnabledForProfile(profile()));

  // Correctly stop service.
  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, Provisioning_Success) {
  PrefService* const prefs = profile()->GetPrefs();

  EXPECT_TRUE(arc_session_manager()->sign_in_start_time().is_null());
  EXPECT_TRUE(arc_session_manager()->arc_start_time().is_null());
  EXPECT_FALSE(arc_session_manager()->IsPlaystoreLaunchRequestedForTesting());

  ASSERT_FALSE(prefs->GetBoolean(prefs::kArcSignedIn));

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  ASSERT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager()->state());

  // Emulate to accept the terms of service.
  arc_session_manager()->OnTermsOfServiceNegotiatedForTesting(true);
  ASSERT_EQ(ArcSessionManager::State::CHECKING_ANDROID_MANAGEMENT,
            arc_session_manager()->state());
  EXPECT_TRUE(arc_session_manager()->sign_in_start_time().is_null());
  arc_session_manager()->StartArcForTesting();
  EXPECT_FALSE(arc_session_manager()->sign_in_start_time().is_null());
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Here, provisining is not yet completed, so kArcSignedIn should be false.
  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcSignedIn));
  EXPECT_FALSE(arc_session_manager()->arc_start_time().is_null());
  EXPECT_FALSE(arc_session_manager()->IsPlaystoreLaunchRequestedForTesting());

  // Emulate successful provisioning.
  arc_session_manager()->OnProvisioningFinished(ProvisioningResult::SUCCESS);
  EXPECT_TRUE(prefs->GetBoolean(prefs::kArcSignedIn));
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_TRUE(arc_session_manager()->IsPlaystoreLaunchRequestedForTesting());
}

// Verifies that Play Store shown is suppressed on restart when required.
TEST_F(ArcSessionManagerTest, PlayStoreSuppressed) {
  // Set up the situation that terms were accepted in the previous session.
  PrefService* const prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kArcTermsAccepted, true);
  // Set the flag indicating that the provisioning was initiated from OOBE in
  // the previous session.
  prefs->SetBoolean(prefs::kArcProvisioningInitiatedFromOobe, true);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  arc_session_manager()->StartArcForTesting();

  // Second start, no fetching code is expected.
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_FALSE(arc_session_manager()->IsPlaystoreLaunchRequestedForTesting());
  arc_session_manager()->OnProvisioningFinished(ProvisioningResult::SUCCESS);
  // Completing the provisioning resets this flag.
  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcProvisioningInitiatedFromOobe));
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  // |prefs::kArcProvisioningInitiatedFromOobe| flag prevents opening the
  // Play Store.
  EXPECT_FALSE(arc_session_manager()->IsPlaystoreLaunchRequestedForTesting());

  // Correctly stop service.
  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, InitiatedFromOobeIsResetOnOptOut) {
  // Set up the situation that terms were accepted in the previous session.
  PrefService* const prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kArcTermsAccepted, true);
  // Set the flag indicating that the provisioning was initiated from OOBE in
  // the previous session.
  prefs->SetBoolean(prefs::kArcProvisioningInitiatedFromOobe, true);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  EXPECT_TRUE(prefs->GetBoolean(prefs::kArcProvisioningInitiatedFromOobe));
  // Disabling ARC resets suppress state
  arc_session_manager()->RequestDisable();
  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcProvisioningInitiatedFromOobe));

  // Correctly stop service.
  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, Provisioning_Restart) {
  // Set up the situation that provisioning is successfully done in the
  // previous session.
  PrefService* const prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kArcTermsAccepted, true);
  prefs->SetBoolean(prefs::kArcSignedIn, true);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();

  // Second start, no fetching code is expected.
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Report failure.
  arc_session_manager()->OnProvisioningFinished(
      ProvisioningResult::GMS_NETWORK_ERROR);
  // On error, UI to send feedback is showing. In that case,
  // the ARC is still necessary to run on background for gathering the logs.
  EXPECT_TRUE(prefs->GetBoolean(prefs::kArcSignedIn));
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Correctly stop service.
  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, RemoveDataDir) {
  // Emulate the situation where the initial Google Play Store enabled
  // preference is false for managed user, i.e., data dir is being removed at
  // beginning.
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestArcDataRemoval();
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));
  EXPECT_EQ(ArcSessionManager::State::REMOVING_DATA_DIR,
            arc_session_manager()->state());

  // Enable ARC. Data is removed asyncronously. At this moment session manager
  // should be in REMOVING_DATA_DIR state.
  arc_session_manager()->RequestEnable();
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));
  EXPECT_EQ(ArcSessionManager::State::REMOVING_DATA_DIR,
            arc_session_manager()->state());
  // Wait until data is removed.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));
  EXPECT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager()->state());
  arc_session_manager()->OnTermsOfServiceNegotiatedForTesting(true);
  ASSERT_EQ(ArcSessionManager::State::CHECKING_ANDROID_MANAGEMENT,
            arc_session_manager()->state());
  arc_session_manager()->StartArcForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Request to remove data and stop session manager.
  arc_session_manager()->RequestArcDataRemoval();
  ASSERT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  arc_session_manager()->Shutdown();
  base::RunLoop().RunUntilIdle();
  // Request should persist.
  ASSERT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));
}

TEST_F(ArcSessionManagerTest, RemoveDataDir_Restart) {
  // Emulate second sign-in. Data should be removed first and ARC started after.
  PrefService* const prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kArcDataRemoveRequested, true);
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));
  ASSERT_TRUE(WaitForDataRemoved(
      ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));

  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, RegularToChildTransition_FlagOn) {
  // Emulate the situation where a regular user has transitioned to a child
  // account.
  profile()->GetPrefs()->SetInteger(
      prefs::kArcSupervisionTransition,
      static_cast<int>(ArcSupervisionTransition::REGULAR_TO_CHILD));
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      kCleanArcDataOnRegularToChildTransitionFeature);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));
  EXPECT_EQ(
      static_cast<int>(ArcSupervisionTransition::NO_TRANSITION),
      profile()->GetPrefs()->GetInteger(prefs::kArcSupervisionTransition));
  EXPECT_EQ(ArcSessionManager::State::REMOVING_DATA_DIR,
            arc_session_manager()->state());

  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, RegularToChildTransition_FlagOff) {
  // Emulate the situation where a regular user has transitioned to a child
  // account, but the feature flag is disabled.
  profile()->GetPrefs()->SetInteger(
      prefs::kArcSupervisionTransition,
      static_cast<int>(ArcSupervisionTransition::REGULAR_TO_CHILD));
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      kCleanArcDataOnRegularToChildTransitionFeature);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));
  EXPECT_EQ(
      static_cast<int>(ArcSupervisionTransition::REGULAR_TO_CHILD),
      profile()->GetPrefs()->GetInteger(prefs::kArcSupervisionTransition));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager()->state());

  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, ClearArcTransitionOnShutdown) {
  profile()->GetPrefs()->SetInteger(
      prefs::kArcSupervisionTransition,
      static_cast<int>(ArcSupervisionTransition::NO_TRANSITION));

  // Initialize ARC.
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager()->state());
  arc_session_manager()->OnTermsOfServiceNegotiatedForTesting(true);
  arc_session_manager()->StartArcForTesting();
  arc_session_manager()->OnProvisioningFinished(ProvisioningResult::SUCCESS);

  EXPECT_EQ(
      static_cast<int>(ArcSupervisionTransition::NO_TRANSITION),
      profile()->GetPrefs()->GetInteger(prefs::kArcSupervisionTransition));

  // Child started graduation.
  profile()->GetPrefs()->SetInteger(
      prefs::kArcSupervisionTransition,
      static_cast<int>(ArcSupervisionTransition::CHILD_TO_REGULAR));
  // Simulate ARC shutdown.
  const bool enable_requested = arc_session_manager()->enable_requested();
  arc_session_manager()->RequestDisable();
  if (enable_requested)
    arc_session_manager()->RequestArcDataRemoval();
  EXPECT_EQ(
      static_cast<int>(ArcSupervisionTransition::NO_TRANSITION),
      profile()->GetPrefs()->GetInteger(prefs::kArcSupervisionTransition));

  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, ClearArcTransitionOnArcDataRemoval) {
  EXPECT_EQ(ArcSupervisionTransition::NO_TRANSITION,
            arc::GetSupervisionTransition(profile()));

  // Initialize ARC.
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager()->state());
  arc_session_manager()->OnTermsOfServiceNegotiatedForTesting(true);
  arc_session_manager()->StartArcForTesting();
  arc_session_manager()->OnProvisioningFinished(ProvisioningResult::SUCCESS);

  EXPECT_EQ(ArcSupervisionTransition::NO_TRANSITION,
            arc::GetSupervisionTransition(profile()));

  // Child started graduation.
  profile()->GetPrefs()->SetInteger(
      prefs::kArcSupervisionTransition,
      static_cast<int>(ArcSupervisionTransition::CHILD_TO_REGULAR));

  arc_session_manager()->RequestArcDataRemoval();
  EXPECT_EQ(ArcSupervisionTransition::NO_TRANSITION,
            arc::GetSupervisionTransition(profile()));

  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, IgnoreSecondErrorReporting) {
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  arc_session_manager()->OnTermsOfServiceNegotiatedForTesting(true);
  arc_session_manager()->StartArcForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Report some failure that does not stop the bridge.
  arc_session_manager()->OnProvisioningFinished(
      ProvisioningResult::GMS_SIGN_IN_FAILED);
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Try to send another error that stops the bridge if sent first. It should
  // be ignored.
  arc_session_manager()->OnProvisioningFinished(
      ProvisioningResult::CHROME_SERVER_COMMUNICATION_ERROR);
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  arc_session_manager()->Shutdown();
}

// Test case when directly started flag is not set during the ARC boot.
TEST_F(ArcSessionManagerTest, IsDirectlyStartedFalse) {
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  // On initial start directy started flag is not set.
  EXPECT_FALSE(arc_session_manager()->is_directly_started());
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(arc_session_manager()->is_directly_started());
  ASSERT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager()->state());
  arc_session_manager()->OnTermsOfServiceNegotiatedForTesting(true);
  arc_session_manager()->StartArcForTesting();
  arc_session_manager()->OnProvisioningFinished(ProvisioningResult::SUCCESS);
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_FALSE(arc_session_manager()->is_directly_started());
  arc_session_manager()->Shutdown();
  EXPECT_FALSE(arc_session_manager()->is_directly_started());
}

// Test case when directly started flag is set during the ARC boot.
// Preconditions are: ToS accepted and ARC was signed in.
TEST_F(ArcSessionManagerTest, IsDirectlyStartedTrue) {
  PrefService* const prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kArcTermsAccepted, true);
  prefs->SetBoolean(prefs::kArcSignedIn, true);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  EXPECT_FALSE(arc_session_manager()->is_directly_started());
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(arc_session_manager()->is_directly_started());
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Disabling ARC turns directy started flag off.
  arc_session_manager()->RequestDisable();
  EXPECT_FALSE(arc_session_manager()->is_directly_started());
  arc_session_manager()->Shutdown();
}

// Test case when directly started flag is preserved during the internal ARC
// restart.
TEST_F(ArcSessionManagerTest, IsDirectlyStartedOnInternalRestart) {
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager()->state());
  arc_session_manager()->OnTermsOfServiceNegotiatedForTesting(true);
  arc_session_manager()->StartArcForTesting();
  arc_session_manager()->OnProvisioningFinished(ProvisioningResult::SUCCESS);
  EXPECT_FALSE(arc_session_manager()->is_directly_started());
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_FALSE(arc_session_manager()->is_directly_started());

  // Simualate internal restart.
  arc_session_manager()->StopAndEnableArc();
  // Fake ARC session implementation synchronously calls stop callback and
  // session manager should be reactivated at this moment.
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  // directy started flag should be preserved.
  EXPECT_FALSE(arc_session_manager()->is_directly_started());
  arc_session_manager()->Shutdown();
}

// In case of the next start ArcSessionManager should go through remove data
// folder phase before negotiating terms of service.
TEST_F(ArcSessionManagerTest, DataCleanUpOnFirstStart) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitch(
      chromeos::switches::kArcDataCleanupOnStart);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  ArcPlayStoreEnabledPreferenceHandler handler(profile(),
                                               arc_session_manager());
  handler.Start();

  EXPECT_EQ(ArcSessionManager::State::REMOVING_DATA_DIR,
            arc_session_manager()->state());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());

  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager()->state());
  arc_session_manager()->OnTermsOfServiceNegotiatedForTesting(true);
  EXPECT_EQ(ArcSessionManager::State::CHECKING_ANDROID_MANAGEMENT,
            arc_session_manager()->state());
  arc_session_manager()->StartArcForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  arc_session_manager()->Shutdown();
}

// In case of the next start ArcSessionManager should go through remove data
// folder phase before activating.
TEST_F(ArcSessionManagerTest, DataCleanUpOnNextStart) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitch(
      chromeos::switches::kArcDataCleanupOnStart);

  PrefService* const prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kArcTermsAccepted, true);
  prefs->SetBoolean(prefs::kArcSignedIn, true);
  prefs->SetBoolean(prefs::kArcEnabled, true);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  ArcPlayStoreEnabledPreferenceHandler handler(profile(),
                                               arc_session_manager());
  handler.Start();

  EXPECT_EQ(ArcSessionManager::State::REMOVING_DATA_DIR,
            arc_session_manager()->state());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, RequestDisableDoesNotRemoveData) {
  // Start ARC.
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager()->state());

  // Disable ARC.
  arc_session_manager()->RequestDisable();

  // Data removal is not requested.
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));

  // Correctly stop service.
  arc_session_manager()->Shutdown();
}

class ArcSessionManagerArcAlwaysStartTest : public ArcSessionManagerTest {
 public:
  ArcSessionManagerArcAlwaysStartTest() = default;

  void SetUp() override {
    SetArcAlwaysStartWithoutPlayStoreForTesting();
    ArcSessionManagerTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcSessionManagerArcAlwaysStartTest);
};

TEST_F(ArcSessionManagerArcAlwaysStartTest, BaseWorkflow) {
  // TODO(victorhsieh): Consider also tracking sign-in activity, which is
  // initiated from the Android side.
  EXPECT_TRUE(arc_session_manager()->arc_start_time().is_null());

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  // By default ARC is not enabled.
  EXPECT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());

  // When ARC is always started, ArcSessionManager should always be in ACTIVE
  // state.
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_FALSE(arc_session_manager()->arc_start_time().is_null());

  arc_session_manager()->Shutdown();
}

class ArcSessionManagerPolicyTest
    : public ArcSessionManagerTestBase,
      public testing::WithParamInterface<
          std::tuple<bool, bool, bool, int, int>> {
 public:
  void SetUp() override {
    ArcSessionManagerTestBase::SetUp();
    AccountId account_id;
    if (is_active_directory_user()) {
      account_id = AccountId(AccountId::AdFromUserEmailObjGuid(
          profile()->GetProfileUserName(), "1234567890"));
    } else {
      account_id = AccountId(AccountId::FromUserEmailGaiaId(
          profile()->GetProfileUserName(), "1234567890"));
    }
    GetFakeUserManager()->AddUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);
    // Mocks OOBE environment so that IsArcOobeOptInActive() returns true.
    if (is_oobe_optin()) {
      GetFakeUserManager()->set_current_user_new(true);
      CreateLoginDisplayHost();
    }
  }

  void TearDown() override {
    if (is_oobe_optin()) {
      fake_login_display_host_.reset();
    }
    ArcSessionManagerTestBase::TearDown();
  }

  bool arc_enabled_pref_managed() const { return std::get<0>(GetParam()); }

  bool is_active_directory_user() const { return std::get<1>(GetParam()); }

  bool is_oobe_optin() const { return std::get<2>(GetParam()); }

  base::Value backup_restore_pref_value() const {
    switch (std::get<3>(GetParam())) {
      case 0:
        return base::Value();
      case 1:
        return base::Value(false);
      case 2:
        return base::Value(true);
    }
    NOTREACHED();
    return base::Value();
  }

  base::Value location_service_pref_value() const {
    switch (std::get<4>(GetParam())) {
      case 0:
        return base::Value();
      case 1:
        return base::Value(false);
      case 2:
        return base::Value(true);
    }
    NOTREACHED();
    return base::Value();
  }

 private:
  void CreateLoginDisplayHost() {
    fake_login_display_host_ =
        std::make_unique<chromeos::FakeLoginDisplayHost>();
  }

  std::unique_ptr<chromeos::FakeLoginDisplayHost> fake_login_display_host_;
};

TEST_P(ArcSessionManagerPolicyTest, SkippingTerms) {
  sync_preferences::TestingPrefServiceSyncable* const prefs =
      profile()->GetTestingPrefService();

  // Backup-restore and location-service prefs are off by default.
  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcSignedIn));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcTermsAccepted));

  EXPECT_EQ(is_active_directory_user(),
            IsActiveDirectoryUserForProfile(profile()));

  // Enable ARC through user pref or by policy, according to the test parameter.
  if (arc_enabled_pref_managed())
    prefs->SetManagedPref(prefs::kArcEnabled,
                          std::make_unique<base::Value>(true));
  else
    prefs->SetBoolean(prefs::kArcEnabled, true);
  EXPECT_TRUE(IsArcPlayStoreEnabledForProfile(profile()));

  // Assign test values to the prefs.
  if (backup_restore_pref_value().is_bool()) {
    prefs->SetManagedPref(prefs::kArcBackupRestoreEnabled,
                          backup_restore_pref_value().CreateDeepCopy());
  }
  if (location_service_pref_value().is_bool()) {
    prefs->SetManagedPref(prefs::kArcLocationServiceEnabled,
                          location_service_pref_value().CreateDeepCopy());
  }

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();

  // Terms of Service are skipped if ARC is enabled by policy and both policies
  // are either managed or unused (for Active Directory users a LaForge
  // account is created, not a full Dasher account, where the policies have no
  // meaning).
  // Terms of Service are skipped if ARC is enabled by policy and if it's in
  // session opt-in.
  const bool prefs_unused = is_active_directory_user();
  const bool backup_managed = backup_restore_pref_value().is_bool();
  const bool location_managed = location_service_pref_value().is_bool();
  const bool is_arc_oobe_optin = is_oobe_optin();
  const bool expected_terms_skipping =
      arc_enabled_pref_managed() && ((backup_managed && location_managed) ||
                                     prefs_unused || !is_arc_oobe_optin);
  EXPECT_EQ(expected_terms_skipping
                ? ArcSessionManager::State::CHECKING_ANDROID_MANAGEMENT
                : ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager()->state());
  EXPECT_EQ(IsArcOobeOptInActive(), is_arc_oobe_optin);

  // Complete provisioning if it's not done yet.
  if (!expected_terms_skipping)
    arc_session_manager()->OnTermsOfServiceNegotiatedForTesting(true);

  arc_session_manager()->StartArcForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  arc_session_manager()->OnProvisioningFinished(ProvisioningResult::SUCCESS);

  // Play Store app is launched unless the Terms screen was suppressed or Tos is
  // accepted during OOBE.
  EXPECT_NE(expected_terms_skipping || is_arc_oobe_optin,
            arc_session_manager()->IsPlaystoreLaunchRequestedForTesting());

  // In case Tos is skipped, B&R and GLS should not be set if not managed.
  if (expected_terms_skipping) {
    if (!backup_managed)
      EXPECT_FALSE(prefs->GetBoolean(prefs::kArcBackupRestoreEnabled));
    if (!location_managed)
      EXPECT_FALSE(prefs->GetBoolean(prefs::kArcLocationServiceEnabled));
  }

  // Managed values for the prefs are unset.
  prefs->RemoveManagedPref(prefs::kArcBackupRestoreEnabled);
  prefs->RemoveManagedPref(prefs::kArcLocationServiceEnabled);

  // The ARC state is preserved. The prefs return to the default false values.
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcBackupRestoreEnabled));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcLocationServiceEnabled));

  // Stop ARC and shutdown the service.
  prefs->RemoveManagedPref(prefs::kArcEnabled);
  WaitForDataRemoved(ArcSessionManager::State::STOPPED);
  arc_session_manager()->Shutdown();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ArcSessionManagerPolicyTest,
    // testing::Values is incompatible with move-only types, hence ints are used
    // as a proxy for base::Value.
    testing::Combine(testing::Bool() /* arc_enabled_pref_managed */,
                     testing::Bool() /* is_active_directory_user */,
                     testing::Bool() /* is_oobe_optin */,
                     /* backup_restore_pref_value */
                     testing::Values(0,   // base::Value()
                                     1,   // base::Value(false)
                                     2),  // base::Value(true)
                     /* location_service_pref_value */
                     testing::Values(0,     // base::Value()
                                     1,     // base::Value(false)
                                     2)));  // base::Value(true)

class ArcSessionManagerKioskTest : public ArcSessionManagerTestBase {
 public:
  ArcSessionManagerKioskTest() = default;

  void SetUp() override {
    ArcSessionManagerTestBase::SetUp();
    const AccountId account_id(
        AccountId::FromUserEmail(profile()->GetProfileUserName()));
    GetFakeUserManager()->AddArcKioskAppUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcSessionManagerKioskTest);
};

TEST_F(ArcSessionManagerKioskTest, AuthFailure) {
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Replace chrome::AttemptUserExit() for testing.
  // At the end of test, leave the dangling pointer |terminated|,
  // assuming the callback is invoked exactly once in OnProvisioningFinished()
  // and not invoked then, including TearDown().
  bool terminated = false;
  arc_session_manager()->SetAttemptUserExitCallbackForTesting(
      base::Bind([](bool* terminated) { *terminated = true; }, &terminated));

  arc_session_manager()->OnProvisioningFinished(
      ProvisioningResult::CHROME_SERVER_COMMUNICATION_ERROR);
  EXPECT_TRUE(terminated);
}

class ArcSessionManagerPublicSessionTest : public ArcSessionManagerTestBase {
 public:
  ArcSessionManagerPublicSessionTest() = default;

  void SetUp() override {
    ArcSessionManagerTestBase::SetUp();
    const AccountId account_id(
        AccountId::FromUserEmail(profile()->GetProfileUserName()));
    GetFakeUserManager()->AddPublicAccountUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcSessionManagerPublicSessionTest);
};

TEST_F(ArcSessionManagerPublicSessionTest, AuthFailure) {
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Replace chrome::AttemptUserExit() for testing.
  // At the end of test, leave the dangling pointer |terminated|,
  // assuming the callback is never invoked in OnProvisioningFinished()
  // and not invoked then, including TearDown().
  bool terminated = false;
  arc_session_manager()->SetAttemptUserExitCallbackForTesting(
      base::BindRepeating([](bool* terminated) { *terminated = true; },
                          &terminated));

  arc_session_manager()->OnProvisioningFinished(
      ProvisioningResult::CHROME_SERVER_COMMUNICATION_ERROR);
  EXPECT_FALSE(terminated);
  EXPECT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());
}

class ArcSessionOobeOptInNegotiatorTest
    : public ArcSessionManagerTest,
      public chromeos::ArcTermsOfServiceScreenView,
      public testing::WithParamInterface<bool> {
 public:
  ArcSessionOobeOptInNegotiatorTest() = default;

  void SetUp() override {
    ArcSessionManagerTest::SetUp();

    ArcSessionManager::SetArcTermsOfServiceOobeNegotiatorEnabledForTesting(
        true);
    ArcTermsOfServiceOobeNegotiator::SetArcTermsOfServiceScreenViewForTesting(
        this);

    GetFakeUserManager()->set_current_user_new(true);

    CreateLoginDisplayHost();

    if (IsManagedUser()) {
      policy::ProfilePolicyConnector* const connector =
          profile()->GetProfilePolicyConnector();
      connector->OverrideIsManagedForTesting(true);

      profile()->GetTestingPrefService()->SetManagedPref(
          prefs::kArcEnabled, std::make_unique<base::Value>(true));
    }

    arc_session_manager()->SetProfile(profile());
    arc_session_manager()->Initialize();

    if (IsArcPlayStoreEnabledForProfile(profile()))
      arc_session_manager()->RequestEnable();
  }

  void TearDown() override {
    // Correctly stop service.
    arc_session_manager()->Shutdown();

    ArcTermsOfServiceOobeNegotiator::SetArcTermsOfServiceScreenViewForTesting(
        nullptr);
    ArcSessionManager::SetArcTermsOfServiceOobeNegotiatorEnabledForTesting(
        false);

    ArcSessionManagerTest::TearDown();
  }

 protected:
  bool IsManagedUser() { return GetParam(); }

  void ReportAccepted() {
    for (auto& observer : observer_list_) {
      observer.OnAccept(false);
    }
    base::RunLoop().RunUntilIdle();
  }

  void ReportViewDestroyed() {
    for (auto& observer : observer_list_)
      observer.OnViewDestroyed(this);
    base::RunLoop().RunUntilIdle();
  }

  void CreateLoginDisplayHost() {
    fake_login_display_host_ =
        std::make_unique<chromeos::FakeLoginDisplayHost>();
  }

  chromeos::FakeLoginDisplayHost* login_display_host() {
    return fake_login_display_host_.get();
  }

  void CloseLoginDisplayHost() { fake_login_display_host_.reset(); }

  chromeos::ArcTermsOfServiceScreenView* view() { return this; }

 private:
  // ArcTermsOfServiceScreenView:
  void AddObserver(
      chromeos::ArcTermsOfServiceScreenViewObserver* observer) override {
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(
      chromeos::ArcTermsOfServiceScreenViewObserver* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  void Show() override {
    // To match ArcTermsOfServiceScreenHandler logic where Google Play Store
    // enabled preferencee is set to true on showing UI, which eventually
    // triggers to call RequestEnable().
    arc_session_manager()->RequestEnable();
  }

  void Hide() override {}

  void Bind(chromeos::ArcTermsOfServiceScreen* screen) override {}

  base::ObserverList<chromeos::ArcTermsOfServiceScreenViewObserver>::Unchecked
      observer_list_;
  std::unique_ptr<chromeos::FakeLoginDisplayHost> fake_login_display_host_;

  DISALLOW_COPY_AND_ASSIGN(ArcSessionOobeOptInNegotiatorTest);
};

INSTANTIATE_TEST_SUITE_P(All,
                         ArcSessionOobeOptInNegotiatorTest,
                         ::testing::Values(true, false));

TEST_P(ArcSessionOobeOptInNegotiatorTest, OobeTermsAccepted) {
  view()->Show();
  EXPECT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager()->state());
  ReportAccepted();
  EXPECT_EQ(ArcSessionManager::State::CHECKING_ANDROID_MANAGEMENT,
            arc_session_manager()->state());
}

TEST_P(ArcSessionOobeOptInNegotiatorTest, OobeTermsViewDestroyed) {
  view()->Show();
  EXPECT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager()->state());
  CloseLoginDisplayHost();
  ReportViewDestroyed();
  if (!IsManagedUser()) {
    // ArcPlayStoreEnabledPreferenceHandler is not running, so the state should
    // be kept as is.
    EXPECT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
              arc_session_manager()->state());
    EXPECT_FALSE(IsArcPlayStoreEnabledForProfile(profile()));
  } else {
    // For managed case we handle closing outside of
    // ArcPlayStoreEnabledPreferenceHandler. So it session turns to STOPPED.
    EXPECT_EQ(ArcSessionManager::State::STOPPED,
              arc_session_manager()->state());
    // Managed user's preference should not be overwritten.
    EXPECT_TRUE(IsArcPlayStoreEnabledForProfile(profile()));
  }
}

struct ArcSessionRetryTestParam {
  enum class Negotiation {
    // Negotiation is required for provisioning.
    REQUIRED,
    // Negotiation is not required and not shown for provisioning.
    SKIPPED,
  };

  Negotiation negotiation;

  // Provisioning error to test.
  ProvisioningResult error;

  // Whether ARC++ container is alive on error.
  bool container_alive;

  // Whether data is removed on error.
  bool data_removed;
};

constexpr ArcSessionRetryTestParam kRetryTestCases[] = {
    {ArcSessionRetryTestParam::Negotiation::REQUIRED,
     ProvisioningResult::UNKNOWN_ERROR, true, true},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED,
     ProvisioningResult::GMS_NETWORK_ERROR, true, false},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED,
     ProvisioningResult::GMS_SERVICE_UNAVAILABLE, true, false},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED,
     ProvisioningResult::GMS_BAD_AUTHENTICATION, true, false},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED,
     ProvisioningResult::DEVICE_CHECK_IN_FAILED, true, false},
    {ArcSessionRetryTestParam::Negotiation::SKIPPED,
     ProvisioningResult::CLOUD_PROVISION_FLOW_FAILED, true, true},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED,
     ProvisioningResult::MOJO_VERSION_MISMATCH, true, false},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED,
     ProvisioningResult::MOJO_CALL_TIMEOUT, true, false},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED,
     ProvisioningResult::DEVICE_CHECK_IN_TIMEOUT, true, false},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED,
     ProvisioningResult::DEVICE_CHECK_IN_INTERNAL_ERROR, true, false},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED,
     ProvisioningResult::GMS_SIGN_IN_FAILED, true, false},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED,
     ProvisioningResult::GMS_SIGN_IN_TIMEOUT, true, false},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED,
     ProvisioningResult::GMS_SIGN_IN_INTERNAL_ERROR, true, false},
    {ArcSessionRetryTestParam::Negotiation::SKIPPED,
     ProvisioningResult::CLOUD_PROVISION_FLOW_TIMEOUT, true, true},
    {ArcSessionRetryTestParam::Negotiation::SKIPPED,
     ProvisioningResult::CLOUD_PROVISION_FLOW_INTERNAL_ERROR, true, true},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED,
     ProvisioningResult::ARC_STOPPED, false, false},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED,
     ProvisioningResult::OVERALL_SIGN_IN_TIMEOUT, true, true},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED,
     ProvisioningResult::CHROME_SERVER_COMMUNICATION_ERROR, false, false},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED,
     ProvisioningResult::NO_NETWORK_CONNECTION, true, false},
};

class ArcSessionRetryTest
    : public ArcSessionManagerTest,
      public testing::WithParamInterface<ArcSessionRetryTestParam> {
 public:
  ArcSessionRetryTest() = default;

  void SetUp() override {
    ArcSessionManagerTest::SetUp();

    GetFakeUserManager()->set_current_user_new(true);

    // Make negotiation not needed by switching to managed flow with other
    // preferences under the policy, similar to google.com provisioning case.
    if (GetParam().negotiation ==
        ArcSessionRetryTestParam::Negotiation::SKIPPED) {
      policy::ProfilePolicyConnector* const connector =
          profile()->GetProfilePolicyConnector();
      connector->OverrideIsManagedForTesting(true);

      profile()->GetTestingPrefService()->SetManagedPref(
          prefs::kArcEnabled, std::make_unique<base::Value>(true));
      // Set all prefs as managed to simulate google.com account provisioning.
      profile()->GetTestingPrefService()->SetManagedPref(
          prefs::kArcBackupRestoreEnabled,
          std::make_unique<base::Value>(false));
      profile()->GetTestingPrefService()->SetManagedPref(
          prefs::kArcLocationServiceEnabled,
          std::make_unique<base::Value>(false));
      EXPECT_FALSE(arc::IsArcTermsOfServiceNegotiationNeeded(profile()));
    }
  }

  void TearDown() override {
    // Correctly stop service.
    arc_session_manager()->Shutdown();
    ArcSessionManagerTest::TearDown();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcSessionRetryTest);
};

INSTANTIATE_TEST_SUITE_P(All,
                         ArcSessionRetryTest,
                         ::testing::ValuesIn(kRetryTestCases));

// Verifies that Android container behaves as expected.* This checks:
//   * Whether ARC++ container alive or not on error.
//   * Whether Android data is removed or not on error.
//   * ARC++ Container is restared on retry.
TEST_P(ArcSessionRetryTest, ContainerRestarted) {
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();

  if (GetParam().negotiation ==
      ArcSessionRetryTestParam::Negotiation::REQUIRED) {
    EXPECT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
              arc_session_manager()->state());
    arc_session_manager()->OnTermsOfServiceNegotiatedForTesting(true);
  }

  EXPECT_EQ(ArcSessionManager::State::CHECKING_ANDROID_MANAGEMENT,
            arc_session_manager()->state());
  arc_session_manager()->StartArcForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  arc_session_manager()->OnProvisioningFinished(GetParam().error);

  // In case of permanent error data removal request is scheduled.
  EXPECT_EQ(GetParam().data_removed,
            profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));

  if (GetParam().container_alive) {
    // We don't stop ARC due to let user submit user feedback with alive Android
    // container.
    EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  } else {
    // Container is stopped automatically on this error.
    EXPECT_EQ(ArcSessionManager::State::STOPPED,
              arc_session_manager()->state());
  }

  arc_session_manager()->OnRetryClicked();

  if (GetParam().data_removed) {
    // Check state goes from REMOVING_DATA_DIR to CHECKING_ANDROID_MANAGEMENT
    EXPECT_TRUE(WaitForDataRemoved(
        ArcSessionManager::State::CHECKING_ANDROID_MANAGEMENT));
  }

  arc_session_manager()->StartArcForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Successful retry keeps ARC++ container running.
  arc_session_manager()->OnProvisioningFinished(ProvisioningResult::SUCCESS);
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  arc_session_manager()->Shutdown();
}

// Verifies Initialize() generates the serial number for ARC.
TEST_F(ArcSessionManagerTest, SerialNumber) {
  arc_session_manager()->SetProfile(profile());
  EXPECT_TRUE(
      profile()->GetPrefs()->GetString(prefs::kArcSerialNumber).empty());
  arc_session_manager()->Initialize();
  // Check that the serial number Initialize() has generated is not empty.
  const std::string serial_number =
      profile()->GetPrefs()->GetString(prefs::kArcSerialNumber);
  EXPECT_FALSE(serial_number.empty());
  // ..and it's a hex number.
  std::vector<uint8_t> dummy;
  EXPECT_TRUE(base::HexStringToBytes(serial_number, &dummy));
}

// Verifies Initialize() generates the serial number for ARC.
TEST_F(ArcSessionManagerTest, SerialNumber_Existing) {
  constexpr char kDummySerialNumber[] = "A1C55A0D52D2F8E874D5";
  profile()->GetPrefs()->SetString(prefs::kArcSerialNumber, kDummySerialNumber);
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  // Check that Initialize() does not clear the existing serial number.
  EXPECT_EQ(kDummySerialNumber,
            profile()->GetPrefs()->GetString(prefs::kArcSerialNumber));
}

// Test that when files have already been expaneded, AddObserver() immediately
// calls OnPropertyFilesExpanded().
TEST_F(ArcSessionManagerTest, FileExpansion_AlreadyDone) {
  arc_session_manager()->reset_property_files_expansion_result();
  FileExpansionObserver observer;
  arc_session_manager()->OnExpandPropertyFilesForTesting(true);
  arc_session_manager()->AddObserver(&observer);
  ASSERT_TRUE(observer.property_files_expansion_result().has_value());
  EXPECT_TRUE(observer.property_files_expansion_result().value());
}

// Tests that OnPropertyFilesExpanded() is called with true when the files are
// expended.
TEST_F(ArcSessionManagerTest, FileExpansion) {
  arc_session_manager()->reset_property_files_expansion_result();
  FileExpansionObserver observer;
  arc_session_manager()->AddObserver(&observer);
  EXPECT_FALSE(observer.property_files_expansion_result().has_value());
  arc_session_manager()->OnExpandPropertyFilesForTesting(true);
  ASSERT_TRUE(observer.property_files_expansion_result().has_value());
  EXPECT_TRUE(observer.property_files_expansion_result().value());
}

// Tests that OnPropertyFilesExpanded() is called with false when the expansion
// failed.
TEST_F(ArcSessionManagerTest, FileExpansion_Fail) {
  arc_session_manager()->reset_property_files_expansion_result();
  FileExpansionObserver observer;
  arc_session_manager()->AddObserver(&observer);
  EXPECT_FALSE(observer.property_files_expansion_result().has_value());
  arc_session_manager()->OnExpandPropertyFilesForTesting(false);
  ASSERT_TRUE(observer.property_files_expansion_result().has_value());
  EXPECT_FALSE(observer.property_files_expansion_result().value());
}

class ArcSessionManagerPowerwashTest : public ArcSessionManagerTestBase {
 public:
  ArcSessionManagerPowerwashTest() = default;
  ~ArcSessionManagerPowerwashTest() override = default;
  ArcSessionManagerPowerwashTest(const ArcSessionManagerPowerwashTest&) =
      delete;
  ArcSessionManagerPowerwashTest& operator=(
      const ArcSessionManagerPowerwashTest&) = delete;

  void SetUp() override {
    ArcSessionManagerTestBase::SetUp();
    chromeos::CryptohomeClient::InitializeFake();
  }

  void TearDown() override {
    chromeos::CryptohomeClient::Shutdown();
    ArcSessionManagerTestBase::TearDown();
  }
};

TEST_F(ArcSessionManagerPowerwashTest, PowerwashRequestBlocksArcStart) {
  EXPECT_EQ(ArcSessionManager::State::NOT_INITIALIZED,
            arc_session_manager()->state());

  // Set up the situation that provisioning is successfully done in the
  // previous session.
  PrefService* const prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kArcTermsAccepted, true);
  prefs->SetBoolean(prefs::kArcSignedIn, true);

  // Login unaffiliated user.
  const AccountId account_id(AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), "1234567890"));
  GetFakeUserManager()->AddUserWithAffiliation(account_id, false);
  GetFakeUserManager()->LoginUser(account_id);

  // Set DeviceRebootOnUserSignout to ALWAYS.
  chromeos::ScopedCrosSettingsTestHelper settings_helper{
      /* create_settings_service=*/false};
  settings_helper.ReplaceDeviceSettingsProviderWithStub();
  settings_helper.SetInteger(
      chromeos::kDeviceRebootOnUserSignout,
      enterprise_management::DeviceRebootOnUserSignoutProto::ALWAYS);

  // Initialize cryptohome to require powerwash.
  chromeos::FakeCryptohomeClient::Get()->set_requires_powerwash(true);
  policy::PowerwashRequirementsChecker::InitializeSynchronouslyForTesting();

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  arc_session_manager()->RequestEnable();
  // Wait for manager's state.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());

  arc_session_manager()->Shutdown();
}

}  // namespace
}  // namespace arc
