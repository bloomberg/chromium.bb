// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/auth/arc_auth_context.h"
#include "chrome/browser/chromeos/arc/auth/arc_auth_service.h"
#include "chrome/browser/chromeos/arc/auth/arc_background_auth_code_fetcher.h"
#include "chrome/browser/chromeos/arc/session/arc_service_launcher.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/test/test_arc_session_manager.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/ui/app_list/arc/arc_data_removal_dialog.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "components/account_id/account_id.h"
#include "components/arc/arc_features.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/session/arc_data_remover.h"
#include "components/arc/session/arc_session_runner.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_arc_session.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_test.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kFakeUserName[] = "test@example.com";
constexpr char kSecondaryAccountEmail[] = "email.111@gmail.com";
constexpr char kFakeAuthCode[] = "fake-auth-code";

std::string GetFakeAuthTokenResponse() {
  return base::StringPrintf(R"({ "token" : "%s"})", kFakeAuthCode);
}

std::unique_ptr<KeyedService> CreateCertificateProviderService(
    content::BrowserContext* context) {
  return std::make_unique<chromeos::CertificateProviderService>();
}

}  // namespace

namespace arc {

class FakeAuthInstance : public mojom::AuthInstance {
 public:
  FakeAuthInstance() = default;
  ~FakeAuthInstance() override = default;

  // mojom::AuthInstance:
  void InitDeprecated(mojom::AuthHostPtr host) override {
    Init(std::move(host), base::DoNothing());
  }

  void Init(mojom::AuthHostPtr host, InitCallback callback) override {
    host_ = std::move(host);
    std::move(callback).Run();
  }

  void OnAccountInfoReadyDeprecated(mojom::AccountInfoPtr account_info,
                                    mojom::ArcSignInStatus status) override {
    account_info_ = std::move(account_info);
    std::move(done_closure_).Run();
  }

  void OnAccountUpdated(const std::string& account_name,
                        mojom::AccountUpdateType update_type) override {
    switch (update_type) {
      case mojom::AccountUpdateType::UPSERT:
        ++num_account_upserted_calls_;
        last_upserted_account_ = account_name;
        break;
      case mojom::AccountUpdateType::REMOVAL:
        ++num_account_removed_calls_;
        last_removed_account_ = account_name;
        break;
    }
  }

  void RequestAccountInfoDeprecated(base::OnceClosure done_closure) {
    done_closure_ = std::move(done_closure);
    host_->RequestAccountInfoDeprecated(true /* initial_signin */);
  }

  void RequestPrimaryAccountInfo(base::OnceClosure done_closure) {
    host_->RequestPrimaryAccountInfo(base::BindOnce(
        &FakeAuthInstance::OnPrimaryAccountInfoResponse,
        weak_ptr_factory_.GetWeakPtr(), std::move(done_closure)));
  }

  void RequestAccountInfo(const std::string& account_name,
                          base::OnceClosure done_closure) {
    host_->RequestAccountInfo(
        account_name, base::BindOnce(&FakeAuthInstance::OnAccountInfoResponse,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     std::move(done_closure)));
  }

  void GetGoogleAccounts(GetGoogleAccountsCallback callback) override {
    std::vector<mojom::ArcAccountInfoPtr> accounts;
    accounts.emplace_back(mojom::ArcAccountInfo::New(
        kFakeUserName, signin::GetTestGaiaIdForEmail(kFakeUserName)));
    std::move(callback).Run(std::move(accounts));
  }

  void GetMainAccountResolutionStatus(
      GetMainAccountResolutionStatusCallback callback) override {
    std::move(callback).Run(
        mojom::MainAccountResolutionStatus::HASH_CODE_MATCH_SINGLE_ACCOUNT);
  }

  mojom::AccountInfo* account_info() { return account_info_.get(); }

  mojom::ArcSignInStatus sign_in_status() const { return status_; }

  bool sign_in_persistent_error() const { return persistent_error_; }

  int num_account_upserted_calls() const { return num_account_upserted_calls_; }

  std::string last_upserted_account() const { return last_upserted_account_; }

  int num_account_removed_calls() const { return num_account_removed_calls_; }

  std::string last_removed_account() const { return last_removed_account_; }

 private:
  void OnPrimaryAccountInfoResponse(base::OnceClosure done_closure,
                                    mojom::ArcSignInStatus status,
                                    mojom::AccountInfoPtr account_info) {
    account_info_ = std::move(account_info);
    status_ = status;
    std::move(done_closure).Run();
  }

  void OnAccountInfoResponse(base::OnceClosure done_closure,
                             mojom::ArcSignInStatus status,
                             mojom::AccountInfoPtr account_info,
                             bool persistent_error) {
    status_ = status;
    account_info_ = std::move(account_info);
    persistent_error_ = persistent_error;
    std::move(done_closure).Run();
  }

  mojom::AuthHostPtr host_;
  mojom::ArcSignInStatus status_;
  bool persistent_error_;
  mojom::AccountInfoPtr account_info_;
  base::OnceClosure done_closure_;

  int num_account_upserted_calls_ = 0;
  std::string last_upserted_account_;
  int num_account_removed_calls_ = 0;
  std::string last_removed_account_;

  base::WeakPtrFactory<FakeAuthInstance> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(FakeAuthInstance);
};

class ArcAuthServiceTest : public InProcessBrowserTest {
 protected:
  ArcAuthServiceTest() = default;

