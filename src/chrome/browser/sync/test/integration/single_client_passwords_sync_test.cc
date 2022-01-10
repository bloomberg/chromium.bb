// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/secondary_account_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/password_manager/core/browser/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/browser/sync/password_sync_bridge.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/driver/sync_service_impl.h"
#include "components/sync/test/fake_server/fake_server_nigori_helper.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"

namespace {

using password_manager::features_util::OptInToAccountStorage;
using passwords_helper::CreateTestPasswordForm;
using passwords_helper::GetPasswordCount;
using passwords_helper::GetProfilePasswordStoreInterface;
using passwords_helper::GetVerifierPasswordCount;
using passwords_helper::GetVerifierProfilePasswordStoreInterface;
using passwords_helper::ProfileContainsSamePasswordFormsAsVerifier;

using password_manager::PasswordForm;

using testing::ElementsAre;
using testing::IsEmpty;

const syncer::SyncFirstSetupCompleteSource kSetSourceFromTest =
    syncer::SyncFirstSetupCompleteSource::BASIC_FLOW;

class SingleClientPasswordsSyncTest : public SyncTest {
 public:
  SingleClientPasswordsSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientPasswordsSyncTest() override = default;
};

class SingleClientPasswordsSyncTestWithVerifier
    : public SingleClientPasswordsSyncTest {
 public:
  SingleClientPasswordsSyncTestWithVerifier() = default;
  ~SingleClientPasswordsSyncTestWithVerifier() override = default;

  bool UseVerifier() override {
    // TODO(crbug.com/1137740): rewrite tests to not use verifier.
    return true;
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientPasswordsSyncTestWithVerifier, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  PasswordForm form = CreateTestPasswordForm(0);
  GetVerifierProfilePasswordStoreInterface()->AddLogin(form);
  ASSERT_EQ(1, GetVerifierPasswordCount());
  GetProfilePasswordStoreInterface(0)->AddLogin(form);
  ASSERT_EQ(1, GetPasswordCount(0));

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(ProfileContainsSamePasswordFormsAsVerifier(0));
  ASSERT_EQ(1, GetPasswordCount(0));
}

// Verifies that committed passwords contain the appropriate proto fields, and
// in particular lack some others that could potentially contain unencrypted
// data. In this test, custom passphrase is NOT set.
IN_PROC_BROWSER_TEST_F(SingleClientPasswordsSyncTestWithVerifier,
                       CommitWithoutCustomPassphrase) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  PasswordForm form = CreateTestPasswordForm(0);
  GetVerifierProfilePasswordStoreInterface()->AddLogin(form);
  ASSERT_EQ(1, GetVerifierPasswordCount());
  GetProfilePasswordStoreInterface(0)->AddLogin(form);
  ASSERT_EQ(1, GetPasswordCount(0));
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  const std::vector<sync_pb::SyncEntity> entities =
      fake_server_->GetSyncEntitiesByModelType(syncer::PASSWORDS);
  ASSERT_EQ(1U, entities.size());
  EXPECT_EQ("", entities[0].non_unique_name());
  EXPECT_TRUE(entities[0].specifics().password().has_encrypted());
  EXPECT_FALSE(
      entities[0].specifics().password().has_client_only_encrypted_data());
  EXPECT_TRUE(entities[0].specifics().password().has_unencrypted_metadata());
  EXPECT_TRUE(
      entities[0].specifics().password().unencrypted_metadata().has_url());
}

// Same as above but with custom passphrase set, which requires to prune commit
// data even further.
IN_PROC_BROWSER_TEST_F(SingleClientPasswordsSyncTestWithVerifier,
                       CommitWithCustomPassphrase) {
  SetEncryptionPassphraseForClient(/*index=*/0, "hunter2");
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  PasswordForm form = CreateTestPasswordForm(0);
  GetVerifierProfilePasswordStoreInterface()->AddLogin(form);
  ASSERT_EQ(1, GetVerifierPasswordCount());
  GetProfilePasswordStoreInterface(0)->AddLogin(form);
  ASSERT_EQ(1, GetPasswordCount(0));
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  const std::vector<sync_pb::SyncEntity> entities =
      fake_server_->GetSyncEntitiesByModelType(syncer::PASSWORDS);
  ASSERT_EQ(1U, entities.size());
  EXPECT_EQ("", entities[0].non_unique_name());
  EXPECT_TRUE(entities[0].specifics().password().has_encrypted());
  EXPECT_FALSE(
      entities[0].specifics().password().has_client_only_encrypted_data());
  EXPECT_FALSE(entities[0].specifics().password().has_unencrypted_metadata());
}

// Tests the scenario when a syncing user enables a custom passphrase. PASSWORDS
// should be recommitted with the new encryption key.
IN_PROC_BROWSER_TEST_F(SingleClientPasswordsSyncTestWithVerifier,
                       ReencryptsDataWhenPassphraseIsSet) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(ServerNigoriChecker(GetSyncService(0), fake_server_.get(),
                                  syncer::PassphraseType::kKeystorePassphrase)
                  .Wait());

