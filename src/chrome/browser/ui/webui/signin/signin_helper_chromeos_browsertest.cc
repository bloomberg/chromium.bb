// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_helper_chromeos.h"

#include "ash/components/account_manager/account_manager_factory.h"
#include "ash/constants/ash_features.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/account_manager/account_apps_availability.h"
#include "chrome/browser/ash/account_manager/account_apps_availability_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/chromeos/account_manager_mojo_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {

class SigninHelperChromeOSTest;

namespace {

const char kFakeGaiaId[] = "fake_gaia_id";
const char kFakeEmail[] = "fake_email@gmail.com";
const char kFakeAuthCode[] = "fake_auth_code";
const char kFakeDeviceId[] = "fake_device_id";
const char kFakeRefreshToken[] = "fake_refresh_token";

class TestSigninHelper : public SigninHelper {
 public:
  TestSigninHelper(
      SigninHelperChromeOSTest* test_fixture,
      account_manager::AccountManager* account_manager,
      crosapi::AccountManagerMojoService* account_manager_mojo_service,
      const base::RepeatingClosure& close_dialog_closure,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<ArcHelper> arc_helper,
      const std::string& gaia_id,
      const std::string& email,
      const std::string& auth_code,
      const std::string& signin_scoped_device_id)
      : SigninHelper(account_manager,
                     account_manager_mojo_service,
                     close_dialog_closure,
                     url_loader_factory,
                     std::move(arc_helper),
                     gaia_id,
                     email,
                     auth_code,
                     signin_scoped_device_id) {
    test_fixture_ = test_fixture;
  }

  ~TestSigninHelper() override;

  void OnClientOAuthSuccess(const ClientOAuthResult& result) override {
    SigninHelper::OnClientOAuthSuccess(result);
  }

  void OnClientOAuthFailure(const GoogleServiceAuthError& error) override {
    SigninHelper::OnClientOAuthFailure(error);
  }

 private:
  SigninHelperChromeOSTest* test_fixture_;
};

}  // namespace

class SigninHelperChromeOSTest
    : public InProcessBrowserTest,
      public account_manager::AccountManager::Observer {
 public:
  SigninHelperChromeOSTest() = default;

  ~SigninHelperChromeOSTest() override {
    DCHECK_EQ(signin_helper_created_count_, signin_helper_deleted_count_);
  }

  void SetUpOnMainThread() override {
    auto* profile = browser()->profile();
    auto* factory =
        g_browser_process->platform_part()->GetAccountManagerFactory();
    account_manager_ = factory->GetAccountManager(profile->GetPath().value());
    account_manager_mojo_service_ =
        factory->GetAccountManagerMojoService(profile->GetPath().value());
    account_manager_->SetUrlLoaderFactoryForTests(shared_url_loader_factory());
    account_manager_->AddObserver(this);

    // Setup the main account:
    account_manager::AccountKey kPrimaryAccountKey{
        "primary_account_gaia", account_manager::AccountType::kGaia};
    account_manager()->UpsertAccount(kPrimaryAccountKey, "primary@gmai.com",
                                     "access_token");
    base::RunLoop().RunUntilIdle();
    on_token_upserted_call_count_ = 0;
    on_token_upserted_account_ = absl::nullopt;
  }

  void TearDownOnMainThread() override {
    account_manager_->RemoveObserver(this);
    on_token_upserted_call_count_ = 0;
    on_token_upserted_account_ = absl::nullopt;
  }

  TestSigninHelper* CreateSigninHelper(
      const base::RepeatingClosure& close_dialog_closure) {
    OnSigninHelperCreated();
    return new TestSigninHelper(
        this, account_manager(), account_manager_mojo_service(),
        close_dialog_closure, shared_url_loader_factory(),
        /*arc_helper=*/nullptr, kFakeGaiaId, kFakeEmail, kFakeAuthCode,
        kFakeDeviceId);
  }

  GaiaAuthConsumer::ClientOAuthResult GetFakeOAuthResult() {
    return GaiaAuthConsumer::ClientOAuthResult(kFakeRefreshToken, "", 0, false,
                                               false);
  }

  void OnSigninHelperCreated() { ++signin_helper_created_count_; }
  void OnSigninHelperDeleted() { ++signin_helper_deleted_count_; }

  int on_token_upserted_call_count() { return on_token_upserted_call_count_; }

  absl::optional<account_manager::Account> on_token_upserted_account() {
    return on_token_upserted_account_;
  }

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory() {
    return browser()
        ->profile()
        ->GetDefaultStoragePartition()
        ->GetURLLoaderFactoryForBrowserProcess();
  }

  account_manager::AccountManager* account_manager() {
    return account_manager_;
  }

  crosapi::AccountManagerMojoService* account_manager_mojo_service() {
    return account_manager_mojo_service_;
  }

 private:
  // account_manager::AccountManager::Observer overrides:
  void OnTokenUpserted(const account_manager::Account& account) override {
    ++on_token_upserted_call_count_;
    on_token_upserted_account_ = account;
  }

  void OnAccountRemoved(const account_manager::Account& account) override {}

  account_manager::AccountManager* account_manager_ = nullptr;
  crosapi::AccountManagerMojoService* account_manager_mojo_service_ = nullptr;
  int signin_helper_created_count_ = 0;
  int signin_helper_deleted_count_ = 0;
  int on_token_upserted_call_count_ = 0;
  absl::optional<account_manager::Account> on_token_upserted_account_;
};