  // InProcessBrowserTest:
  ~ArcAuthServiceTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<chromeos::FakeChromeUserManager>());
    // Init ArcSessionManager for testing.
    ArcServiceLauncher::Get()->ResetForTesting();
    ArcSessionManager::SetUiEnabledForTesting(false);
    ArcSessionManager::EnableCheckAndroidManagementForTesting(true);
    ArcSessionManager::Get()->SetArcSessionRunnerForTesting(
        std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));
    EXPECT_TRUE(ExpandPropertyFilesForTesting(ArcSessionManager::Get(),
                                              temp_dir_.GetPath()));

    chromeos::ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(true);
  }

  void TearDownOnMainThread() override {
    if (arc_bridge_service_)
      arc_bridge_service_->auth()->CloseInstance(&auth_instance_);

    // Explicitly removing the user is required; otherwise ProfileHelper keeps
    // a dangling pointer to the User.
    // TODO(nya): Consider removing all users from ProfileHelper in the
    // destructor of FakeChromeUserManager.
    GetFakeUserManager()->RemoveUserFromList(
        GetFakeUserManager()->GetActiveUser()->GetAccountId());
    // Since ArcServiceLauncher is (re-)set up with profile() in
    // SetUpOnMainThread() it is necessary to Shutdown() before the profile()
    // is destroyed. ArcServiceLauncher::Shutdown() will be called again on
    // fixture destruction (because it is initialized with the original Profile
    // instance in fixture, once), but it should be no op.
    // TODO(hidehiko): Think about a way to test the code cleanly.
    ArcServiceLauncher::Get()->Shutdown();
    identity_test_environment_adaptor_.reset();
    profile_.reset();
    user_manager_enabler_.reset();
    chromeos::ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(false);
  }

  chromeos::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<chromeos::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  void EnableRemovalOfExtendedAccountInfo() {
    identity_test_environment_adaptor_->identity_test_env()
        ->EnableRemovalOfExtendedAccountInfo();
  }

  void SetAccountAndProfile(const user_manager::UserType user_type) {
    AccountId account_id;
    if (user_type == user_manager::USER_TYPE_ACTIVE_DIRECTORY) {
      account_id = AccountId::AdFromUserEmailObjGuid(
          kFakeUserName, "fake-active-directory-guid");
    } else {
      account_id = AccountId::FromUserEmailGaiaId(
          kFakeUserName, signin::GetTestGaiaIdForEmail(kFakeUserName));
    }
    const user_manager::User* user = nullptr;
    switch (user_type) {
      case user_manager::USER_TYPE_CHILD:
        user = GetFakeUserManager()->AddChildUser(account_id);
        break;
      case user_manager::USER_TYPE_REGULAR:
        user = GetFakeUserManager()->AddUser(account_id);
        break;
      case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
        user = GetFakeUserManager()->AddPublicAccountUser(account_id);
        break;
      case user_manager::USER_TYPE_ACTIVE_DIRECTORY:
        user = GetFakeUserManager()->AddUserWithAffiliationAndTypeAndProfile(
            account_id, true /*is_affiliated*/,
            user_manager::USER_TYPE_ACTIVE_DIRECTORY, nullptr /*profile*/);
        break;
      case user_manager::USER_TYPE_ARC_KIOSK_APP:
        user = GetFakeUserManager()->AddUserWithAffiliationAndTypeAndProfile(
            account_id, false /*is_affiliated*/,
            user_manager::USER_TYPE_ARC_KIOSK_APP, nullptr /*profile*/);
        break;
      default:
        ADD_FAILURE() << "Unexpected user type " << user_type;
        return;
    }

    GetFakeUserManager()->LoginUser(account_id);
    GetFakeUserManager()->CreateLocalState();

    // Create test profile.
    TestingProfile::Builder profile_builder;
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestArcProfile"));
    profile_builder.SetProfileName(kFakeUserName);
    if (user_type == user_manager::USER_TYPE_CHILD)
      profile_builder.SetSupervisedUserId(supervised_users::kChildAccountSUID);

    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(profile_builder);
    identity_test_environment_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());

    chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        user, profile_.get());

    auto* identity_test_env =
        identity_test_environment_adaptor_->identity_test_env();
    identity_test_env->SetAutomaticIssueOfAccessTokens(true);
    if (user_type != user_manager::USER_TYPE_ACTIVE_DIRECTORY) {
      // IdentityManager doesn't have a primary account for Active Directory
      // sessions. Use "unconsented" because ARC doesn't care about browser
      // sync consent.
      identity_test_env->MakeUnconsentedPrimaryAccountAvailable(kFakeUserName);
    }

    profile()->GetPrefs()->SetBoolean(prefs::kArcSignedIn, true);
    profile()->GetPrefs()->SetBoolean(prefs::kArcTermsAccepted, true);
    MigrateSigninScopedDeviceId(profile());

    // TestingProfile is not interpreted as a primary profile. Inject factory so
    // that the instance of CertificateProviderService for the profile can be
    // created.
    chromeos::CertificateProviderServiceFactory::GetInstance()
        ->SetTestingFactory(
            profile(), base::BindRepeating(&CreateCertificateProviderService));

    ArcServiceLauncher::Get()->OnPrimaryUserProfilePrepared(profile());

    auth_service_ = ArcAuthService::GetForBrowserContext(profile());
    DCHECK(auth_service_);

    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    auth_service_->SetURLLoaderFactoryForTesting(test_shared_loader_factory_);
    arc_bridge_service_ = ArcServiceManager::Get()->arc_bridge_service();
    DCHECK(arc_bridge_service_);
    arc_bridge_service_->auth()->SetInstance(&auth_instance_);
    WaitForInstanceReady(arc_bridge_service_->auth());
    // Waiting for users and profiles to be setup.
    base::RunLoop().RunUntilIdle();
  }

  AccountInfo SeedAccountInfo(const std::string& email) {
    return identity_test_environment_adaptor_->identity_test_env()
        ->MakeAccountAvailable(email);
  }

  void SetInvalidRefreshTokenForAccount(const CoreAccountId& account_id) {
    identity_test_environment_adaptor_->identity_test_env()
        ->SetInvalidRefreshTokenForAccount(account_id);
  }

  void SetRefreshTokenForAccount(const CoreAccountId& account_id) {
    identity_test_environment_adaptor_->identity_test_env()
        ->SetRefreshTokenForAccount(account_id);
  }

  void UpdatePersistentErrorOfRefreshTokenForAccount(
      const CoreAccountId& account_id,
      const GoogleServiceAuthError& error) {
    identity_test_environment_adaptor_->identity_test_env()
        ->UpdatePersistentErrorOfRefreshTokenForAccount(account_id, error);
  }

  void RequestGoogleAccountsInArc() {
    arc_google_accounts_.clear();
    arc_google_accounts_callback_called_ = false;
    run_loop_.reset(new base::RunLoop());

    ArcAuthService::GetGoogleAccountsInArcCallback callback = base::BindOnce(
        [](std::vector<mojom::ArcAccountInfoPtr>* accounts,
           bool* arc_google_accounts_callback_called,
           base::OnceClosure quit_closure,
           std::vector<mojom::ArcAccountInfoPtr> returned_accounts) {
          *accounts = std::move(returned_accounts);
          *arc_google_accounts_callback_called = true;
          std::move(quit_closure).Run();
        },
        &arc_google_accounts_, &arc_google_accounts_callback_called_,
        run_loop_->QuitClosure());

    auth_service().GetGoogleAccountsInArc(std::move(callback));
  }

  AccountInfo SetupGaiaAccount(const std::string& email) {
    SetAccountAndProfile(user_manager::USER_TYPE_REGULAR);
    return SeedAccountInfo(email);
  }

  void WaitForGoogleAccountsInArcCallback() { run_loop_->RunUntilIdle(); }

  std::pair<std::string, mojom::ChromeAccountType>
  RequestPrimaryAccount() {
    base::RunLoop run_loop;
    std::string account_name;
    mojom::ChromeAccountType account_type = mojom::ChromeAccountType::UNKNOWN;
    base::OnceCallback<void(const std::string&, mojom::ChromeAccountType)>
        callback = base::BindLambdaForTesting(
            [&run_loop, &account_name, &account_type](
                const std::string& returned_account_name,
                mojom::ChromeAccountType returned_account_type) {
              account_name = returned_account_name;
              account_type = returned_account_type;
              run_loop.Quit();
            });

    auth_service().RequestPrimaryAccount(std::move(callback));
    run_loop.Run();

    return std::make_pair(account_name, account_type);
  }

  Profile* profile() { return profile_.get(); }

  void set_profile_name(const std::string& username) {
    profile_->set_profile_name(username);
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }
  ArcAuthService& auth_service() { return *auth_service_; }
  FakeAuthInstance& auth_instance() { return auth_instance_; }
  ArcBridgeService& arc_bridge_service() { return *arc_bridge_service_; }
  const std::vector<mojom::ArcAccountInfoPtr>& arc_google_accounts() const {
    return arc_google_accounts_;
  }
  bool arc_google_accounts_callback_called() const {
    return arc_google_accounts_callback_called_;
  }

 private:
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingProfile> profile_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  FakeAuthInstance auth_instance_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_adaptor_;

  std::vector<mojom::ArcAccountInfoPtr> arc_google_accounts_;
  bool arc_google_accounts_callback_called_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;

  // Not owned.
  ArcAuthService* auth_service_ = nullptr;
  ArcBridgeService* arc_bridge_service_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ArcAuthServiceTest);
};

IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest, GetPrimaryAccountForGaiaAccounts) {
  SetAccountAndProfile(user_manager::USER_TYPE_REGULAR);
  const std::pair<std::string, mojom::ChromeAccountType>
      primary_account = RequestPrimaryAccount();
  EXPECT_EQ(kFakeUserName, primary_account.first);
  EXPECT_EQ(mojom::ChromeAccountType::USER_ACCOUNT, primary_account.second);
}

IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest, GetPrimaryAccountForChildAccounts) {
  SetAccountAndProfile(user_manager::USER_TYPE_CHILD);
  const std::pair<std::string, mojom::ChromeAccountType>
      primary_account = RequestPrimaryAccount();
  EXPECT_EQ(kFakeUserName, primary_account.first);
  EXPECT_EQ(mojom::ChromeAccountType::CHILD_ACCOUNT, primary_account.second);
}

IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest,
                       GetPrimaryAccountForActiveDirectoryAccounts) {
  SetAccountAndProfile(user_manager::USER_TYPE_ACTIVE_DIRECTORY);
  const std::pair<std::string, mojom::ChromeAccountType>
      primary_account = RequestPrimaryAccount();
  EXPECT_EQ(std::string(), primary_account.first);
  EXPECT_EQ(mojom::ChromeAccountType::ACTIVE_DIRECTORY_ACCOUNT,
            primary_account.second);
}

IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest, GetPrimaryAccountForPublicAccounts) {
  SetAccountAndProfile(user_manager::USER_TYPE_PUBLIC_ACCOUNT);
  const std::pair<std::string, mojom::ChromeAccountType>
      primary_account = RequestPrimaryAccount();
  EXPECT_EQ(std::string(), primary_account.first);
  EXPECT_EQ(mojom::ChromeAccountType::ROBOT_ACCOUNT, primary_account.second);
}

IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest,
                       GetPrimaryAccountForOfflineDemoAccounts) {
  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kOffline);
  chromeos::DemoSession::StartIfInDemoMode();
  SetAccountAndProfile(user_manager::USER_TYPE_PUBLIC_ACCOUNT);
  const std::pair<std::string, mojom::ChromeAccountType>
      primary_account = RequestPrimaryAccount();
  EXPECT_EQ(std::string(), primary_account.first);
  EXPECT_EQ(mojom::ChromeAccountType::OFFLINE_DEMO_ACCOUNT,
            primary_account.second);
}

