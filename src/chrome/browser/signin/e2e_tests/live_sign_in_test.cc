// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/e2e_tests/live_test.h"
#include "chrome/browser/signin/e2e_tests/test_accounts_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "chrome/browser/ui/webui/signin/signin_email_confirmation_dialog.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/sync/sync_ui_util.h"
#endif  // !defined(OS_CHROMEOS)

namespace signin {
namespace test {

const base::TimeDelta kDialogTimeout = base::TimeDelta::FromSeconds(10);

// A wrapper importing the settings module when the chrome://settings serve the
// Polymer 3 version.
const char kSettingsScriptWrapperFormat[] =
    "import('./settings.js').then(settings => {%s});";

enum class PrimarySyncAccountWait { kWaitForAdded, kWaitForCleared, kNotWait };

// Observes various sign-in events and allows to wait for a specific state of
// signed-in accounts.
class SignInTestObserver : public IdentityManager::Observer,
                           public AccountReconcilor::Observer {
 public:
  explicit SignInTestObserver(IdentityManager* identity_manager,
                              AccountReconcilor* reconcilor)
      : identity_manager_(identity_manager), reconcilor_(reconcilor) {
    identity_manager_->AddObserver(this);
    reconcilor_->AddObserver(this);
  }
  ~SignInTestObserver() override {
    identity_manager_->RemoveObserver(this);
    reconcilor_->RemoveObserver(this);
  }

  // IdentityManager::Observer:
  void OnPrimaryAccountSet(const CoreAccountInfo&) override {
    QuitIfConditionIsSatisfied();
  }
  void OnPrimaryAccountCleared(const CoreAccountInfo&) override {
    QuitIfConditionIsSatisfied();
  }
  void OnRefreshTokenUpdatedForAccount(const CoreAccountInfo&) override {
    QuitIfConditionIsSatisfied();
  }
  void OnRefreshTokenRemovedForAccount(const CoreAccountId&) override {
    QuitIfConditionIsSatisfied();
  }
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo&,
      const GoogleServiceAuthError&) override {
    QuitIfConditionIsSatisfied();
  }
  void OnAccountsInCookieUpdated(const AccountsInCookieJarInfo&,
                                 const GoogleServiceAuthError&) override {
    QuitIfConditionIsSatisfied();
  }

  // AccountReconcilor::Observer:
  // TODO(https://crbug.com/1051864): Remove this obsever method once the bug is
  // fixed.
  void OnStateChanged(signin_metrics::AccountReconcilorState state) override {
    if (state == signin_metrics::ACCOUNT_RECONCILOR_OK) {
      // This will trigger cookie update if accounts are stale.
      identity_manager_->GetAccountsInCookieJar();
    }
  }

  void WaitForAccountChanges(int signed_in_accounts,
                             PrimarySyncAccountWait primary_sync_account_wait) {
    expected_signed_in_accounts_ = signed_in_accounts;
    primary_sync_account_wait_ = primary_sync_account_wait;
    are_expectations_set = true;
    QuitIfConditionIsSatisfied();
    run_loop_.Run();
  }

 private:
  void QuitIfConditionIsSatisfied() {
    if (!are_expectations_set)
      return;

    int accounts_with_valid_refresh_token =
        CountAccountsWithValidRefreshToken();
    int accounts_in_cookie = CountSignedInAccountsInCookie();

    if (accounts_with_valid_refresh_token != accounts_in_cookie ||
        accounts_with_valid_refresh_token != expected_signed_in_accounts_) {
      return;
    }

    bool has_valid_primary_sync_account = HasValidPrimarySyncAccount();
    switch (primary_sync_account_wait_) {
      case PrimarySyncAccountWait::kWaitForAdded:
        if (!has_valid_primary_sync_account)
          return;
        break;
      case PrimarySyncAccountWait::kWaitForCleared:
        if (has_valid_primary_sync_account)
          return;
        break;
      case PrimarySyncAccountWait::kNotWait:
        break;
    }

    run_loop_.Quit();
  }

  int CountAccountsWithValidRefreshToken() const {
    std::vector<CoreAccountInfo> accounts_with_refresh_tokens =
        identity_manager_->GetAccountsWithRefreshTokens();
    int valid_accounts = 0;
    for (const auto& account_info : accounts_with_refresh_tokens) {
      if (!identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
              account_info.account_id)) {
        ++valid_accounts;
      }
    }
    return valid_accounts;
  }