TestSigninHelper::~TestSigninHelper() {
  test_fixture_->OnSigninHelperDeleted();
}

IN_PROC_BROWSER_TEST_F(SigninHelperChromeOSTest,
                       NoAccountAddedWhenAuthTokenFetchFails) {
  base::RunLoop close_dialog_closure_run_loop;
  auto* helper = CreateSigninHelper(
      base::BindLambdaForTesting([&close_dialog_closure_run_loop]() {
        close_dialog_closure_run_loop.Quit();
      }));
  // Auth token fetch fails.
  helper->OnClientOAuthFailure(
      GoogleServiceAuthError(GoogleServiceAuthError::State::CONNECTION_FAILED));
  // Make sure the close_dialog_closure was called.
  close_dialog_closure_run_loop.Run();
  // Wait until SigninHelper finishes and deletes itself.
  base::RunLoop().RunUntilIdle();
  // No account should be added.
  EXPECT_EQ(on_token_upserted_call_count(), 0);
}

IN_PROC_BROWSER_TEST_F(SigninHelperChromeOSTest,
                       AccountAddedWhenAuthTokenFetchSucceeds) {
  base::RunLoop close_dialog_closure_run_loop;
  auto* helper =
      CreateSigninHelper(close_dialog_closure_run_loop.QuitClosure());
  // Auth token fetch succeeds.
  helper->OnClientOAuthSuccess(GetFakeOAuthResult());
  // Make sure the close_dialog_closure was called.
  close_dialog_closure_run_loop.Run();
  // Wait until SigninHelper finishes and deletes itself.
  base::RunLoop().RunUntilIdle();
  // 1 account should be added.
  EXPECT_EQ(on_token_upserted_call_count(), 1);
  auto account = on_token_upserted_account();
  ASSERT_TRUE(account.has_value());
  EXPECT_EQ(account.value().raw_email, kFakeEmail);
}