  PasswordForm form = CreateTestPasswordForm(0);
  GetVerifierProfilePasswordStoreInterface()->AddLogin(form);
  ASSERT_EQ(1, GetVerifierPasswordCount());
  GetProfilePasswordStoreInterface(0)->AddLogin(form);
  ASSERT_EQ(1, GetPasswordCount(0));
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  std::string prior_encryption_key_name;
  {
    const std::vector<sync_pb::SyncEntity> entities =
        fake_server_->GetSyncEntitiesByModelType(syncer::PASSWORDS);
    ASSERT_EQ(1U, entities.size());
    ASSERT_EQ("", entities[0].non_unique_name());
    ASSERT_TRUE(entities[0].specifics().password().has_encrypted());
    ASSERT_FALSE(
        entities[0].specifics().password().has_client_only_encrypted_data());
    ASSERT_TRUE(entities[0].specifics().password().has_unencrypted_metadata());
    prior_encryption_key_name =
        entities[0].specifics().password().encrypted().key_name();
  }

  ASSERT_FALSE(prior_encryption_key_name.empty());

  GetSyncService(0)->GetUserSettings()->SetEncryptionPassphrase("hunter2");
  ASSERT_TRUE(ServerNigoriChecker(GetSyncService(0), fake_server_.get(),
                                  syncer::PassphraseType::kCustomPassphrase)
                  .Wait());
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  const std::vector<sync_pb::SyncEntity> entities =
      fake_server_->GetSyncEntitiesByModelType(syncer::PASSWORDS);
  ASSERT_EQ(1U, entities.size());
  EXPECT_EQ("", entities[0].non_unique_name());
  EXPECT_TRUE(entities[0].specifics().password().has_encrypted());
  EXPECT_FALSE(
      entities[0].specifics().password().has_client_only_encrypted_data());
  EXPECT_FALSE(entities[0].specifics().password().has_unencrypted_metadata());

  const std::string new_encryption_key_name =
      entities[0].specifics().password().encrypted().key_name();
  EXPECT_FALSE(new_encryption_key_name.empty());
  EXPECT_NE(new_encryption_key_name, prior_encryption_key_name);
}

IN_PROC_BROWSER_TEST_F(SingleClientPasswordsSyncTest,
                       PRE_PersistProgressMarkerOnRestart) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  PasswordForm form = CreateTestPasswordForm(0);
  GetProfilePasswordStoreInterface(0)->AddLogin(form);
  ASSERT_EQ(1, GetPasswordCount(0));
  // Setup sync, wait for its completion, and make sure changes were synced.
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  // Upon a local creation, the received update will be seen as reflection and
  // get counted as incremental update.
  EXPECT_EQ(
      1, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.PASSWORD",
                                         /*REMOTE_NON_INITIAL_UPDATE=*/4));
}

IN_PROC_BROWSER_TEST_F(SingleClientPasswordsSyncTest,
                       PersistProgressMarkerOnRestart) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  ASSERT_EQ(1, GetPasswordCount(0));
  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization());

  // After restart, the last sync cycle snapshot should be empty. Once a sync
  // request happened (e.g. by a poll), that snapshot is populated. We use the
  // following checker to simply wait for an non-empty snapshot.
  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  // If that metadata hasn't been properly persisted, the password stored on the
  // server will be received at the client as an initial update or an
  // incremental once.
  EXPECT_EQ(
      0, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.PASSWORD",
                                         /*REMOTE_INITIAL_UPDATE=*/5));
  EXPECT_EQ(
      0, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.PASSWORD",
                                         /*REMOTE_NON_INITIAL_UPDATE=*/4));
}