  int CountSignedInAccountsInCookie() const {
    signin::AccountsInCookieJarInfo accounts_in_cookie_jar =
        identity_manager_->GetAccountsInCookieJar();
    if (!accounts_in_cookie_jar.accounts_are_fresh)
      return -1;

    return accounts_in_cookie_jar.signed_in_accounts.size();
  }

  bool HasValidPrimarySyncAccount() const {
    CoreAccountId primary_account_id =
        identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSync);
    if (primary_account_id.empty())
      return false;

    return !identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
        primary_account_id);
  }

  signin::IdentityManager* const identity_manager_;
  AccountReconcilor* const reconcilor_;
  base::RunLoop run_loop_;

  bool are_expectations_set = false;
  int expected_signed_in_accounts_ = 0;
  PrimarySyncAccountWait primary_sync_account_wait_ =
      PrimarySyncAccountWait::kNotWait;
};

// Live tests for SignIn.
class LiveSignInTest : public signin::test::LiveTest {
 public:
  LiveSignInTest() = default;
  ~LiveSignInTest() override = default;

  void SetUp() override {
    LiveTest::SetUp();
    // Always disable animation for stability.
    ui::ScopedAnimationDurationScaleMode disable_animation(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  }

  void SignInFromWeb(const TestAccount& test_account,
                     int previously_signed_in_accounts) {
    AddTabAtIndex(0, GaiaUrls::GetInstance()->add_account_url(),
                  ui::PageTransition::PAGE_TRANSITION_TYPED);
    SignInFromCurrentPage(test_account, previously_signed_in_accounts);
  }

  void SignInFromSettings(const TestAccount& test_account,
                          int previously_signed_in_accounts) {
    GURL settings_url("chrome://settings");
    AddTabAtIndex(0, settings_url, ui::PageTransition::PAGE_TRANSITION_TYPED);
    auto* settings_tab = browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(content::ExecuteScript(
        settings_tab,
        base::StringPrintf(
            kSettingsScriptWrapperFormat,
            "settings.SyncBrowserProxyImpl.getInstance().startSignIn();")));
    SignInFromCurrentPage(test_account, previously_signed_in_accounts);
  }

  void SignInFromCurrentPage(const TestAccount& test_account,
                             int previously_signed_in_accounts) {
    SignInTestObserver observer(identity_manager(), account_reconcilor());
    login_ui_test_utils::ExecuteJsToSigninInSigninFrame(
        browser(), test_account.user, test_account.password);
    observer.WaitForAccountChanges(previously_signed_in_accounts + 1,
                                   PrimarySyncAccountWait::kNotWait);
  }

  void TurnOnSync(const TestAccount& test_account,
                  int previously_signed_in_accounts) {
    SignInFromSettings(test_account, previously_signed_in_accounts);

    SignInTestObserver observer(identity_manager(), account_reconcilor());
    EXPECT_TRUE(login_ui_test_utils::ConfirmSyncConfirmationDialog(
        browser(), kDialogTimeout));
    observer.WaitForAccountChanges(previously_signed_in_accounts + 1,
                                   PrimarySyncAccountWait::kWaitForAdded);
  }

  void SignOutFromWeb() {
    SignInTestObserver observer(identity_manager(), account_reconcilor());
    AddTabAtIndex(0, GaiaUrls::GetInstance()->service_logout_url(),
                  ui::PageTransition::PAGE_TRANSITION_TYPED);
    observer.WaitForAccountChanges(0, PrimarySyncAccountWait::kNotWait);
  }

  void TurnOffSync() {
    GURL settings_url("chrome://settings");
    AddTabAtIndex(0, settings_url, ui::PageTransition::PAGE_TRANSITION_TYPED);
    SignInTestObserver observer(identity_manager(), account_reconcilor());
    auto* settings_tab = browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(content::ExecuteScript(
        settings_tab,
        base::StringPrintf(
            kSettingsScriptWrapperFormat,
            "settings.SyncBrowserProxyImpl.getInstance().signOut(false)")));
    observer.WaitForAccountChanges(0, PrimarySyncAccountWait::kWaitForCleared);
  }

  signin::IdentityManager* identity_manager() {
    return identity_manager(browser());
  }

  signin::IdentityManager* identity_manager(Browser* browser) {
    return IdentityManagerFactory::GetForProfile(browser->profile());
  }

  syncer::SyncService* sync_service() { return sync_service(browser()); }

  syncer::SyncService* sync_service(Browser* browser) {
    return ProfileSyncServiceFactory::GetForProfile(browser->profile());
  }

  AccountReconcilor* account_reconcilor() {
    return account_reconcilor(browser());
  }

  AccountReconcilor* account_reconcilor(Browser* browser) {
    return AccountReconcilorFactory::GetForProfile(browser->profile());
  }
};

// This test can pass. Marked as manual because it TIMED_OUT on Win7.
// See crbug.com/1025335.
// Sings in an account through the settings page and checks that the account is
// added to Chrome. Sync should be disabled because the test doesn't pass
// through the Sync confirmation dialog.
IN_PROC_BROWSER_TEST_F(LiveSignInTest, MANUAL_SimpleSignInFlow) {
  TestAccount ta;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", ta));
  SignInFromSettings(ta, 0);

  const AccountsInCookieJarInfo& accounts_in_cookie_jar =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar.accounts_are_fresh);
  ASSERT_EQ(1u, accounts_in_cookie_jar.signed_in_accounts.size());
  EXPECT_TRUE(accounts_in_cookie_jar.signed_out_accounts.empty());
  const gaia::ListedAccount& account =
      accounts_in_cookie_jar.signed_in_accounts[0];
  EXPECT_TRUE(gaia::AreEmailsSame(ta.user, account.email));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account.id));
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());
}