class SigninHelperChromeOSTestWithArcAccountRestrictions
    : public SigninHelperChromeOSTest,
      public ::ash::AccountAppsAvailability::Observer {
 public:
  SigninHelperChromeOSTestWithArcAccountRestrictions() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kArcAccountRestrictions,
                              chromeos::features::kLacrosSupport},
        /*disabled_features=*/{});
  }

  ~SigninHelperChromeOSTestWithArcAccountRestrictions() override = default;

  void SetUpOnMainThread() override {
    SigninHelperChromeOSTest::SetUpOnMainThread();
    account_apps_availability_ =
        ash::AccountAppsAvailabilityFactory::GetForProfile(
            browser()->profile());
    // In-session account addition happens when `AccountAppsAvailability` is
    // already initialized.
    EXPECT_TRUE(account_apps_availability()->IsInitialized());
    account_apps_availability()->AddObserver(this);
  }

  void TearDownOnMainThread() override {
    account_apps_availability()->RemoveObserver(this);
    on_account_available_in_arc_call_count_ = 0;
    on_account_unavailable_in_arc_call_count_ = 0;
    on_account_available_in_arc_account_ = absl::nullopt;
    on_account_unavailable_in_arc_account_ = absl::nullopt;
    SigninHelperChromeOSTest::TearDownOnMainThread();
  }

  TestSigninHelper* CreateSigninHelper(
      std::unique_ptr<SigninHelper::ArcHelper> arc_helper,
      const base::RepeatingClosure& close_dialog_closure) {
    OnSigninHelperCreated();
    return new TestSigninHelper(
        this, account_manager(), account_manager_mojo_service(),
        close_dialog_closure, shared_url_loader_factory(),
        std::move(arc_helper), kFakeGaiaId, kFakeEmail, kFakeAuthCode,
        kFakeDeviceId);
  }

  bool IsAccountAvailableInArc(account_manager::Account account) {
    bool result = false;
    base::RunLoop run_loop;
    account_apps_availability()->GetAccountsAvailableInArc(
        base::BindLambdaForTesting(
            [&result, &account, &run_loop](
                const base::flat_set<account_manager::Account>& accounts) {
              for (const auto& a : accounts) {
                if (a.raw_email == account.raw_email)
                  result = true;
              }
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  ash::AccountAppsAvailability* account_apps_availability() {
    return account_apps_availability_;
  }

  int on_account_available_in_arc_call_count() {
    return on_account_available_in_arc_call_count_;
  }

  int on_account_unavailable_in_arc_call_count() {
    return on_account_unavailable_in_arc_call_count_;
  }

  absl::optional<account_manager::Account>
  on_account_available_in_arc_account() {
    return on_account_available_in_arc_account_;
  }

  absl::optional<account_manager::Account>
  on_account_unavailable_in_arc_account() {
    return on_account_unavailable_in_arc_account_;
  }

 private:
  void OnAccountAvailableInArc(
      const account_manager::Account& account) override {
    ++on_account_available_in_arc_call_count_;
    on_account_available_in_arc_account_ = account;
  }

  void OnAccountUnavailableInArc(
      const account_manager::Account& account) override {
    ++on_account_unavailable_in_arc_call_count_;
    on_account_unavailable_in_arc_account_ = account;
  }

  int on_account_available_in_arc_call_count_ = 0;
  int on_account_unavailable_in_arc_call_count_ = 0;
  absl::optional<account_manager::Account> on_account_available_in_arc_account_;
  absl::optional<account_manager::Account>
      on_account_unavailable_in_arc_account_;
  ash::AccountAppsAvailability* account_apps_availability_;
  base::test::ScopedFeatureList feature_list_;
};

// Account is available in ARC after account addition if `is_available_in_arc`
// is set to `true`.
IN_PROC_BROWSER_TEST_F(SigninHelperChromeOSTestWithArcAccountRestrictions,
                       AccountIsAvailableInArcAfterAddition) {
  std::unique_ptr<SigninHelper::ArcHelper> arc_helper =
      std::make_unique<SigninHelper::ArcHelper>(
          /*is_available_in_arc=*/true, /*is_account_addition=*/true,
          account_apps_availability());
  base::RunLoop close_dialog_closure_run_loop;
  auto* helper = CreateSigninHelper(
      std::move(arc_helper), close_dialog_closure_run_loop.QuitClosure());
  // Auth token fetch succeeds.
  helper->OnClientOAuthSuccess(GetFakeOAuthResult());
  // Make sure the close_dialog_closure was called.
  close_dialog_closure_run_loop.Run();
  // Wait until SigninHelper finishes and deletes itself.
  base::RunLoop().RunUntilIdle();
  // 1 account should be added.
  EXPECT_EQ(on_token_upserted_call_count(), 1);
  auto account = on_token_upserted_account();
  ASSERT_TRUE(account.has_value());
  EXPECT_EQ(account.value().raw_email, kFakeEmail);
  // 1 account should be available in ARC.
  EXPECT_EQ(on_account_unavailable_in_arc_call_count(), 0);
  // Note: after we receive one `OnAccountAvailableInArc` call - we may get
  // another call after the refresh token is updated for account.
  EXPECT_GT(on_account_available_in_arc_call_count(), 0);
  auto arc_account = on_account_available_in_arc_account();
  ASSERT_TRUE(arc_account.has_value());
  EXPECT_EQ(arc_account.value().raw_email, kFakeEmail);
  // `AccountAppsAvailability::GetAccountsAvailableInArc` should return account
  // list containing this account.
  EXPECT_TRUE(IsAccountAvailableInArc(account.value()));
}

// Account is not available in ARC after account addition if
// `is_available_in_arc` is set to `false`.
IN_PROC_BROWSER_TEST_F(SigninHelperChromeOSTestWithArcAccountRestrictions,
                       AccountIsNotAvailableInArcAfterAddition) {
  std::unique_ptr<SigninHelper::ArcHelper> arc_helper =
      std::make_unique<SigninHelper::ArcHelper>(
          /*is_available_in_arc=*/false, /*is_account_addition=*/true,
          account_apps_availability());
  base::RunLoop close_dialog_closure_run_loop;
  auto* helper = CreateSigninHelper(
      std::move(arc_helper),
      base::BindLambdaForTesting([&close_dialog_closure_run_loop]() {
        close_dialog_closure_run_loop.Quit();
      }));
  // Auth token fetch succeeds.
  helper->OnClientOAuthSuccess(GetFakeOAuthResult());
  // Make sure the close_dialog_closure was called.
  close_dialog_closure_run_loop.Run();
  // Wait until SigninHelper finishes and deletes itself.
  base::RunLoop().RunUntilIdle();
  // 1 account should be added.
  EXPECT_EQ(on_token_upserted_call_count(), 1);
  auto account = on_token_upserted_account();
  ASSERT_TRUE(account.has_value());
  EXPECT_EQ(account.value().raw_email, kFakeEmail);

  // The account didn't exist (and therefore wasn't available in ARC) before, so
  // no `OnAccountUnavailableInArc` calls are expected.
  EXPECT_EQ(on_account_available_in_arc_call_count(), 0);
  EXPECT_EQ(on_account_unavailable_in_arc_call_count(), 0);
  // `AccountAppsAvailability::GetAccountsAvailableInArc` should return account
  // list not containing this account.
  EXPECT_FALSE(IsAccountAvailableInArc(account.value()));
}

IN_PROC_BROWSER_TEST_F(SigninHelperChromeOSTestWithArcAccountRestrictions,
                       ArcAvailabilityIsNotSetAfterReauthentication) {
  account_manager::AccountKey kAccountKey{kFakeGaiaId,
                                          account_manager::AccountType::kGaia};
  account_manager()->UpsertAccount(kAccountKey, kFakeEmail, "access_token");
  base::RunLoop().RunUntilIdle();
  // 1 account should be added.
  const int initial_upserted_calls = 1;
  EXPECT_EQ(on_token_upserted_call_count(), initial_upserted_calls);

  // Go through a reauthentication flow.
  std::unique_ptr<SigninHelper::ArcHelper> arc_helper =
      std::make_unique<SigninHelper::ArcHelper>(
          /*is_available_in_arc=*/true, /*is_account_addition=*/false,
          account_apps_availability());
  base::RunLoop close_dialog_closure_run_loop;
  auto* helper = CreateSigninHelper(
      std::move(arc_helper),
      base::BindLambdaForTesting([&close_dialog_closure_run_loop]() {
        close_dialog_closure_run_loop.Quit();
      }));
  // Auth token fetch succeeds.
  helper->OnClientOAuthSuccess(GetFakeOAuthResult());
  // Make sure the close_dialog_closure was called.
  close_dialog_closure_run_loop.Run();
  // Wait until SigninHelper finishes and deletes itself.
  base::RunLoop().RunUntilIdle();
  // 1 account should be updated.
  EXPECT_EQ(on_token_upserted_call_count(), initial_upserted_calls + 1);
  auto account = on_token_upserted_account();
  ASSERT_TRUE(account.has_value());
  EXPECT_EQ(account.value().raw_email, kFakeEmail);
  // 0 accounts should be added as "available in ARC".
  EXPECT_EQ(on_account_available_in_arc_call_count(), 0);
  EXPECT_EQ(on_account_unavailable_in_arc_call_count(), 0);
}

IN_PROC_BROWSER_TEST_F(SigninHelperChromeOSTestWithArcAccountRestrictions,
                       AccountAvailabilityDoesntChangeAfterReauthentication) {}

}  // namespace chromeos
