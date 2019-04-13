// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/test_oauth_token_getter.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "remoting/base/fake_oauth_token_getter.h"
#include "remoting/base/oauth_token_getter_impl.h"
#include "remoting/base/url_request_context_getter.h"
#include "remoting/test/cli_util.h"
#include "remoting/test/test_token_storage.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/transitional_url_loader_factory_owner.h"

namespace remoting {
namespace test {

namespace {

constexpr char kChromotingAuthScopeValues[] =
    "https://www.googleapis.com/auth/chromoting "
    "https://www.googleapis.com/auth/googletalk "
    "https://www.googleapis.com/auth/userinfo.email "
    "https://www.googleapis.com/auth/tachyon";

constexpr char kOauthRedirectUrl[] =
    "https://chromoting-oauth.talkgadget."
    "google.com/talkgadget/oauth/chrome-remote-desktop/dev";

std::string GetAuthorizationCodeUri() {
  // Replace space characters with a '+' sign when formatting.
  bool use_plus = true;
  return base::StringPrintf(
      "https://accounts.google.com/o/oauth2/auth"
      "?scope=%s"
      "&redirect_uri=https://chromoting-oauth.talkgadget.google.com/"
      "talkgadget/oauth/chrome-remote-desktop/dev"
      "&response_type=code"
      "&client_id=%s"
      "&access_type=offline",
      net::EscapeUrlEncodedData(kChromotingAuthScopeValues, use_plus).c_str(),
      net::EscapeUrlEncodedData(
          google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING),
          use_plus)
          .c_str());
}

}  // namespace

constexpr char TestOAuthTokenGetter::kSwitchNameAuthCode[];

TestOAuthTokenGetter::TestOAuthTokenGetter(TestTokenStorage* token_storage)
    : weak_factory_(this) {
  DCHECK(token_storage);
  token_storage_ = token_storage;
  auto url_request_context_getter =
      base::MakeRefCounted<URLRequestContextGetter>(
          base::ThreadTaskRunnerHandle::Get());
  url_loader_factory_owner_ =
      std::make_unique<network::TransitionalURLLoaderFactoryOwner>(
          url_request_context_getter);
}

TestOAuthTokenGetter::~TestOAuthTokenGetter() = default;

void TestOAuthTokenGetter::Initialize(base::OnceClosure on_done) {
  std::string user_email = token_storage_->FetchUserEmail();
  std::string access_token = token_storage_->FetchAccessToken();
  if (user_email.empty() || access_token.empty()) {
    ResetWithAuthenticationFlow(std::move(on_done));
    return;
  }
  VLOG(0) << "Reusing user_email: " << user_email << ", "
          << "access_token: " << access_token;
  token_getter_ = std::make_unique<FakeOAuthTokenGetter>(
      OAuthTokenGetter::Status::SUCCESS, user_email, access_token);
  std::move(on_done).Run();
}

void TestOAuthTokenGetter::ResetWithAuthenticationFlow(
    base::OnceClosure on_done) {
  on_authentication_done_.push(std::move(on_done));
  InvalidateCache();
}

void TestOAuthTokenGetter::CallWithToken(TokenCallback on_access_token) {
  token_getter_->CallWithToken(std::move(on_access_token));
}

void TestOAuthTokenGetter::InvalidateCache() {
  if (is_authenticating_) {
    return;
  }

  is_authenticating_ = true;

  static const std::string read_auth_code_prompt = base::StringPrintf(
      "Please authenticate at:\n\n"
      "  %s\n\n"
      "Enter the auth code: ",
      GetAuthorizationCodeUri().c_str());
  std::string auth_code = test::ReadStringFromCommandLineOrStdin(
      kSwitchNameAuthCode, read_auth_code_prompt);

  // Make sure we don't try to reuse an auth code.
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(kSwitchNameAuthCode);

  // We can't get back the refresh token since we have first-party scope, so
  // we are not trying to store it.
  token_getter_ = CreateFromIntermediateCredentials(
      auth_code,
      base::DoNothing::Repeatedly<const std::string&, const std::string&>());

  // Get the access token so that we can reuse it for next time.
  token_getter_->CallWithToken(base::BindOnce(
      &TestOAuthTokenGetter::OnAccessToken, weak_factory_.GetWeakPtr()));
}

std::unique_ptr<OAuthTokenGetter>
TestOAuthTokenGetter::CreateFromIntermediateCredentials(
    const std::string& auth_code,
    const OAuthTokenGetter::CredentialsUpdatedCallback& on_credentials_update) {
  auto oauth_credentials =
      std::make_unique<OAuthTokenGetter::OAuthIntermediateCredentials>(
          auth_code, /* is_service_account */ false);
  oauth_credentials->oauth_redirect_uri = kOauthRedirectUrl;

  return std::make_unique<OAuthTokenGetterImpl>(
      std::move(oauth_credentials), on_credentials_update,
      url_loader_factory_owner_->GetURLLoaderFactory(),
      /* auto_refresh */ true);
}

void TestOAuthTokenGetter::OnAccessToken(OAuthTokenGetter::Status status,
                                         const std::string& user_email,
                                         const std::string& access_token) {
  is_authenticating_ = false;
  if (status != OAuthTokenGetter::Status::SUCCESS) {
    fprintf(stderr,
            "Failed to authenticate. Please check if your access  token is "
            "correct.\n");
    InvalidateCache();
    return;
  }
  VLOG(0) << "Received access_token: " << access_token;
  token_storage_->StoreUserEmail(user_email);
  token_storage_->StoreAccessToken(access_token);
  while (!on_authentication_done_.empty()) {
    std::move(on_authentication_done_.front()).Run();
    on_authentication_done_.pop();
  }
}

}  // namespace test
}  // namespace remoting