// Tests that when ARC requests account info for a non-managed account, via
// |RequestAccountInfoDeprecated| API, Chrome supplies the info configured in
// SetAccountAndProfile() method.
IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest,
                       SuccessfulBackgroundFetchViaDeprecatedApi) {
  SetAccountAndProfile(user_manager::USER_TYPE_REGULAR);
  test_url_loader_factory()->AddResponse(arc::kAuthTokenExchangeEndPoint,
                                         GetFakeAuthTokenResponse());

  base::RunLoop run_loop;
  auth_instance().RequestAccountInfoDeprecated(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_EQ(kFakeUserName,
            auth_instance().account_info()->account_name.value());
  EXPECT_EQ(kFakeAuthCode, auth_instance().account_info()->auth_code.value());
  EXPECT_EQ(mojom::ChromeAccountType::USER_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_FALSE(auth_instance().account_info()->enrollment_token);
  EXPECT_FALSE(auth_instance().account_info()->is_managed);
}

// Tests that when ARC requests account info for a non-managed account,
// Chrome supplies the info configured in SetAccountAndProfile() method.
IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest, SuccessfulBackgroundFetch) {
  SetAccountAndProfile(user_manager::USER_TYPE_REGULAR);
  test_url_loader_factory()->AddResponse(arc::kAuthTokenExchangeEndPoint,
                                         GetFakeAuthTokenResponse());

  base::RunLoop run_loop;
  auth_instance().RequestPrimaryAccountInfo(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_EQ(kFakeUserName,
            auth_instance().account_info()->account_name.value());
  EXPECT_EQ(kFakeAuthCode, auth_instance().account_info()->auth_code.value());
  EXPECT_EQ(mojom::ChromeAccountType::USER_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_FALSE(auth_instance().account_info()->enrollment_token);
  EXPECT_FALSE(auth_instance().account_info()->is_managed);
}

IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest,
                       ReAuthenticatePrimaryAccountSucceeds) {
  SetAccountAndProfile(user_manager::USER_TYPE_REGULAR);
  test_url_loader_factory()->AddResponse(arc::kAuthTokenExchangeEndPoint,
                                         GetFakeAuthTokenResponse());

  base::RunLoop run_loop;
  auth_instance().RequestAccountInfo(kFakeUserName, run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_EQ(kFakeUserName,
            auth_instance().account_info()->account_name.value());
  EXPECT_EQ(kFakeAuthCode, auth_instance().account_info()->auth_code.value());
  EXPECT_EQ(mojom::ChromeAccountType::USER_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_FALSE(auth_instance().account_info()->enrollment_token);
  EXPECT_FALSE(auth_instance().account_info()->is_managed);
  EXPECT_FALSE(auth_instance().sign_in_persistent_error());
}

IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest,
                       RetryAuthTokenExchangeRequestOnUnauthorizedError) {
  SetAccountAndProfile(user_manager::USER_TYPE_REGULAR);

  base::RunLoop run_loop;
  auth_instance().RequestAccountInfo(kFakeUserName, run_loop.QuitClosure());

  EXPECT_TRUE(
      test_url_loader_factory()->IsPending(arc::kAuthTokenExchangeEndPoint));
  test_url_loader_factory()->SimulateResponseForPendingRequest(
      arc::kAuthTokenExchangeEndPoint, std::string(), net::HTTP_UNAUTHORIZED);

  // Should retry auth token exchange request
  EXPECT_TRUE(
      test_url_loader_factory()->IsPending(arc::kAuthTokenExchangeEndPoint));
  test_url_loader_factory()->SimulateResponseForPendingRequest(
      arc::kAuthTokenExchangeEndPoint, GetFakeAuthTokenResponse());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
}

IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest,
                       ReAuthenticatePrimaryAccountFailsForInvalidAccount) {
  SetAccountAndProfile(user_manager::USER_TYPE_REGULAR);
  test_url_loader_factory()->AddResponse(arc::kAuthTokenExchangeEndPoint,
                                         std::string() /* response */,
                                         net::HTTP_UNAUTHORIZED);

  base::RunLoop run_loop;
  auth_instance().RequestAccountInfo(kFakeUserName, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(auth_instance().account_info());
  EXPECT_EQ(mojom::ArcSignInStatus::CHROME_SERVER_COMMUNICATION_ERROR,
            auth_instance().sign_in_status());
}

IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest, FetchSecondaryAccountInfoSucceeds) {
  // Add a Secondary Account.
  SetAccountAndProfile(user_manager::USER_TYPE_REGULAR);
  SeedAccountInfo(kSecondaryAccountEmail);
  test_url_loader_factory()->AddResponse(arc::kAuthTokenExchangeEndPoint,
                                         GetFakeAuthTokenResponse());

  base::RunLoop run_loop;
  auth_instance().RequestAccountInfo(kSecondaryAccountEmail,
                                     run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_EQ(kSecondaryAccountEmail,
            auth_instance().account_info()->account_name.value());
  EXPECT_EQ(kFakeAuthCode, auth_instance().account_info()->auth_code.value());
  EXPECT_EQ(mojom::ChromeAccountType::USER_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_FALSE(auth_instance().account_info()->enrollment_token);
  EXPECT_FALSE(auth_instance().account_info()->is_managed);
  EXPECT_FALSE(auth_instance().sign_in_persistent_error());
}

IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest,
                       FetchSecondaryAccountInfoFailsForInvalidAccounts) {
  // Add a Secondary Account.
  SetAccountAndProfile(user_manager::USER_TYPE_REGULAR);
  SeedAccountInfo(kSecondaryAccountEmail);
  test_url_loader_factory()->AddResponse(arc::kAuthTokenExchangeEndPoint,
                                         std::string() /* response */,
                                         net::HTTP_UNAUTHORIZED);

  base::RunLoop run_loop;
  auth_instance().RequestAccountInfo(kSecondaryAccountEmail,
                                     run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(auth_instance().account_info());
  EXPECT_EQ(mojom::ArcSignInStatus::CHROME_SERVER_COMMUNICATION_ERROR,
            auth_instance().sign_in_status());
}

IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest,
                       FetchSecondaryAccountInfoInvalidRefreshToken) {
  const AccountInfo account_info = SetupGaiaAccount(kSecondaryAccountEmail);
  SetInvalidRefreshTokenForAccount(account_info.account_id);
  test_url_loader_factory()->AddResponse(arc::kAuthTokenExchangeEndPoint,
                                         std::string() /* response */,
                                         net::HTTP_UNAUTHORIZED);

  base::RunLoop run_loop;
  auth_instance().RequestAccountInfo(kSecondaryAccountEmail,
                                     run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(auth_instance().account_info());
  EXPECT_EQ(mojom::ArcSignInStatus::CHROME_SERVER_COMMUNICATION_ERROR,
            auth_instance().sign_in_status());
  EXPECT_TRUE(auth_instance().sign_in_persistent_error());
}

IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest,
                       FetchSecondaryAccountRefreshTokenHasPersistentError) {
  const AccountInfo account_info = SetupGaiaAccount(kSecondaryAccountEmail);
  UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.account_id,
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  base::RunLoop run_loop;
  auth_instance().RequestAccountInfo(kSecondaryAccountEmail,
                                     run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(auth_instance().account_info());
  EXPECT_EQ(mojom::ArcSignInStatus::CHROME_SERVER_COMMUNICATION_ERROR,
            auth_instance().sign_in_status());
  EXPECT_TRUE(auth_instance().sign_in_persistent_error());
}

IN_PROC_BROWSER_TEST_F(
    ArcAuthServiceTest,
    FetchSecondaryAccountInfoReturnsErrorForNotFoundAccounts) {
  SetAccountAndProfile(user_manager::USER_TYPE_REGULAR);
  // Don't add account with kSecondaryAccountEmail.

  base::RunLoop run_loop;
  auth_instance().RequestAccountInfo(kSecondaryAccountEmail,
                                     run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(auth_instance().account_info());
  EXPECT_EQ(mojom::ArcSignInStatus::CHROME_ACCOUNT_NOT_FOUND,
            auth_instance().sign_in_status());
  EXPECT_TRUE(auth_instance().sign_in_persistent_error());
}

IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest, FetchGoogleAccountsFromArc) {
  SetAccountAndProfile(user_manager::USER_TYPE_REGULAR);

  EXPECT_FALSE(arc_google_accounts_callback_called());
  RequestGoogleAccountsInArc();
  WaitForGoogleAccountsInArcCallback();

  EXPECT_TRUE(arc_google_accounts_callback_called());
  ASSERT_EQ(1UL, arc_google_accounts().size());
  EXPECT_EQ(kFakeUserName, arc_google_accounts()[0]->email);
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kFakeUserName),
            arc_google_accounts()[0]->gaia_id);
}

IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest,
                       FetchGoogleAccountsFromArcWorksAcrossConnectionResets) {
  SetAccountAndProfile(user_manager::USER_TYPE_REGULAR);

  // Close the connection.
  arc_bridge_service().auth()->CloseInstance(&auth_instance());
  // Make a request.
  EXPECT_FALSE(arc_google_accounts_callback_called());
  RequestGoogleAccountsInArc();
  WaitForGoogleAccountsInArcCallback();
  // Callback should not be called before connection is restarted.
  EXPECT_FALSE(arc_google_accounts_callback_called());
  // Restart the connection.
  arc_bridge_service().auth()->SetInstance(&auth_instance());
  WaitForInstanceReady(arc_bridge_service().auth());

  EXPECT_TRUE(arc_google_accounts_callback_called());
  ASSERT_EQ(1UL, arc_google_accounts().size());
  EXPECT_EQ(kFakeUserName, arc_google_accounts()[0]->email);
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kFakeUserName),
            arc_google_accounts()[0]->gaia_id);
}

IN_PROC_BROWSER_TEST_F(
    ArcAuthServiceTest,
    PrimaryAccountReauthIsNotAttemptedJustAfterProvisioning) {
  SetAccountAndProfile(user_manager::USER_TYPE_REGULAR);
  const int initial_num_calls = auth_instance().num_account_upserted_calls();
  // Our test setup manually sets the device as provisioned and invokes
  // |ArcAuthService::OnConnectionReady|. Hence, we would have received an
  // update for the Primary Account.
  EXPECT_EQ(1, initial_num_calls);

  // Simulate ARC first time provisioning call.
  auth_service().OnArcInitialStart();
  EXPECT_EQ(initial_num_calls, auth_instance().num_account_upserted_calls());
}

IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest,
                       UnAuthenticatedAccountsAreNotPropagated) {
  const AccountInfo account_info = SetupGaiaAccount(kSecondaryAccountEmail);

  const int initial_num_calls = auth_instance().num_account_upserted_calls();
  // 2 calls: 1 for the Primary Account and 1 for the Secondary Account.
  EXPECT_EQ(2, initial_num_calls);

  SetInvalidRefreshTokenForAccount(account_info.account_id);
  EXPECT_EQ(initial_num_calls, auth_instance().num_account_upserted_calls());
}

IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest, AccountUpdatesArePropagated) {
  AccountInfo account_info = SetupGaiaAccount(kSecondaryAccountEmail);

  SetInvalidRefreshTokenForAccount(account_info.account_id);
  const int initial_num_calls = auth_instance().num_account_upserted_calls();
  // 2 calls: 1 for the Primary Account and 1 for the Secondary Account.
  EXPECT_EQ(2, initial_num_calls);

  SetRefreshTokenForAccount(account_info.account_id);
  // Expect exactly one call for the account update above.
  EXPECT_EQ(1,
            auth_instance().num_account_upserted_calls() - initial_num_calls);
  EXPECT_EQ(kSecondaryAccountEmail, auth_instance().last_upserted_account());
}

IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest, AccountRemovalsArePropagated) {
  SetAccountAndProfile(user_manager::USER_TYPE_REGULAR);
  SeedAccountInfo(kSecondaryAccountEmail);

  EXPECT_EQ(0, auth_instance().num_account_removed_calls());

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  base::Optional<AccountInfo> maybe_account_info =
      identity_manager
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
              kSecondaryAccountEmail);
  ASSERT_TRUE(maybe_account_info.has_value());

  // Necessary to ensure that the OnExtendedAccountInfoRemoved() observer will
  // be sent.
  EnableRemovalOfExtendedAccountInfo();

  identity_manager->GetAccountsMutator()->RemoveAccount(
      maybe_account_info.value().account_id,
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, auth_instance().num_account_removed_calls());
  EXPECT_EQ(kSecondaryAccountEmail, auth_instance().last_removed_account());
}

class ArcRobotAccountAuthServiceTest : public ArcAuthServiceTest {
 public:
  ArcRobotAccountAuthServiceTest() = default;
  ~ArcRobotAccountAuthServiceTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(policy::switches::kDeviceManagementUrl,
                                    "http://localhost");
    ArcAuthServiceTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    ArcAuthServiceTest::SetUpOnMainThread();
    SetUpPolicyClient();
  }

  void TearDownOnMainThread() override {
    ArcAuthServiceTest::TearDownOnMainThread();
  }

 protected:
  void ResponseJob(const network::ResourceRequest& request,
                   network::TestURLLoaderFactory* factory) {
    enterprise_management::DeviceManagementResponse response;
    response.mutable_service_api_access_response()->set_auth_code(
        kFakeAuthCode);

    std::string response_data;
    EXPECT_TRUE(response.SerializeToString(&response_data));

    factory->AddResponse(request.url.spec(), response_data);
  }

 private:
  void SetUpPolicyClient() {
    policy::BrowserPolicyConnectorChromeOS* const connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    policy::DeviceCloudPolicyManagerChromeOS* const cloud_policy_manager =
        connector->GetDeviceCloudPolicyManager();

    cloud_policy_manager->StartConnection(
        std::make_unique<policy::MockCloudPolicyClient>(),
        connector->GetInstallAttributes());

    policy::MockCloudPolicyClient* const cloud_policy_client =
        static_cast<policy::MockCloudPolicyClient*>(
            cloud_policy_manager->core()->client());
    cloud_policy_client->SetDMToken("fake-dm-token");
    cloud_policy_client->client_id_ = "client-id";
  }

  DISALLOW_COPY_AND_ASSIGN(ArcRobotAccountAuthServiceTest);
};

// Tests that when ARC requests account info for a demo session account, via
// |RequestAccountInfoDeprecated| API, Chrome supplies the info configured in
// SetAccountAndProfile() above.
IN_PROC_BROWSER_TEST_F(ArcRobotAccountAuthServiceTest,
                       GetDemoAccountViaDeprecatedApi) {
  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kOnline);
  chromeos::DemoSession::StartIfInDemoMode();

  SetAccountAndProfile(user_manager::USER_TYPE_PUBLIC_ACCOUNT);

  test_url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        ResponseJob(request, test_url_loader_factory());
      }));

  base::RunLoop run_loop;
  auth_instance().RequestAccountInfoDeprecated(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_TRUE(auth_instance().account_info()->account_name.value().empty());
  EXPECT_EQ(kFakeAuthCode, auth_instance().account_info()->auth_code.value());
  EXPECT_EQ(mojom::ChromeAccountType::ROBOT_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_FALSE(auth_instance().account_info()->enrollment_token);
  EXPECT_FALSE(auth_instance().account_info()->is_managed);
}

// Tests that when ARC requests account info for a demo session account,
// Chrome supplies the info configured in SetAccountAndProfile() above.
IN_PROC_BROWSER_TEST_F(ArcRobotAccountAuthServiceTest, GetDemoAccount) {
  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kOnline);
  chromeos::DemoSession::StartIfInDemoMode();

  SetAccountAndProfile(user_manager::USER_TYPE_PUBLIC_ACCOUNT);

  test_url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        ResponseJob(request, test_url_loader_factory());
      }));

  base::RunLoop run_loop;
  auth_instance().RequestPrimaryAccountInfo(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_TRUE(auth_instance().account_info()->account_name.value().empty());
  EXPECT_EQ(kFakeAuthCode, auth_instance().account_info()->auth_code.value());
  EXPECT_EQ(mojom::ChromeAccountType::ROBOT_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_FALSE(auth_instance().account_info()->enrollment_token);
  EXPECT_FALSE(auth_instance().account_info()->is_managed);
}

IN_PROC_BROWSER_TEST_F(ArcRobotAccountAuthServiceTest,
                       GetOfflineDemoAccountViaDeprecatedApi) {
  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kOffline);
  chromeos::DemoSession::StartIfInDemoMode();

  SetAccountAndProfile(user_manager::USER_TYPE_PUBLIC_ACCOUNT);

  base::RunLoop run_loop;
  auth_instance().RequestAccountInfoDeprecated(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_TRUE(auth_instance().account_info()->account_name.value().empty());
  EXPECT_TRUE(auth_instance().account_info()->auth_code.value().empty());
  EXPECT_EQ(mojom::ChromeAccountType::OFFLINE_DEMO_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_FALSE(auth_instance().account_info()->enrollment_token);
  EXPECT_TRUE(auth_instance().account_info()->is_managed);
}

IN_PROC_BROWSER_TEST_F(ArcRobotAccountAuthServiceTest, GetOfflineDemoAccount) {
  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kOffline);
  chromeos::DemoSession::StartIfInDemoMode();

  SetAccountAndProfile(user_manager::USER_TYPE_PUBLIC_ACCOUNT);

  base::RunLoop run_loop;
  auth_instance().RequestPrimaryAccountInfo(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_TRUE(auth_instance().account_info()->account_name.value().empty());
  EXPECT_TRUE(auth_instance().account_info()->auth_code.value().empty());
  EXPECT_EQ(mojom::ChromeAccountType::OFFLINE_DEMO_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_FALSE(auth_instance().account_info()->enrollment_token);
  EXPECT_TRUE(auth_instance().account_info()->is_managed);
}

IN_PROC_BROWSER_TEST_F(ArcRobotAccountAuthServiceTest,
                       GetDemoAccountOnAuthTokenFetchFailureViaDeprecatedApi) {
  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kOnline);
  chromeos::DemoSession::StartIfInDemoMode();

  SetAccountAndProfile(user_manager::USER_TYPE_PUBLIC_ACCOUNT);

  test_url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        test_url_loader_factory()->AddResponse(
            request.url.spec(), std::string(), net::HTTP_NOT_FOUND);
      }));

  base::RunLoop run_loop;
  auth_instance().RequestAccountInfoDeprecated(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_TRUE(auth_instance().account_info()->account_name.value().empty());
  EXPECT_TRUE(auth_instance().account_info()->auth_code.value().empty());
  EXPECT_EQ(mojom::ChromeAccountType::OFFLINE_DEMO_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_FALSE(auth_instance().account_info()->enrollment_token);
  EXPECT_TRUE(auth_instance().account_info()->is_managed);
}

IN_PROC_BROWSER_TEST_F(ArcRobotAccountAuthServiceTest,
                       GetDemoAccountOnAuthTokenFetchFailure) {
  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kOnline);
  chromeos::DemoSession::StartIfInDemoMode();

  SetAccountAndProfile(user_manager::USER_TYPE_PUBLIC_ACCOUNT);

  test_url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        test_url_loader_factory()->AddResponse(
            request.url.spec(), std::string(), net::HTTP_NOT_FOUND);
      }));

  base::RunLoop run_loop;
  auth_instance().RequestPrimaryAccountInfo(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_TRUE(auth_instance().account_info()->account_name.value().empty());
  EXPECT_TRUE(auth_instance().account_info()->auth_code.value().empty());
  EXPECT_EQ(mojom::ChromeAccountType::OFFLINE_DEMO_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_FALSE(auth_instance().account_info()->enrollment_token);
  EXPECT_TRUE(auth_instance().account_info()->is_managed);
}

IN_PROC_BROWSER_TEST_F(ArcRobotAccountAuthServiceTest,
                       RequestPublicAccountInfo) {
  SetAccountAndProfile(user_manager::USER_TYPE_PUBLIC_ACCOUNT);
  profile()->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);

  test_url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        ResponseJob(request, test_url_loader_factory());
      }));

  base::RunLoop run_loop;
  auth_instance().RequestAccountInfo(kFakeUserName, run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_TRUE(auth_instance().account_info()->account_name.value().empty());
  EXPECT_EQ(kFakeAuthCode, auth_instance().account_info()->auth_code.value());
  EXPECT_EQ(mojom::ChromeAccountType::ROBOT_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_FALSE(auth_instance().account_info()->enrollment_token);
  EXPECT_TRUE(auth_instance().account_info()->is_managed);
  EXPECT_FALSE(auth_instance().sign_in_persistent_error());
}

// Tests that when ARC requests account info for a child account, via
// |RequestAccountInfoDeprecated| and Chrome supplies the info configured in
// SetAccountAndProfile() above.
IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest, ChildAccountFetchViaDeprecatedApi) {
  SetAccountAndProfile(user_manager::USER_TYPE_CHILD);
  EXPECT_TRUE(profile()->IsChild());
  test_url_loader_factory()->AddResponse(arc::kAuthTokenExchangeEndPoint,
                                         GetFakeAuthTokenResponse());

  base::RunLoop run_loop;
  auth_instance().RequestAccountInfoDeprecated(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_EQ(kFakeUserName,
            auth_instance().account_info()->account_name.value());
  EXPECT_EQ(kFakeAuthCode, auth_instance().account_info()->auth_code.value());
  EXPECT_EQ(mojom::ChromeAccountType::CHILD_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_FALSE(auth_instance().account_info()->enrollment_token);
  EXPECT_FALSE(auth_instance().account_info()->is_managed);
}

// Tests that when ARC requests account info for a child account and
// Chrome supplies the info configured in SetAccountAndProfile() above.
IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest, ChildAccountFetch) {
  SetAccountAndProfile(user_manager::USER_TYPE_CHILD);
  EXPECT_TRUE(profile()->IsChild());
  test_url_loader_factory()->AddResponse(arc::kAuthTokenExchangeEndPoint,
                                         GetFakeAuthTokenResponse());

  base::RunLoop run_loop;
  auth_instance().RequestPrimaryAccountInfo(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_EQ(kFakeUserName,
            auth_instance().account_info()->account_name.value());
  EXPECT_EQ(kFakeAuthCode, auth_instance().account_info()->auth_code.value());
  EXPECT_EQ(mojom::ChromeAccountType::CHILD_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_FALSE(auth_instance().account_info()->enrollment_token);
  EXPECT_FALSE(auth_instance().account_info()->is_managed);
}

IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest, ChildTransition) {
  SetAccountAndProfile(user_manager::USER_TYPE_CHILD);

  ArcSessionManager* session = ArcSessionManager::Get();
  ASSERT_TRUE(session);

  // Used to track data removal requests.
  ArcDataRemover data_remover(profile()->GetPrefs(),
                              cryptohome::Identification{EmptyAccountId()});

  const std::vector<mojom::SupervisionChangeStatus> success_statuses{
      mojom::SupervisionChangeStatus::CLOUD_DPC_DISABLED,
      mojom::SupervisionChangeStatus::CLOUD_DPC_ALREADY_DISABLED,
      mojom::SupervisionChangeStatus::CLOUD_DPC_ENABLED,
      mojom::SupervisionChangeStatus::CLOUD_DPC_ALREADY_ENABLED};

  const std::vector<mojom::SupervisionChangeStatus> failure_statuses{
      mojom::SupervisionChangeStatus::CLOUD_DPC_DISABLING_FAILED,
      mojom::SupervisionChangeStatus::CLOUD_DPC_ENABLING_FAILED};

  // Suppress ToS.
  profile()->GetPrefs()->SetBoolean(prefs::kArcTermsAccepted, true);
  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);

  // Success statuses do not affect running state of ARC++.
  for (mojom::SupervisionChangeStatus status : success_statuses) {
    EXPECT_EQ(ArcSessionManager::State::ACTIVE, session->state());
    EXPECT_FALSE(IsDataRemovalConfirmationDialogOpenForTesting());
    auth_service().ReportSupervisionChangeStatus(status);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(ArcSessionManager::State::ACTIVE, session->state());
    EXPECT_FALSE(IsDataRemovalConfirmationDialogOpenForTesting());
  }

  // Test failure statuses that lead to showing data removal confirmation and
  // ARC++ stopping. This block tests cancelation of data removal.
  for (mojom::SupervisionChangeStatus status : failure_statuses) {
    EXPECT_EQ(ArcSessionManager::State::ACTIVE, session->state());
    // Confirmation dialog is not shown.
    EXPECT_FALSE(IsDataRemovalConfirmationDialogOpenForTesting());
    // No data removal request.
    EXPECT_FALSE(data_remover.IsScheduledForTesting());
    // Report a failure that brings confirmation dialog.
    auth_service().ReportSupervisionChangeStatus(status);
    base::RunLoop().RunUntilIdle();
    // This does not cause ARC++ stopped.
    EXPECT_EQ(ArcSessionManager::State::ACTIVE, session->state());
    // Dialog should be shown.
    EXPECT_TRUE(IsDataRemovalConfirmationDialogOpenForTesting());
    // No data removal request.
    EXPECT_FALSE(data_remover.IsScheduledForTesting());
    // Cancel data removal confirmation.
    CloseDataRemovalConfirmationDialogForTesting(false);
    // No data removal request.
    EXPECT_FALSE(data_remover.IsScheduledForTesting());
    // Session state does not change.
    EXPECT_EQ(ArcSessionManager::State::ACTIVE, session->state());
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(IsDataRemovalConfirmationDialogOpenForTesting());
  }

  // At this time accepts data removal.
  for (mojom::SupervisionChangeStatus status : failure_statuses) {
    EXPECT_EQ(ArcSessionManager::State::ACTIVE, session->state());
    EXPECT_FALSE(IsDataRemovalConfirmationDialogOpenForTesting());
    EXPECT_FALSE(data_remover.IsScheduledForTesting());
    auth_service().ReportSupervisionChangeStatus(status);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(ArcSessionManager::State::ACTIVE, session->state());
    EXPECT_TRUE(IsDataRemovalConfirmationDialogOpenForTesting());
    EXPECT_FALSE(data_remover.IsScheduledForTesting());

    // Accept data removal confirmation.
    CloseDataRemovalConfirmationDialogForTesting(true);
    // Data removal request is issued.
    EXPECT_TRUE(data_remover.IsScheduledForTesting());
    // Session should switch to data removal.
    EXPECT_EQ(ArcSessionManager::State::REMOVING_DATA_DIR, session->state());
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(IsDataRemovalConfirmationDialogOpenForTesting());
    // After data removal ARC++ is automatically restarted.
    EXPECT_EQ(ArcSessionManager::State::ACTIVE, session->state());
  }

  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ArcSessionManager::State::STOPPED, session->state());

  // Opting out ARC++ forces confirmation dialog to close.
  for (mojom::SupervisionChangeStatus status : failure_statuses) {
    // Suppress ToS.
    profile()->GetPrefs()->SetBoolean(prefs::kArcTermsAccepted, true);
    profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);
    session->StartArcForTesting();
    EXPECT_EQ(ArcSessionManager::State::ACTIVE, session->state());

    auth_service().ReportSupervisionChangeStatus(status);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(IsDataRemovalConfirmationDialogOpenForTesting());

    profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, false);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(ArcSessionManager::State::STOPPED, session->state());
    EXPECT_FALSE(IsDataRemovalConfirmationDialogOpenForTesting());
  }
}

IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest,
                       RegularUserSecondaryAccountsArePropagated) {
  SetAccountAndProfile(user_manager::USER_TYPE_REGULAR);
  SeedAccountInfo(kSecondaryAccountEmail);
  EXPECT_EQ(2, auth_instance().num_account_upserted_calls());
}

IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest,
                       ChildUserSecondaryAccountsNotPropagated) {
  SetAccountAndProfile(user_manager::USER_TYPE_CHILD);
  SeedAccountInfo(kSecondaryAccountEmail);
  EXPECT_TRUE(profile()->IsChild());
  EXPECT_EQ(1, auth_instance().num_account_upserted_calls());
}

}  // namespace arc