// This test can pass. Marked as manual because it TIMED_OUT on Win7.
// See crbug.com/1025335.
// Signs in an account through the settings page and enables Sync. Checks that
// Sync is enabled.
// Then, signs out on the web and checks that the account is removed from
// cookies and Sync paused error is displayed.
IN_PROC_BROWSER_TEST_F(LiveSignInTest, MANUAL_WebSignOut) {
  TestAccount test_account;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", test_account));
  TurnOnSync(test_account, 0);

  const CoreAccountInfo& primary_account =
      identity_manager()->GetPrimaryAccountInfo();
  EXPECT_FALSE(primary_account.IsEmpty());
  EXPECT_TRUE(gaia::AreEmailsSame(test_account.user, primary_account.email));
  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());

  SignOutFromWeb();

  const AccountsInCookieJarInfo& accounts_in_cookie_jar =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar.accounts_are_fresh);
  ASSERT_TRUE(accounts_in_cookie_jar.signed_in_accounts.empty());
  ASSERT_EQ(1u, accounts_in_cookie_jar.signed_out_accounts.size());
  EXPECT_TRUE(gaia::AreEmailsSame(
      test_account.user, accounts_in_cookie_jar.signed_out_accounts[0].email));
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account.account_id));
#if !defined(OS_CHROMEOS)
  int unused1, unused2;
  EXPECT_EQ(sync_ui_util::GetMessagesForAvatarSyncError(browser()->profile(),
                                                        &unused1, &unused2),
            sync_ui_util::AUTH_ERROR);
#endif  // !defined(OS_CHROMEOS)
}

