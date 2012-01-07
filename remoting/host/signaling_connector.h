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
#include "remoting/jingle_glue/signal_strategy.h"

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
      public net::NetworkChangeNotifier::IPAddressObserver {
 public:
  SignalingConnector(SignalStrategy* signal_strategy);
  virtual ~SignalingConnector();

  // SignalStrategy::Listener interface.
  virtual void OnSignalStrategyStateChange(
      SignalStrategy::State state) OVERRIDE;

  // NetworkChangeNotifier::IPAddressObserver interface.
  virtual void OnIPAddressChanged() OVERRIDE;

  // NetworkChangeNotifier::OnlineStateObserver interface.
  virtual void OnOnlineStateChanged(bool online) OVERRIDE;

 private:
  void ScheduleTryReconnect();
  void ResetAndTryReconnect();
  void TryReconnect();

  SignalStrategy* signal_strategy_;

  // Number of times we tried to connect without success.
  int reconnect_attempts_;

  base::OneShotTimer<SignalingConnector> timer_;

  DISALLOW_COPY_AND_ASSIGN(SignalingConnector);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SIGNALING_CONNECTOR_H
