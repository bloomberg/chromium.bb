// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SIGNALING_CONNECTOR_H
#define REMOTING_HOST_SIGNALING_CONNECTOR_H

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/timer.h"
#include "net/base/network_change_notifier.h"
#include "remoting/host/gaia_oauth_client.h"
#include "remoting/jingle_glue/xmpp_signal_strategy.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace remoting {

// SignalingConnector listens for SignalStrategy status notifications
// and attempts to keep it connected when possible. When signalling is
// not connected it keeps trying to reconnect it until it is
// connected. It limits connection attempt rate using exponential
// backoff. It also monitors network state and reconnects signalling
// whenever online state changes or IP address changes.
class SignalingConnector
    : public base::SupportsWeakPtr<SignalingConnector>,
      public base::NonThreadSafe,
      public SignalStrategy::Listener,
      public net::NetworkChangeNotifier::OnlineStateObserver,
      public net::NetworkChangeNotifier::IPAddressObserver,
      public GaiaOAuthClient::Delegate {
 public:
  // This structure contains information required to perform
  // authentication to OAuth2.
  struct OAuthCredentials {
    OAuthCredentials(const std::string& login_value,
                     const std::string& refresh_token_value,
                     const OAuthClientInfo& client_info);

    // The user's account name (i.e. their email address).
    std::string login;

    // Token delegating authority to us to act as the user.
    std::string refresh_token;

    // Credentials identifying the application to OAuth.
    OAuthClientInfo client_info;
  };

  // The |auth_failed_callback| is called when authentication fails.
  SignalingConnector(XmppSignalStrategy* signal_strategy,
                     const base::Closure& auth_failed_callback);
  virtual ~SignalingConnector();

  // May be called immediately after the constructor to enable OAuth
  // access token updating.
  void EnableOAuth(scoped_ptr<OAuthCredentials> oauth_credentials,
                   net::URLRequestContextGetter* url_context);

  // SignalStrategy::Listener interface.
  virtual void OnSignalStrategyStateChange(
      SignalStrategy::State state) OVERRIDE;

  // NetworkChangeNotifier::IPAddressObserver interface.
  virtual void OnIPAddressChanged() OVERRIDE;

  // NetworkChangeNotifier::OnlineStateObserver interface.
  virtual void OnOnlineStateChanged(bool online) OVERRIDE;

  // GaiaOAuthClient::Delegate interface.
  virtual void OnRefreshTokenResponse(const std::string& access_token,
                                      int expires_seconds) OVERRIDE;
  virtual void OnOAuthError() OVERRIDE;
  virtual void OnNetworkError(int response_code) OVERRIDE;

 private:
  void ScheduleTryReconnect();
  void ResetAndTryReconnect();
  void TryReconnect();

  void RefreshOAuthToken();

  XmppSignalStrategy* signal_strategy_;
  base::Closure auth_failed_callback_;

  scoped_ptr<OAuthCredentials> oauth_credentials_;
  scoped_ptr<GaiaOAuthClient> gaia_oauth_client_;

  // Number of times we tried to connect without success.
  int reconnect_attempts_;

  bool refreshing_oauth_token_;
  base::Time auth_token_expiry_time_;

  base::OneShotTimer<SignalingConnector> timer_;

  DISALLOW_COPY_AND_ASSIGN(SignalingConnector);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SIGNALING_CONNECTOR_H