class SingleClientPasswordsWithAccountStorageSyncTest : public SyncTest {
 public:
  SingleClientPasswordsWithAccountStorageSyncTest() : SyncTest(SINGLE_CLIENT) {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{password_manager::features::
                                  kEnablePasswordsAccountStorage},
        /*disabled_features=*/{});
  }

  SingleClientPasswordsWithAccountStorageSyncTest(
      const SingleClientPasswordsWithAccountStorageSyncTest&) = delete;
  SingleClientPasswordsWithAccountStorageSyncTest& operator=(
      const SingleClientPasswordsWithAccountStorageSyncTest&) = delete;

  ~SingleClientPasswordsWithAccountStorageSyncTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    test_signin_client_subscription_ =
        secondary_account_helper::SetUpSigninClient(&test_url_loader_factory_);
  }

  void SetUpOnMainThread() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    secondary_account_helper::InitNetwork();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    SyncTest::SetUpOnMainThread();

    fake_server::SetKeystoreNigoriInFakeServer(GetFakeServer());
  }

  void AddTestPasswordToFakeServer() {
    sync_pb::PasswordSpecificsData password_data;
    // Used for computing the client tag.
    password_data.set_origin("https://origin.com");
    password_data.set_username_element("username_element");
    password_data.set_username_value("username_value");
    password_data.set_password_element("password_element");
    password_data.set_signon_realm("abc");
    // Other data.
    password_data.set_password_value("password_value");

    passwords_helper::InjectKeystoreEncryptedServerPassword(password_data,
                                                            GetFakeServer());
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  base::CallbackListSubscription test_signin_client_subscription_;
};

// Sanity check: For Sync-the-feature, password data still ends up in the
// profile database.
IN_PROC_BROWSER_TEST_F(SingleClientPasswordsWithAccountStorageSyncTest,
                       StoresDataForSyncingPrimaryAccountInProfileDB) {
  AddTestPasswordToFakeServer();

  // Sign in and enable Sync.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));

  // Make sure the password showed up in the account store and not in the
  // profile store.
  password_manager::PasswordStoreInterface* profile_store =
      passwords_helper::GetProfilePasswordStoreInterface(0);
  EXPECT_EQ(passwords_helper::GetAllLogins(profile_store).size(), 1u);

  password_manager::PasswordStoreInterface* account_store =
      passwords_helper::GetAccountPasswordStoreInterface(0);
  EXPECT_EQ(passwords_helper::GetAllLogins(account_store).size(), 0u);
}

IN_PROC_BROWSER_TEST_F(SingleClientPasswordsWithAccountStorageSyncTest,
                       StoresDataForNonSyncingPrimaryAccountInAccountDB) {
  AddTestPasswordToFakeServer();

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On ChromeOS, Sync-the-feature gets started automatically once a primary
  // account is signed in. To prevent that, explicitly set SyncRequested to
  // false.
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Setup a primary account, but don't actually enable Sync-the-feature (so
  // that Sync will start in transport mode).
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Let the user opt in to the account-scoped password storage, and wait for it
  // to become active.
  OptInToAccountStorage(GetProfile(0)->GetPrefs(), GetSyncService(0));
  PasswordSyncActiveChecker(GetSyncService(0)).Wait();
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));

  // Make sure the password showed up in the account store and not in the
  // profile store.
  password_manager::PasswordStoreInterface* profile_store =
      passwords_helper::GetProfilePasswordStoreInterface(0);
  EXPECT_EQ(passwords_helper::GetAllLogins(profile_store).size(), 0u);

  password_manager::PasswordStoreInterface* account_store =
      passwords_helper::GetAccountPasswordStoreInterface(0);
  EXPECT_EQ(passwords_helper::GetAllLogins(account_store).size(), 1u);
}

// The unconsented primary account isn't supported on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SingleClientPasswordsWithAccountStorageSyncTest,
                       StoresDataForSecondaryAccountInAccountDB) {
  AddTestPasswordToFakeServer();

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Setup Sync without consent (i.e. in transport mode).
  secondary_account_helper::SignInUnconsentedAccount(
      GetProfile(0), &test_url_loader_factory_, "user@email.com");
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Let the user opt in to the account-scoped password storage, and wait for it
  // to become active.
  OptInToAccountStorage(GetProfile(0)->GetPrefs(), GetSyncService(0));
  PasswordSyncActiveChecker(GetSyncService(0)).Wait();
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));

  // Make sure the password showed up in the account store and not in the
  // profile store.
  password_manager::PasswordStoreInterface* profile_store =
      passwords_helper::GetProfilePasswordStoreInterface(0);
  EXPECT_EQ(passwords_helper::GetAllLogins(profile_store).size(), 0u);

  password_manager::PasswordStoreInterface* account_store =
      passwords_helper::GetAccountPasswordStoreInterface(0);
  EXPECT_EQ(passwords_helper::GetAllLogins(account_store).size(), 1u);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// ChromeOS does not support signing out of a primary account.
