// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/signaling_connector.h"

#include "base/bind.h"
#include "base/callback.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/google_api_keys.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/host/dns_blackhole_checker.h"

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
    const std::string& refresh_token_value)
    : login(login_value),
      refresh_token(refresh_token_value) {
}

SignalingConnector::SignalingConnector(
    XmppSignalStrategy* signal_strategy,
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
    scoped_ptr<DnsBlackholeChecker> dns_blackhole_checker,
    const base::Closure& auth_failed_callback)
    : signal_strategy_(signal_strategy),
      url_request_context_getter_(url_request_context_getter),
      auth_failed_callback_(auth_failed_callback),
      dns_blackhole_checker_(dns_blackhole_checker.Pass()),
      reconnect_attempts_(0),
      refreshing_oauth_token_(false) {
  DCHECK(!auth_failed_callback_.is_null());
  DCHECK(dns_blackhole_checker_.get());
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
  net::NetworkChangeNotifier::AddIPAddressObserver(this);
  signal_strategy_->AddListener(this);
  ScheduleTryReconnect();
}

SignalingConnector::~SignalingConnector() {
  signal_strategy_->RemoveListener(this);
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
  net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
}

void SignalingConnector::EnableOAuth(
    scoped_ptr<OAuthCredentials> oauth_credentials) {
  oauth_credentials_ = oauth_credentials.Pass();
  gaia_oauth_client_.reset(
      new gaia::GaiaOAuthClient(GaiaUrls::GetInstance()->oauth2_token_url(),
                                url_request_context_getter_));
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

bool SignalingConnector::OnSignalStrategyIncomingStanza(
    const buzz::XmlElement* stanza) {
  return false;
}

void SignalingConnector::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  DCHECK(CalledOnValidThread());
  if (type != net::NetworkChangeNotifier::CONNECTION_NONE &&
      signal_strategy_->GetState() == SignalStrategy::DISCONNECTED) {
    LOG(INFO) << "Network state changed to online.";
    ResetAndTryReconnect();
  }
}

void SignalingConnector::OnIPAddressChanged() {
  DCHECK(CalledOnValidThread());
  if (signal_strategy_->GetState() == SignalStrategy::DISCONNECTED) {
    LOG(INFO) << "IP address has changed.";
    ResetAndTryReconnect();
  }
}

void SignalingConnector::OnGetTokensResponse(const std::string& user_email,
                                             const std::string& access_token,
                                             int expires_seconds) {
  NOTREACHED();
}

void SignalingConnector::OnRefreshTokenResponse(
    const std::string& access_token,
    int expires_seconds) {
  DCHECK(CalledOnValidThread());
  DCHECK(oauth_credentials_.get());
  LOG(INFO) << "Received OAuth token.";

  oauth_access_token_ = access_token;
  auth_token_expiry_time_ = base::Time::Now() +
      base::TimeDelta::FromSeconds(expires_seconds) -
      base::TimeDelta::FromSeconds(kTokenUpdateTimeBeforeExpirySeconds);

  gaia_oauth_client_->GetUserInfo(access_token, 1, this);
}

void SignalingConnector::OnGetUserInfoResponse(const std::string& user_email) {
  DCHECK(CalledOnValidThread());
  DCHECK(oauth_credentials_.get());
  LOG(INFO) << "Received user info.";

  if (user_email != oauth_credentials_->login) {
    LOG(ERROR) << "OAuth token and email address do not refer to "
        "the same account.";
    auth_failed_callback_.Run();
    return;
  }

  signal_strategy_->SetAuthInfo(oauth_credentials_->login,
                                oauth_access_token_, "oauth2");
  refreshing_oauth_token_ = false;

  // Now that we've refreshed the token and verified that it's for the correct
  // user account, try to connect using the new token.
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
  int delay_s = std::min(1 << reconnect_attempts_,
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
  DCHECK(dns_blackhole_checker_.get());

  // This will check if this machine is allowed to access the chromoting
  // host talkgadget.
  dns_blackhole_checker_->CheckForDnsBlackhole(
      base::Bind(&SignalingConnector::OnDnsBlackholeCheckerDone,
                 base::Unretained(this)));
}

void SignalingConnector::OnDnsBlackholeCheckerDone(bool allow) {
  DCHECK(CalledOnValidThread());

  // Unable to access the host talkgadget. Don't allow the connection, but
  // schedule a reconnect in case this is a transient problem rather than
  // an outright block.
  if (!allow) {
    reconnect_attempts_++;
    LOG(INFO) << "Talkgadget check failed. Scheduling reconnect. Attempt "
              << reconnect_attempts_;
    ScheduleTryReconnect();
    return;
  }

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

  gaia::OAuthClientInfo client_info = {
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING),
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING),
      // Redirect URL is only used when getting tokens from auth code. It
      // is not required when getting access tokens.
      ""
  };

  refreshing_oauth_token_ = true;
  gaia_oauth_client_->RefreshToken(
      client_info, oauth_credentials_->refresh_token, 1, this);
}

}  // namespace remoting
