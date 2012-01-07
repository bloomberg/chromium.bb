// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/signaling_connector.h"

#include "base/bind.h"
#include "base/callback.h"

namespace remoting {

namespace {

// The delay between reconnect attempts will increase exponentially up
// to the maximum specified here.
const int kMaxReconnectDelaySeconds = 10 * 60;

}  // namespace

SignalingConnector::SignalingConnector(SignalStrategy* signal_strategy)
    : signal_strategy_(signal_strategy),
      reconnect_attempts_(0) {
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

void SignalingConnector::OnSignalStrategyStateChange(
    SignalStrategy::State state) {
  DCHECK(CalledOnValidThread());

  if (state == SignalStrategy::CONNECTED) {
    LOG(INFO) << "Signaling connected.";
    reconnect_attempts_ = 0;
  } else if (state == SignalStrategy::DISCONNECTED) {
    LOG(INFO) << "Signaling disconnected.";
    reconnect_attempts_++;
    ScheduleTryReconnect();
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

void SignalingConnector::ScheduleTryReconnect() {
  if (timer_.IsRunning() || net::NetworkChangeNotifier::IsOffline())
    return;
  int delay_s = std::min(1 << (reconnect_attempts_ * 2),
                         kMaxReconnectDelaySeconds);
  timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(delay_s),
               this, &SignalingConnector::TryReconnect);
}

void SignalingConnector::ResetAndTryReconnect() {
  signal_strategy_->Disconnect();
  reconnect_attempts_ = 0;
  timer_.Stop();
  ScheduleTryReconnect();
}

void SignalingConnector::TryReconnect() {
  DCHECK(CalledOnValidThread());
  if (signal_strategy_->GetState() == SignalStrategy::DISCONNECTED) {
    LOG(INFO) << "Attempting to connect signaling.";
    signal_strategy_->Connect();
  }
}

}  // namespace remoting