#if !BUILDFLAG(IS_CHROMEOS_ASH)

// Sanity check: The profile database should *not* get cleared on signout.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
// On Lacros, signout is not supported with Mirror account consistency.
// TODO(https://crbug.com/1260291): Enable this test once signout is supported.
#define MAYBE_DoesNotClearProfileDBOnSignout \
  DISABLED_DoesNotClearProfileDBOnSignout
#else
#define MAYBE_DoesNotClearProfileDBOnSignout DoesNotClearProfileDBOnSignout
#endif
IN_PROC_BROWSER_TEST_F(SingleClientPasswordsWithAccountStorageSyncTest,
                       MAYBE_DoesNotClearProfileDBOnSignout) {
  AddTestPasswordToFakeServer();

  // Sign in and enable Sync.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Make sure the password showed up in the profile store.
  password_manager::PasswordStoreInterface* profile_store =
      passwords_helper::GetProfilePasswordStoreInterface(0);
  ASSERT_EQ(passwords_helper::GetAllLogins(profile_store).size(), 1u);

  // Sign out again.
  GetClient(0)->SignOutPrimaryAccount();
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Make sure the password is still in the store.
  ASSERT_EQ(passwords_helper::GetAllLogins(profile_store).size(), 1u);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// The unconsented primary account isn't supported on ChromeOS so Sync won't
// start up for an unconsented account.
// Signing out on Lacros is not possible.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SingleClientPasswordsWithAccountStorageSyncTest,
                       ClearsAccountDBOnSignout) {
  AddTestPasswordToFakeServer();

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Setup Sync without consent (i.e. in transport mode).
  AccountInfo account_info = secondary_account_helper::SignInUnconsentedAccount(
      GetProfile(0), &test_url_loader_factory_, "user@email.com");
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Let the user opt in to the account-scoped password storage, and wait for it
  // to become active.
  OptInToAccountStorage(GetProfile(0)->GetPrefs(), GetSyncService(0));
  PasswordSyncActiveChecker(GetSyncService(0)).Wait();

  // Make sure the password showed up in the account store.
  password_manager::PasswordStoreInterface* account_store =
      passwords_helper::GetAccountPasswordStoreInterface(0);
  ASSERT_EQ(passwords_helper::GetAllLogins(account_store).size(), 1u);

  // Sign out again.
  secondary_account_helper::SignOutAccount(
      GetProfile(0), &test_url_loader_factory_, account_info.account_id);

  // Make sure the password is gone from the store.
  ASSERT_EQ(passwords_helper::GetAllLogins(account_store).size(), 0u);
}
#endif  // !defined(OS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(SingleClientPasswordsWithAccountStorageSyncTest,
                       SwitchesStoresOnEnablingSync) {
  AddTestPasswordToFakeServer();

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // On ChromeOS, Sync-the-feature starts automatically as soon as a primary
  // account is signed in. To prevent that, explicitly set SyncRequested to
  // false on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(false);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  // Sign in to a primary account, but don't enable Sync-the-feature.
  // Note: This state shouldn't actually be reachable (the flow for setting a
  // primary account also enables Sync), but still best to cover it here.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Let the user opt in to the account-scoped password storage, and wait for it
  // to become active.
  OptInToAccountStorage(GetProfile(0)->GetPrefs(), GetSyncService(0));
  PasswordSyncActiveChecker(GetSyncService(0)).Wait();

  // Make sure the password showed up in the account store.
  password_manager::PasswordStoreInterface* account_store =
      passwords_helper::GetAccountPasswordStoreInterface(0);
  ASSERT_EQ(passwords_helper::GetAllLogins(account_store).size(), 1u);

  // Turn on Sync-the-feature.
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(true);
  GetSyncService(0)->GetUserSettings()->SetFirstSetupComplete(
      kSetSourceFromTest);
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Make sure the password is now in the profile store, but *not* in the
  // account store anymore.
  password_manager::PasswordStoreInterface* profile_store =
      passwords_helper::GetProfilePasswordStoreInterface(0);
  EXPECT_EQ(passwords_helper::GetAllLogins(profile_store).size(), 1u);
  EXPECT_EQ(passwords_helper::GetAllLogins(account_store).size(), 0u);

  // Turn off Sync-the-feature again.
  // Note: Turning Sync off without signing out isn't actually exposed to the
  // user, so this generally shouldn't happen. Still best to cover it here.
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(false);
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Now the password should be in both stores: The profile store does *not* get
  // cleared when Sync gets disabled.
  EXPECT_EQ(passwords_helper::GetAllLogins(profile_store).size(), 1u);
  EXPECT_EQ(passwords_helper::GetAllLogins(account_store).size(), 1u);
}

