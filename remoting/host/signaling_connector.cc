// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/signaling_connector.h"

#include "base/bind.h"
#include "base/callback.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/url_request_context.h"

namespace remoting {

namespace {

// The delay between reconnect attempts will increase exponentially up
// to the maximum specified here.
const int kMaxReconnectDelaySeconds = 10 * 60;

// Time when we we try to update OAuth token before its expiration.
const int kTokenUpdateTimeBeforeExpirySeconds = 60;

}  // namespace

SignalingConnector::OAuthCredentials::OAuthCredentials(
    const std::string& login_value,
    const std::string& refresh_token_value,
    const OAuthClientInfo& client_info_value)
    : login(login_value),
      refresh_token(refresh_token_value),
      client_info(client_info_value) {
}

SignalingConnector::SignalingConnector(
    XmppSignalStrategy* signal_strategy,
    const base::Closure& auth_failed_callback)
    : signal_strategy_(signal_strategy),
      auth_failed_callback_(auth_failed_callback),
      reconnect_attempts_(0),
      refreshing_oauth_token_(false) {
  DCHECK(!auth_failed_callback_.is_null());
  net::NetworkChangeNotifier::AddOnlineStateObserver(this);
  net::NetworkChangeNotifier::AddIPAddressObserver(this);
  signal_strategy_->AddListener(this);
  ScheduleTryReconnect();
}

SignalingConnector::~SignalingConnector() {
  signal_strategy_->RemoveListener(this);
  net::NetworkChangeNotifier::RemoveOnlineStateObserver(this);
  net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
}

void SignalingConnector::EnableOAuth(
    scoped_ptr<OAuthCredentials> oauth_credentials,
    net::URLRequestContextGetter* url_context) {
  oauth_credentials_ = oauth_credentials.Pass();
  gaia_oauth_client_.reset(new GaiaOAuthClient(
      OAuthProviderInfo::GetDefault(), url_context));
}

void SignalingConnector::OnSignalStrategyStateChange(
    SignalStrategy::State state) {
  DCHECK(CalledOnValidThread());

  if (state == SignalStrategy::CONNECTED) {
    LOG(INFO) << "Signaling connected.";
    reconnect_attempts_ = 0;
  } else if (state == SignalStrategy::DISCONNECTED) {
    LOG(INFO) << "Signaling disconnected.";
    reconnect_attempts_++;

    // If authentication failed then we have an invalid OAuth token,
    // inform the upper layer about it.
    if (signal_strategy_->GetError() == SignalStrategy::AUTHENTICATION_FAILED) {
      auth_failed_callback_.Run();
    } else {
      ScheduleTryReconnect();
    }
  }
}

void SignalingConnector::OnIPAddressChanged() {
  DCHECK(CalledOnValidThread());
  LOG(INFO) << "IP address has changed.";
  ResetAndTryReconnect();
}

void SignalingConnector::OnOnlineStateChanged(bool online) {
  DCHECK(CalledOnValidThread());
  if (online) {
    LOG(INFO) << "Network state changed to online.";
    ResetAndTryReconnect();
  }
}

void SignalingConnector::OnRefreshTokenResponse(const std::string& user_email,
                                                const std::string& access_token,
                                                int expires_seconds) {
  DCHECK(CalledOnValidThread());
  DCHECK(oauth_credentials_.get());
  LOG(INFO) << "Received OAuth token.";

  if (user_email != oauth_credentials_->login) {
    LOG(ERROR) << "OAuth token and email address do not refer to "
        "the same account.";
    auth_failed_callback_.Run();
    return;
  }

  refreshing_oauth_token_ = false;
  auth_token_expiry_time_ = base::Time::Now() +
      base::TimeDelta::FromSeconds(expires_seconds) -
      base::TimeDelta::FromSeconds(kTokenUpdateTimeBeforeExpirySeconds);
  signal_strategy_->SetAuthInfo(oauth_credentials_->login,
                                access_token, "oauth2");

  // Now that we've got the new token, try to connect using it.
  DCHECK_EQ(signal_strategy_->GetState(), SignalStrategy::DISCONNECTED);
  signal_strategy_->Connect();
}

void SignalingConnector::OnOAuthError() {
  DCHECK(CalledOnValidThread());
  LOG(ERROR) << "OAuth: invalid credentials.";
  refreshing_oauth_token_ = false;
  reconnect_attempts_++;
  auth_failed_callback_.Run();
}

void SignalingConnector::OnNetworkError(int response_code) {
  DCHECK(CalledOnValidThread());
  LOG(ERROR) << "Network error when trying to update OAuth token: "
             << response_code;
  refreshing_oauth_token_ = false;
  reconnect_attempts_++;
  ScheduleTryReconnect();
}

void SignalingConnector::ScheduleTryReconnect() {
  DCHECK(CalledOnValidThread());
  if (timer_.IsRunning() || net::NetworkChangeNotifier::IsOffline())
    return;
  int delay_s = std::min(1 << (reconnect_attempts_ * 2),
                         kMaxReconnectDelaySeconds);
  timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(delay_s),
               this, &SignalingConnector::TryReconnect);
}

void SignalingConnector::ResetAndTryReconnect() {
  DCHECK(CalledOnValidThread());
  signal_strategy_->Disconnect();
  reconnect_attempts_ = 0;
  timer_.Stop();
  ScheduleTryReconnect();
}

void SignalingConnector::TryReconnect() {
  DCHECK(CalledOnValidThread());
  if (signal_strategy_->GetState() == SignalStrategy::DISCONNECTED) {
    bool need_new_auth_token = oauth_credentials_.get() &&
        (auth_token_expiry_time_.is_null() ||
         base::Time::Now() >= auth_token_expiry_time_);
    if (need_new_auth_token) {
      RefreshOAuthToken();
    } else {
      LOG(INFO) << "Attempting to connect signaling.";
      signal_strategy_->Connect();
    }
  }
}

void SignalingConnector::RefreshOAuthToken() {
  DCHECK(CalledOnValidThread());
  LOG(INFO) << "Refreshing OAuth token.";
  DCHECK(!refreshing_oauth_token_);

  refreshing_oauth_token_ = true;
  gaia_oauth_client_->RefreshToken(
      oauth_credentials_->client_info,
      oauth_credentials_->refresh_token, this);
}

}  // namespace remoting