// This test can pass. Marked as manual because it TIMED_OUT on Win7.
// See crbug.com/1025335.
// Sings in two accounts on the web and checks that cookies and refresh tokens
// are added to Chrome. Sync should be disabled.
// Then, signs out on the web and checks that accounts are removed from Chrome.
IN_PROC_BROWSER_TEST_F(LiveSignInTest, MANUAL_WebSignInAndSignOut) {
  TestAccount test_account_1;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", test_account_1));
  SignInFromWeb(test_account_1, 0);

  const AccountsInCookieJarInfo& accounts_in_cookie_jar_1 =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar_1.accounts_are_fresh);
  ASSERT_EQ(1u, accounts_in_cookie_jar_1.signed_in_accounts.size());
  EXPECT_TRUE(accounts_in_cookie_jar_1.signed_out_accounts.empty());
  const gaia::ListedAccount& account_1 =
      accounts_in_cookie_jar_1.signed_in_accounts[0];
  EXPECT_TRUE(gaia::AreEmailsSame(test_account_1.user, account_1.email));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_1.id));
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());

  TestAccount test_account_2;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_2", test_account_2));
  SignInFromWeb(test_account_2, 1);

  const AccountsInCookieJarInfo& accounts_in_cookie_jar_2 =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar_2.accounts_are_fresh);
  ASSERT_EQ(2u, accounts_in_cookie_jar_2.signed_in_accounts.size());
  EXPECT_TRUE(accounts_in_cookie_jar_2.signed_out_accounts.empty());
  EXPECT_EQ(accounts_in_cookie_jar_2.signed_in_accounts[0].id, account_1.id);
  const gaia::ListedAccount& account_2 =
      accounts_in_cookie_jar_2.signed_in_accounts[1];
  EXPECT_TRUE(gaia::AreEmailsSame(test_account_2.user, account_2.email));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_2.id));
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());

  SignOutFromWeb();

  const AccountsInCookieJarInfo& accounts_in_cookie_jar_3 =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar_3.accounts_are_fresh);
  ASSERT_TRUE(accounts_in_cookie_jar_3.signed_in_accounts.empty());
  EXPECT_EQ(2u, accounts_in_cookie_jar_3.signed_out_accounts.size());
  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());
}

// This test can pass. Marked as manual because it TIMED_OUT on Win7.
// See crbug.com/1025335.
// Signs in an account through the settings page and enables Sync. Checks that
// Sync is enabled. Signs in a second account on the web.
// Then, turns Sync off from the settings page and checks that both accounts are
// removed from Chrome and from cookies.
IN_PROC_BROWSER_TEST_F(LiveSignInTest, MANUAL_TurnOffSync) {
  TestAccount test_account_1;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", test_account_1));
  TurnOnSync(test_account_1, 0);

  TestAccount test_account_2;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_2", test_account_2));
  SignInFromWeb(test_account_2, 1);

  const CoreAccountInfo& primary_account =
      identity_manager()->GetPrimaryAccountInfo();
  EXPECT_FALSE(primary_account.IsEmpty());
  EXPECT_TRUE(gaia::AreEmailsSame(test_account_1.user, primary_account.email));
  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());

  TurnOffSync();

  const AccountsInCookieJarInfo& accounts_in_cookie_jar_2 =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar_2.accounts_are_fresh);
  ASSERT_TRUE(accounts_in_cookie_jar_2.signed_in_accounts.empty());
  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
}

// This test can pass. Marked as manual because it TIMED_OUT on Win7.
// See crbug.com/1025335.
// Signs in an account on the web. Goes to the Chrome settings to enable Sync
// but cancels the sync confirmation dialog. Checks that the account is still
// signed in on the web but Sync is disabled.
IN_PROC_BROWSER_TEST_F(LiveSignInTest, MANUAL_CancelSyncWithWebAccount) {
  TestAccount test_account;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", test_account));
  SignInFromWeb(test_account, 0);

  SignInTestObserver observer(identity_manager(), account_reconcilor());
  GURL settings_url("chrome://settings");
  AddTabAtIndex(0, settings_url, ui::PageTransition::PAGE_TRANSITION_TYPED);
  auto* settings_tab = browser()->tab_strip_model()->GetActiveWebContents();
  std::string start_syncing_script = base::StringPrintf(
      "settings.SyncBrowserProxyImpl.getInstance()."
      "startSyncingWithEmail(\"%s\", true);",
      test_account.user.c_str());
  EXPECT_TRUE(content::ExecuteScript(
      settings_tab, base::StringPrintf(kSettingsScriptWrapperFormat,
                                       start_syncing_script.c_str())));
  EXPECT_TRUE(login_ui_test_utils::CancelSyncConfirmationDialog(
      browser(), kDialogTimeout));
  observer.WaitForAccountChanges(1, PrimarySyncAccountWait::kWaitForCleared);

  const AccountsInCookieJarInfo& accounts_in_cookie_jar =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar.accounts_are_fresh);
  ASSERT_EQ(1u, accounts_in_cookie_jar.signed_in_accounts.size());
  const gaia::ListedAccount& account =
      accounts_in_cookie_jar.signed_in_accounts[0];
  EXPECT_TRUE(gaia::AreEmailsSame(test_account.user, account.email));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account.id));
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
}