// The unconsented primary account isn't supported on ChromeOS so Sync won't
// start up for an unconsented account.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SingleClientPasswordsWithAccountStorageSyncTest,
                       SwitchesStoresOnMakingAccountPrimary) {
  AddTestPasswordToFakeServer();

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Setup Sync for an unconsented account (i.e. in transport mode).
  AccountInfo account_info = secondary_account_helper::SignInUnconsentedAccount(
      GetProfile(0), &test_url_loader_factory_, "user@email.com");
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Let the user opt in to the account-scoped password storage, and wait for it
  // to become active.
  OptInToAccountStorage(GetProfile(0)->GetPrefs(), GetSyncService(0));
  PasswordSyncActiveChecker(GetSyncService(0)).Wait();

  // Make sure the password showed up in the account store.
  password_manager::PasswordStoreInterface* account_store =
      passwords_helper::GetAccountPasswordStoreInterface(0);
  ASSERT_EQ(passwords_helper::GetAllLogins(account_store).size(), 1u);

  // Turn on Sync-the-feature.
  secondary_account_helper::GrantSyncConsent(GetProfile(0), "user@email.com");
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(true);
  GetSyncService(0)->GetUserSettings()->SetFirstSetupComplete(
      kSetSourceFromTest);
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Make sure the password is now in the profile store, but *not* in the
  // account store anymore.
  password_manager::PasswordStoreInterface* profile_store =
      passwords_helper::GetProfilePasswordStoreInterface(0);
  EXPECT_EQ(passwords_helper::GetAllLogins(profile_store).size(), 1u);
  EXPECT_EQ(passwords_helper::GetAllLogins(account_store).size(), 0u);

// On Lacros, signout is not supported with Mirror account consistency.
// TODO(https://crbug.com/1260291): Enable this part of the test once signout is
// supported.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  // Clear the primary account to put Sync into transport mode again.
  // Note: Clearing the primary account without also signing out isn't exposed
  // to the user, so this shouldn't happen. Still best to cover it here.
  signin::RevokeSyncConsent(
      IdentityManagerFactory::GetForProfile(GetProfile(0)));
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // The account-storage opt-in gets cleared when turning off Sync, so opt in
  // again.
  OptInToAccountStorage(GetProfile(0)->GetPrefs(), GetSyncService(0));
  PasswordSyncActiveChecker(GetSyncService(0)).Wait();

  // Now the password should be in both stores: The profile store does *not* get
  // cleared when Sync gets disabled.
  EXPECT_EQ(passwords_helper::GetAllLogins(profile_store).size(), 1u);
  EXPECT_EQ(passwords_helper::GetAllLogins(account_store).size(), 1u);
#endif
}

// Regression test for crbug.com/1076378.
IN_PROC_BROWSER_TEST_F(SingleClientPasswordsWithAccountStorageSyncTest,
                       EnablesPasswordSyncOnOptingInToSync) {
  AddTestPasswordToFakeServer();

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Setup Sync for an unconsented account (i.e. in transport mode).
  AccountInfo account_info = secondary_account_helper::SignInUnconsentedAccount(
      GetProfile(0), &test_url_loader_factory_, "user@email.com");
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // The user is not opted in to the account-scoped password storage, so the
  // passwords data type should *not* be active.
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));

  // Turn on Sync-the-feature.
  secondary_account_helper::GrantSyncConsent(GetProfile(0), "user@email.com");
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(true);
  GetSyncService(0)->GetUserSettings()->SetFirstSetupComplete(
      kSetSourceFromTest);
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Now password sync should be active.
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace
