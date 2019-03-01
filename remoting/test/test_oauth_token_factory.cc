// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/test_oauth_token_factory.h"

#include <utility>

#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "remoting/base/oauth_token_getter_impl.h"
#include "remoting/base/url_request_context_getter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/transitional_url_loader_factory_owner.h"

namespace remoting {

namespace {

constexpr char kChromotingAuthScopeValues[] =
    "https://www.googleapis.com/auth/chromoting "
    "https://www.googleapis.com/auth/googletalk "
    "https://www.googleapis.com/auth/userinfo.email "
    "https://www.googleapis.com/auth/tachyon";

constexpr char kOauthRedirectUrl[] =
    "https://chromoting-oauth.talkgadget."
    "google.com/talkgadget/oauth/chrome-remote-desktop/dev";

}  // namespace

TestOAuthTokenGetterFactory::TestOAuthTokenGetterFactory() {
  auto url_request_context_getter =
      base::MakeRefCounted<URLRequestContextGetter>(
          base::ThreadTaskRunnerHandle::Get());
  url_loader_factory_owner_ =
      std::make_unique<network::TransitionalURLLoaderFactoryOwner>(
          url_request_context_getter);
}

TestOAuthTokenGetterFactory::~TestOAuthTokenGetterFactory() = default;

// static
std::string TestOAuthTokenGetterFactory::GetAuthorizationCodeUri() {
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

std::unique_ptr<OAuthTokenGetter>
TestOAuthTokenGetterFactory::CreateFromIntermediateCredentials(
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

}  // namespace remoting
