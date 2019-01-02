// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signin/merge_session_navigation_throttle.h"

#include "base/time/time.h"
#include "chrome/browser/chromeos/login/signin/merge_session_throttling_utils.h"
#include "chrome/browser/chromeos/login/signin/oauth2_login_manager.h"
#include "chrome/browser/chromeos/login/signin/oauth2_login_manager_factory.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"

namespace {

constexpr char kURL[] = "http://www.google.com/";
constexpr int kSessionMergeTimeout = 60;

}  // namespace

namespace chromeos {

class MergeSessionNavigationThrottleTest
    : public ChromeRenderViewHostTestHarness {
 public:
  MergeSessionNavigationThrottleTest() = default;

  bool ManagerHasObservers() {
    OAuth2LoginManager* manager = GetOAuth2LoginManager();
    if (!manager) {
      ADD_FAILURE();
      return false;
    }
    return manager->observer_list_.might_have_observers();
  }

 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // This is needed since the global state is not cleared in
    // merge_session_throttling_utils between tests, and tests can fail
    // depending on the order they are run in otherwise due to the navigation
    // throttle not being created for further navigations.
    merge_session_throttling_utils::ResetAreAllSessionsMergedForTesting();
    FakeChromeUserManager* fake_manager = new FakeChromeUserManager();
    AccountId account_id(AccountId::FromUserEmail("user@example.com"));
    fake_manager->AddUser(account_id);
    fake_manager->LoginUser(account_id);
    user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(std::move(fake_manager)));
  }

  void TearDown() override {
    user_manager_.reset();
    base::RunLoop().RunUntilIdle();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  OAuth2LoginManager* GetOAuth2LoginManager() {
    content::BrowserContext* browser_context =
        web_contents()->GetBrowserContext();
    if (!browser_context)
      return nullptr;

    Profile* profile = Profile::FromBrowserContext(browser_context);
    if (!profile)
      return nullptr;

    OAuth2LoginManager* login_manager =
        OAuth2LoginManagerFactory::GetInstance()->GetForProfile(profile);
    return login_manager;
  }

  void SetMergeSessionState(OAuth2LoginManager::SessionRestoreState state) {
    OAuth2LoginManager* login_manager = GetOAuth2LoginManager();
    ASSERT_TRUE(login_manager);
    login_manager->SetSessionRestoreState(state);
  }

  void SetSessionRestoreStart(const base::Time& time) {
    OAuth2LoginManager* login_manager = GetOAuth2LoginManager();
    ASSERT_TRUE(login_manager);
    login_manager->SetSessionRestoreStartForTesting(time);
  }

 private:
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_;
};

// Tests navigations are not deferred when merge session is already done.
TEST_F(MergeSessionNavigationThrottleTest, NotThrottled) {
  SetMergeSessionState(OAuth2LoginManager::SESSION_RESTORE_DONE);
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL(kURL), web_contents());
  navigation->Start();
  EXPECT_FALSE(navigation->IsDeferred());
  EXPECT_FALSE(ManagerHasObservers());
}

// Tests navigations are deferred when merge session is in progress, and
// resumed once it's done.
TEST_F(MergeSessionNavigationThrottleTest, Throttled) {
  SetMergeSessionState(OAuth2LoginManager::SESSION_RESTORE_IN_PROGRESS);
  SetSessionRestoreStart(base::Time::Now());
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL(kURL), web_contents());
  navigation->SetAutoAdvance(false);
  navigation->Start();
  EXPECT_TRUE(navigation->IsDeferred());
  SetMergeSessionState(OAuth2LoginManager::SESSION_RESTORE_DONE);
  navigation->Wait();
  EXPECT_FALSE(navigation->IsDeferred());
  EXPECT_FALSE(ManagerHasObservers());
}

// Tests navigations are not deferred if merge session started over 60
// seconds ago
TEST_F(MergeSessionNavigationThrottleTest, Timeout) {
  SetMergeSessionState(OAuth2LoginManager::SESSION_RESTORE_IN_PROGRESS);
  SetSessionRestoreStart(base::Time::Now() - base::TimeDelta::FromSeconds(
                                                 kSessionMergeTimeout + 1));
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL(kURL), web_contents());
  navigation->Start();
  EXPECT_FALSE(navigation->IsDeferred());
  EXPECT_FALSE(ManagerHasObservers());
}

}  // namespace chromeos
