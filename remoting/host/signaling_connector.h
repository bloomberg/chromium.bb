// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SIGNALING_CONNECTOR_H_
#define REMOTING_HOST_SIGNALING_CONNECTOR_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/timer/timer.h"
#include "net/base/network_change_notifier.h"
#include "remoting/host/oauth_token_getter.h"
#include "remoting/signaling/xmpp_signal_strategy.h"

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
      public net::NetworkChangeNotifier::IPAddressObserver {
 public:
  // The |auth_failed_callback| is called when authentication fails.
  SignalingConnector(
      XmppSignalStrategy* signal_strategy,
      scoped_ptr<DnsBlackholeChecker> dns_blackhole_checker,
      const base::Closure& auth_failed_callback);
  virtual ~SignalingConnector();

  // May be called immediately after the constructor to enable OAuth
  // access token updating.
  // |oauth_token_getter| must outlive SignalingConnector.
  void EnableOAuth(OAuthTokenGetter* oauth_token_getter);

  // OAuthTokenGetter callback.
  void OnAccessToken(OAuthTokenGetter::Status status,
                     const std::string& user_email,
                     const std::string& access_token);

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

 private:
  void OnNetworkError();
  void ScheduleTryReconnect();
  void ResetAndTryReconnect();
  void TryReconnect();
  void OnDnsBlackholeCheckerDone(bool allow);

  XmppSignalStrategy* signal_strategy_;
  base::Closure auth_failed_callback_;
  scoped_ptr<DnsBlackholeChecker> dns_blackhole_checker_;

  OAuthTokenGetter* oauth_token_getter_;

  // Number of times we tried to connect without success.
  int reconnect_attempts_;

  base::OneShotTimer<SignalingConnector> timer_;

  DISALLOW_COPY_AND_ASSIGN(SignalingConnector);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SIGNALING_CONNECTOR_H_
