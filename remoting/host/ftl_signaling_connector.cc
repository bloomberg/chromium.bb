// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ftl_signaling_connector.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string_util.h"
#include "google_apis/google_api_keys.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/logging.h"
#include "remoting/signaling/signaling_address.h"

namespace remoting {

namespace {

const char* SignalStrategyErrorToString(SignalStrategy::Error error) {
  switch (error) {
    case SignalStrategy::OK:
      return "OK";
    case SignalStrategy::AUTHENTICATION_FAILED:
      return "AUTHENTICATION_FAILED";
    case SignalStrategy::NETWORK_ERROR:
      return "NETWORK_ERROR";
    case SignalStrategy::PROTOCOL_ERROR:
      return "PROTOCOL_ERROR";
  }
  return "";
}

}  // namespace

FtlSignalingConnector::FtlSignalingConnector(
    FtlSignalStrategy* signal_strategy,
    base::OnceClosure auth_failed_callback)
    : signal_strategy_(signal_strategy),
      auth_failed_callback_(std::move(auth_failed_callback)) {
  DCHECK(signal_strategy_);
  DCHECK(auth_failed_callback_);
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
  signal_strategy_->AddListener(this);
}

FtlSignalingConnector::~FtlSignalingConnector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  signal_strategy_->RemoveListener(this);
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void FtlSignalingConnector::Start() {
  TryReconnect();
}

void FtlSignalingConnector::OnSignalStrategyStateChange(
    SignalStrategy::State state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state == SignalStrategy::CONNECTED) {
    HOST_LOG << "Signaling connected. New JID: "
             << signal_strategy_->GetLocalAddress().jid();
  } else if (state == SignalStrategy::DISCONNECTED) {
    HOST_LOG << "Signaling disconnected. error="
             << SignalStrategyErrorToString(signal_strategy_->GetError());
    if (signal_strategy_->IsSignInError() &&
        signal_strategy_->GetError() == SignalStrategy::AUTHENTICATION_FAILED) {
      std::move(auth_failed_callback_).Run();
      return;
    }
    TryReconnect();
  }
}

bool FtlSignalingConnector::OnSignalStrategyIncomingStanza(
    const jingle_xmpp::XmlElement* stanza) {
  return false;
}

void FtlSignalingConnector::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (type != net::NetworkChangeNotifier::CONNECTION_NONE &&
      signal_strategy_->GetState() == SignalStrategy::DISCONNECTED) {
    HOST_LOG << "Network state changed to online.";
    TryReconnect();
  }
}

void FtlSignalingConnector::TryReconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timer_.Start(FROM_HERE, base::TimeDelta(), this,
               &FtlSignalingConnector::DoReconnect);
}

void FtlSignalingConnector::DoReconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (signal_strategy_->GetState() == SignalStrategy::DISCONNECTED) {
    HOST_LOG << "Attempting to reconnect signaling.";
    signal_strategy_->Connect();
  }
}

}  // namespace remoting