// This test can pass. Marked as manual because it TIMED_OUT on Win7.
// See crbug.com/1025335.
// Starts the sign in flow from the settings page, enters credentials on the
// login page but cancels the Sync confirmation dialog. Checks that Sync is
// disabled and no account was added to Chrome.
IN_PROC_BROWSER_TEST_F(LiveSignInTest, MANUAL_CancelSync) {
  TestAccount test_account;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", test_account));
  SignInFromSettings(test_account, 0);

  SignInTestObserver observer(identity_manager(), account_reconcilor());
  EXPECT_TRUE(login_ui_test_utils::CancelSyncConfirmationDialog(
      browser(), kDialogTimeout));
  observer.WaitForAccountChanges(0, PrimarySyncAccountWait::kWaitForCleared);

  const AccountsInCookieJarInfo& accounts_in_cookie_jar =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar.accounts_are_fresh);
  EXPECT_TRUE(accounts_in_cookie_jar.signed_in_accounts.empty());
  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
}

// This test can pass. Marked as manual because it TIMED_OUT on Win7.
// See crbug.com/1025335.
// Enables and disables sync to account 1. Enables sync to account 2 and clicks
// on "This wasn't me" in the email confirmation dialog. Checks that the new
// profile is created. Checks that Sync to account 2 is enabled in the new
// profile. Checks that account 2 was removed from the original profile.
IN_PROC_BROWSER_TEST_F(LiveSignInTest,
                       MANUAL_SyncSecondAccount_CreateNewProfile) {
  // Enable and disable sync for the first account.
  TestAccount test_account_1;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", test_account_1));
  TurnOnSync(test_account_1, 0);
  TurnOffSync();

  // Start enable sync for the second account.
  TestAccount test_account_2;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_2", test_account_2));
  SignInFromSettings(test_account_2, 0);

  // Set up an observer for removing the second account from the original
  // profile.
  SignInTestObserver original_browser_observer(identity_manager(),
                                               account_reconcilor());

  // Check there is only one profile.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  EXPECT_EQ(profile_manager->GetNumberOfProfiles(), 1U);
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 1U);

  // Click "This wasn't me" on the email confirmation dialog and wait for a new
  // browser and profile created.
  EXPECT_TRUE(login_ui_test_utils::CompleteSigninEmailConfirmationDialog(
      browser(), kDialogTimeout,
      SigninEmailConfirmationDialog::CREATE_NEW_USER));
  Browser* new_browser = ui_test_utils::WaitForBrowserToOpen();
  EXPECT_EQ(profile_manager->GetNumberOfProfiles(), 2U);
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 2U);
  EXPECT_NE(browser()->profile(), new_browser->profile());

  // Confirm sync in the new browser window.
  SignInTestObserver new_browser_observer(identity_manager(new_browser),
                                          account_reconcilor(new_browser));
  EXPECT_TRUE(login_ui_test_utils::ConfirmSyncConfirmationDialog(
      new_browser, kDialogTimeout));
  new_browser_observer.WaitForAccountChanges(
      1, PrimarySyncAccountWait::kWaitForAdded);

  // Check accounts in cookies in the new profile.
  const AccountsInCookieJarInfo& accounts_in_cookie_jar =
      identity_manager(new_browser)->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar.accounts_are_fresh);
  ASSERT_EQ(1u, accounts_in_cookie_jar.signed_in_accounts.size());
  const gaia::ListedAccount& account =
      accounts_in_cookie_jar.signed_in_accounts[0];
  EXPECT_TRUE(gaia::AreEmailsSame(test_account_2.user, account.email));

  // Check the primary account in the new profile is set and syncing.
  const CoreAccountInfo& primary_account =
      identity_manager(new_browser)->GetPrimaryAccountInfo();
  EXPECT_FALSE(primary_account.IsEmpty());
  EXPECT_TRUE(gaia::AreEmailsSame(test_account_2.user, primary_account.email));
  EXPECT_TRUE(identity_manager(new_browser)
                  ->HasAccountWithRefreshToken(primary_account.account_id));
  EXPECT_TRUE(sync_service(new_browser)->IsSyncFeatureEnabled());

  // Check that the second account was removed from the original profile.
  original_browser_observer.WaitForAccountChanges(
      0, PrimarySyncAccountWait::kWaitForCleared);
  const AccountsInCookieJarInfo& accounts_in_cookie_jar_2 =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar_2.accounts_are_fresh);
  ASSERT_TRUE(accounts_in_cookie_jar_2.signed_in_accounts.empty());
  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
}

