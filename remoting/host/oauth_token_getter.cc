// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/oauth_token_getter.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string_util.h"
#include "google_apis/google_api_keys.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/logging.h"

namespace remoting {

namespace {

// Maximum number of retries on network/500 errors.
const int kMaxRetries = 3;

// Time when we we try to update OAuth token before its expiration.
const int kTokenUpdateTimeBeforeExpirySeconds = 60;

}  // namespace

OAuthTokenGetter::OAuthCredentials::OAuthCredentials(
    const std::string& login,
    const std::string& refresh_token,
    bool is_service_account)
    : login(login),
      refresh_token(refresh_token),
      is_service_account(is_service_account) {
}

OAuthTokenGetter::OAuthTokenGetter(
    scoped_ptr<OAuthCredentials> oauth_credentials,
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
    bool auto_refresh)
    : oauth_credentials_(oauth_credentials.Pass()),
      gaia_oauth_client_(
          new gaia::GaiaOAuthClient(url_request_context_getter.get())),
      url_request_context_getter_(url_request_context_getter),
      refreshing_oauth_token_(false) {
  if (auto_refresh) {
    refresh_timer_.reset(new base::OneShotTimer<OAuthTokenGetter>());
  }
}

OAuthTokenGetter::~OAuthTokenGetter() {}

void OAuthTokenGetter::OnGetTokensResponse(const std::string& user_email,
                                           const std::string& access_token,
                                           int expires_seconds) {
  NOTREACHED();
}

void OAuthTokenGetter::OnRefreshTokenResponse(
    const std::string& access_token,
    int expires_seconds) {
  DCHECK(CalledOnValidThread());
  DCHECK(oauth_credentials_.get());
  HOST_LOG << "Received OAuth token.";

  oauth_access_token_ = access_token;
  base::TimeDelta token_expiration =
      base::TimeDelta::FromSeconds(expires_seconds) -
      base::TimeDelta::FromSeconds(kTokenUpdateTimeBeforeExpirySeconds);
  auth_token_expiry_time_ = base::Time::Now() + token_expiration;

  if (refresh_timer_) {
    refresh_timer_->Stop();
    refresh_timer_->Start(FROM_HERE, token_expiration, this,
                          &OAuthTokenGetter::RefreshOAuthToken);
  }

  if (verified_email_.empty()) {
    gaia_oauth_client_->GetUserEmail(access_token, kMaxRetries, this);
  } else {
    refreshing_oauth_token_ = false;
    NotifyCallbacks(
        OAuthTokenGetter::SUCCESS, verified_email_, oauth_access_token_);
  }
}

void OAuthTokenGetter::OnGetUserEmailResponse(const std::string& user_email) {
  DCHECK(CalledOnValidThread());
  DCHECK(oauth_credentials_.get());
  HOST_LOG << "Received user info.";

  if (user_email != oauth_credentials_->login) {
    LOG(ERROR) << "OAuth token and email address do not refer to "
        "the same account.";
    OnOAuthError();
    return;
  }

  verified_email_ = user_email;
  refreshing_oauth_token_ = false;

  // Now that we've refreshed the token and verified that it's for the correct
  // user account, try to connect using the new token.
  NotifyCallbacks(OAuthTokenGetter::SUCCESS, user_email, oauth_access_token_);
}

void OAuthTokenGetter::NotifyCallbacks(Status status,
                     const std::string& user_email,
                     const std::string& access_token) {
  std::queue<TokenCallback> callbacks(pending_callbacks_);
  pending_callbacks_ = std::queue<TokenCallback>();

  while (!callbacks.empty()) {
    callbacks.front().Run(status, user_email, access_token);
    callbacks.pop();
  }
}

void OAuthTokenGetter::OnOAuthError() {
  DCHECK(CalledOnValidThread());
  LOG(ERROR) << "OAuth: invalid credentials.";
  refreshing_oauth_token_ = false;

  // Throw away invalid credentials and force a refresh.
  oauth_access_token_.clear();
  auth_token_expiry_time_ = base::Time();
  verified_email_.clear();

  NotifyCallbacks(OAuthTokenGetter::AUTH_ERROR, std::string(), std::string());
}

void OAuthTokenGetter::OnNetworkError(int response_code) {
  DCHECK(CalledOnValidThread());
  LOG(ERROR) << "Network error when trying to update OAuth token: "
             << response_code;
  refreshing_oauth_token_ = false;
  NotifyCallbacks(
      OAuthTokenGetter::NETWORK_ERROR, std::string(), std::string());
}

void OAuthTokenGetter::CallWithToken(const TokenCallback& on_access_token) {
  DCHECK(CalledOnValidThread());
  bool need_new_auth_token = auth_token_expiry_time_.is_null() ||
      base::Time::Now() >= auth_token_expiry_time_ ||
      verified_email_.empty();

  if (need_new_auth_token) {
    pending_callbacks_.push(on_access_token);
    if (!refreshing_oauth_token_)
      RefreshOAuthToken();
  } else {
    on_access_token.Run(
        SUCCESS, oauth_credentials_->login, oauth_access_token_);
  }
}

void OAuthTokenGetter::RefreshOAuthToken() {
  DCHECK(CalledOnValidThread());
  HOST_LOG << "Refreshing OAuth token.";
  DCHECK(!refreshing_oauth_token_);

  // Service accounts use different API keys, as they use the client app flow.
  google_apis::OAuth2Client oauth2_client =
      oauth_credentials_->is_service_account ?
      google_apis::CLIENT_REMOTING_HOST : google_apis::CLIENT_REMOTING;

  gaia::OAuthClientInfo client_info = {
    google_apis::GetOAuth2ClientID(oauth2_client),
    google_apis::GetOAuth2ClientSecret(oauth2_client),
    // Redirect URL is only used when getting tokens from auth code. It
    // is not required when getting access tokens.
    ""
  };

  refreshing_oauth_token_ = true;
  std::vector<std::string> empty_scope_list;  // Use scope from refresh token.
  gaia_oauth_client_->RefreshToken(
      client_info, oauth_credentials_->refresh_token, empty_scope_list,
      kMaxRetries, this);
}

}  // namespace remoting
