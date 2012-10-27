// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SIGNALING_CONNECTOR_H_
#define REMOTING_HOST_SIGNALING_CONNECTOR_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/timer.h"
#include "net/base/network_change_notifier.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "remoting/jingle_glue/xmpp_signal_strategy.h"

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}  // namespace net

namespace remoting {

class DnsBlackholeChecker;

// SignalingConnector listens for SignalStrategy status notifications
// and attempts to keep it connected when possible. When signalling is
// not connected it keeps trying to reconnect it until it is
// connected. It limits connection attempt rate using exponential
// backoff. It also monitors network state and reconnects signalling
// whenever connection type changes or IP address changes.
class SignalingConnector
    : public base::SupportsWeakPtr<SignalingConnector>,
      public base::NonThreadSafe,
      public SignalStrategy::Listener,
      public net::NetworkChangeNotifier::ConnectionTypeObserver,
      public net::NetworkChangeNotifier::IPAddressObserver,
      public gaia::GaiaOAuthClient::Delegate {
 public:
  // This structure contains information required to perform
  // authentication to OAuth2.
  struct OAuthCredentials {
    OAuthCredentials(const std::string& login_value,
                     const std::string& refresh_token_value);

    // The user's account name (i.e. their email address).
    std::string login;

    // Token delegating authority to us to act as the user.
    std::string refresh_token;
  };

  // The |auth_failed_callback| is called when authentication fails.
  SignalingConnector(
      XmppSignalStrategy* signal_strategy,
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
      scoped_ptr<DnsBlackholeChecker> dns_blackhole_checker,
      const base::Closure& auth_failed_callback);
  virtual ~SignalingConnector();

  // May be called immediately after the constructor to enable OAuth
  // access token updating.
  void EnableOAuth(scoped_ptr<OAuthCredentials> oauth_credentials);

  // SignalStrategy::Listener interface.
  virtual void OnSignalStrategyStateChange(
      SignalStrategy::State state) OVERRIDE;
  virtual bool OnSignalStrategyIncomingStanza(
      const buzz::XmlElement* stanza) OVERRIDE;

  // NetworkChangeNotifier::ConnectionTypeObserver interface.
  virtual void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

  // NetworkChangeNotifier::IPAddressObserver interface.
  virtual void OnIPAddressChanged() OVERRIDE;

  // gaia::GaiaOAuthClient::Delegate interface.
  virtual void OnGetTokensResponse(const std::string& user_email,
                                   const std::string& access_token,
                                   int expires_seconds) OVERRIDE;
  virtual void OnRefreshTokenResponse(const std::string& access_token,
                                      int expires_in_seconds) OVERRIDE;
  virtual void OnGetUserInfoResponse(const std::string& user_email) OVERRIDE;
  virtual void OnOAuthError() OVERRIDE;
  virtual void OnNetworkError(int response_code) OVERRIDE;

 private:
  void ScheduleTryReconnect();
  void ResetAndTryReconnect();
  void TryReconnect();
  void OnDnsBlackholeCheckerDone(bool allow);

  void RefreshOAuthToken();

  XmppSignalStrategy* signal_strategy_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  base::Closure auth_failed_callback_;

  scoped_ptr<OAuthCredentials> oauth_credentials_;
  scoped_ptr<gaia::GaiaOAuthClient> gaia_oauth_client_;
  scoped_ptr<DnsBlackholeChecker> dns_blackhole_checker_;

  // Number of times we tried to connect without success.
  int reconnect_attempts_;

  bool refreshing_oauth_token_;
  std::string oauth_access_token_;
  base::Time auth_token_expiry_time_;

  base::OneShotTimer<SignalingConnector> timer_;

  DISALLOW_COPY_AND_ASSIGN(SignalingConnector);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SIGNALING_CONNECTOR_H_
