// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/logged_in_user_mixin.h"

#include "chromeos/login/auth/stub_authenticator_builder.h"
#include "chromeos/login/auth/user_context.h"
#include "net/dns/mock_host_resolver.h"

namespace chromeos {

namespace {

constexpr char kTestUserName[] = "user@gmail.com";
constexpr char kTestUserGaiaId[] = "user";

user_manager::UserType ConvertUserType(LoggedInUserMixin::LogInType type) {
  switch (type) {
    case LoggedInUserMixin::LogInType::kChild:
      return user_manager::USER_TYPE_CHILD;
    case LoggedInUserMixin::LogInType::kRegular:
      return user_manager::USER_TYPE_REGULAR;
  }
}

}  // namespace

LoggedInUserMixin::LoggedInUserMixin(
    InProcessBrowserTestMixinHost* mixin_host,
    LogInType type,
    net::EmbeddedTestServer* embedded_test_server,
    bool should_launch_browser)
    : user_(AccountId::FromUserEmailGaiaId(kTestUserName, kTestUserGaiaId),
            ConvertUserType(type)),
      login_manager_(mixin_host, {user_}),
      policy_server_(mixin_host),
      user_policy_(mixin_host, user_.account_id, &policy_server_),
      embedded_test_server_setup_(mixin_host, embedded_test_server),
      fake_gaia_(mixin_host, embedded_test_server) {
  // By default, LoginManagerMixin will set up user session manager not to
  // launch browser as part of user session setup - use this to override that
  // behavior.
  login_manager_.set_should_launch_browser(should_launch_browser);
}

LoggedInUserMixin::~LoggedInUserMixin() = default;

void LoggedInUserMixin::SetUpOnMainThreadHelper(
    net::RuleBasedHostResolverProc* host_resolver,
    InProcessBrowserTest* test_base,
    bool issue_any_scope_token,
    bool wait_for_active_session) {
  // By default, browser tests block anything that doesn't go to localhost, so
  // account.google.com requests would never reach fake GAIA server without
  // this.
  host_resolver->AddRule("*", "127.0.0.1");
  LogInUser(issue_any_scope_token, wait_for_active_session);
  // Set the private |browser_| member in InProcessBrowserTest.
  // Otherwise calls to InProcessBrowserTest::browser() returns null and leads
  // to segmentation faults.
  // Note: |browser_| is only non-null if should_launch_browser was set to true
  // in the constructor.
  test_base->SelectFirstBrowser();
}

void LoggedInUserMixin::LogInUser(bool issue_any_scope_token,
                                  bool wait_for_active_session) {
  UserContext user_context = LoginManagerMixin::CreateDefaultUserContext(user_);
  if (user_.user_type == user_manager::USER_TYPE_CHILD) {
    fake_gaia_.SetupFakeGaiaForChildUser(
        user_.account_id.GetUserEmail(), user_.account_id.GetGaiaId(),
        FakeGaiaMixin::kFakeRefreshToken, issue_any_scope_token);
  } else {
    fake_gaia_.SetupFakeGaiaForLogin(user_.account_id.GetUserEmail(),
                                     user_.account_id.GetGaiaId(),
                                     FakeGaiaMixin::kFakeRefreshToken);
  }
  user_context.SetRefreshToken(FakeGaiaMixin::kFakeRefreshToken);
  if (wait_for_active_session) {
    login_manager_.LoginAndWaitForActiveSession(user_context);
  } else {
    login_manager_.AttemptLoginUsingAuthenticator(
        user_context, std::make_unique<StubAuthenticatorBuilder>(user_context));
  }
}

}  // namespace chromeos