// This test can pass. Marked as manual because it TIMED_OUT on Win7.
// See crbug.com/1025335.
// Enables and disables sync to account 1. Enables sync to account 2 and clicks
// on "This was me" in the email confirmation dialog. Checks that Sync to
// account 2 is enabled in the current profile.
IN_PROC_BROWSER_TEST_F(LiveSignInTest,
                       MANUAL_SyncSecondAccount_InExistingProfile) {
  // Enable and disable sync for the first account.
  TestAccount test_account_1;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", test_account_1));
  TurnOnSync(test_account_1, 0);
  TurnOffSync();

  // Start enable sync for the second account.
  TestAccount test_account_2;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_2", test_account_2));
  SignInFromSettings(test_account_2, 0);

  // Check there is only one profile.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  EXPECT_EQ(profile_manager->GetNumberOfProfiles(), 1U);
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 1U);

  // Click "This was me" on the email confirmation dialog, confirm sync and wait
  // for a primary account to be set.
  SignInTestObserver observer(identity_manager(), account_reconcilor());
  EXPECT_TRUE(login_ui_test_utils::CompleteSigninEmailConfirmationDialog(
      browser(), kDialogTimeout, SigninEmailConfirmationDialog::START_SYNC));
  EXPECT_TRUE(login_ui_test_utils::ConfirmSyncConfirmationDialog(
      browser(), kDialogTimeout));
  observer.WaitForAccountChanges(1, PrimarySyncAccountWait::kWaitForAdded);

  // Check no profile was created.
  EXPECT_EQ(profile_manager->GetNumberOfProfiles(), 1U);
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 1U);

  // Check accounts in cookies.
  const AccountsInCookieJarInfo& accounts_in_cookie_jar =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar.accounts_are_fresh);
  ASSERT_EQ(1u, accounts_in_cookie_jar.signed_in_accounts.size());
  const gaia::ListedAccount& account =
      accounts_in_cookie_jar.signed_in_accounts[0];
  EXPECT_TRUE(gaia::AreEmailsSame(test_account_2.user, account.email));

  // Check the primary account is set and syncing.
  const CoreAccountInfo& primary_account =
      identity_manager()->GetPrimaryAccountInfo();
  EXPECT_FALSE(primary_account.IsEmpty());
  EXPECT_TRUE(gaia::AreEmailsSame(test_account_2.user, primary_account.email));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      primary_account.account_id));
  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());
}

// This test can pass. Marked as manual because it TIMED_OUT on Win7.
// See crbug.com/1025335.
// Enables and disables sync to account 1. Enables sync to account 2 and clicks
// on "Cancel" in the email confirmation dialog. Checks that the signin flow is
// canceled and no accounts are added to Chrome.
IN_PROC_BROWSER_TEST_F(LiveSignInTest,
                       MANUAL_SyncSecondAccount_CancelOnEmailConfirmation) {
  // Enable and disable sync for the first account.
  TestAccount test_account_1;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", test_account_1));
  TurnOnSync(test_account_1, 0);
  TurnOffSync();

  // Start enable sync for the second account.
  TestAccount test_account_2;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_2", test_account_2));
  SignInFromSettings(test_account_2, 0);

  // Check there is only one profile.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  EXPECT_EQ(profile_manager->GetNumberOfProfiles(), 1U);
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 1U);

  // Click "Cancel" on the email confirmation dialog and wait for an account to
  // removed from Chrome.
  SignInTestObserver observer(identity_manager(), account_reconcilor());
  EXPECT_TRUE(login_ui_test_utils::CompleteSigninEmailConfirmationDialog(
      browser(), kDialogTimeout, SigninEmailConfirmationDialog::CLOSE));
  observer.WaitForAccountChanges(0, PrimarySyncAccountWait::kWaitForCleared);

  // Check no profile was created.
  EXPECT_EQ(profile_manager->GetNumberOfProfiles(), 1U);
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 1U);

  // Check Chrome has no accounts.
  const AccountsInCookieJarInfo& accounts_in_cookie_jar =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar.accounts_are_fresh);
  EXPECT_TRUE(accounts_in_cookie_jar.signed_in_accounts.empty());
  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
}

}  // namespace test
}  // namespace signin
