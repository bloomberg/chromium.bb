// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/muxing_signal_strategy.h"

#include <utility>

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "remoting/signaling/ftl_signal_strategy.h"
#include "remoting/signaling/xmpp_signal_strategy.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {

MuxingSignalStrategy::MuxingSignalStrategy(
    FtlSignalStrategy* ftl_signal_strategy,
    XmppSignalStrategy* xmpp_signal_strategy) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  ftl_signal_strategy_ = ftl_signal_strategy;
  xmpp_signal_strategy_ = xmpp_signal_strategy;
  DCHECK(ftl_signal_strategy_);
  DCHECK(xmpp_signal_strategy_);
  ftl_signal_strategy_->AddListener(this);
  xmpp_signal_strategy_->AddListener(this);
}

MuxingSignalStrategy::~MuxingSignalStrategy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ftl_signal_strategy_->RemoveListener(this);
  xmpp_signal_strategy_->RemoveListener(this);
}

const SignalingAddress& MuxingSignalStrategy::GetLocalAddress() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!current_local_address_.empty())
      << "GetLocalAddress() can only be called inside "
      << "OnSignalStrategyIncomingStanza().";
  return current_local_address_;
}

void MuxingSignalStrategy::AddListener(SignalStrategy::Listener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  listeners_.AddObserver(listener);
}

void MuxingSignalStrategy::RemoveListener(SignalStrategy::Listener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  listeners_.RemoveObserver(listener);
}

bool MuxingSignalStrategy::SendStanza(
    std::unique_ptr<jingle_xmpp::XmlElement> stanza) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SignalStrategy* strategy = SignalStrategyForStanza(stanza.get());
  if (!strategy) {
    return false;
  }
  return strategy->SendStanza(std::move(stanza));
}

std::string MuxingSignalStrategy::GetNextId() {
  return base::NumberToString(base::RandUint64());
}

SignalStrategy* MuxingSignalStrategy::SignalStrategyForStanza(
    const jingle_xmpp::XmlElement* stanza) {
  std::string error;
  SignalingAddress receiver =
      SignalingAddress::Parse(stanza, SignalingAddress::TO, &error);
  if (!error.empty()) {
    LOG(DFATAL) << "Failed to parse receiver address: " << error;
    return nullptr;
  }
  if (receiver.channel() == SignalingAddress::Channel::FTL) {
    return ftl_signal_strategy_;
  }
  return xmpp_signal_strategy_;
}

void MuxingSignalStrategy::OnSignalStrategyStateChange(State state) {
  // This is not needed by JingleSessionManager, and forwarding state change
  // from two signal strategies may also cause unexpected behavior, so we just
  // silently drop the call.
}

bool MuxingSignalStrategy::OnSignalStrategyIncomingStanza(
    const jingle_xmpp::XmlElement* stanza) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SignalStrategy* strategy = SignalStrategyForStanza(stanza);
  DCHECK(current_local_address_.empty());
  current_local_address_ = strategy->GetLocalAddress();
  bool message_handled = false;
  for (auto& listener : listeners_) {
    if (listener.OnSignalStrategyIncomingStanza(stanza)) {
      message_handled = true;
      break;
    }
  }
  current_local_address_ = SignalingAddress();
  return message_handled;
}

void MuxingSignalStrategy::Connect() {
  NOTREACHED();
}

void MuxingSignalStrategy::Disconnect() {
  NOTREACHED();
}

SignalStrategy::State MuxingSignalStrategy::GetState() const {
  NOTREACHED();
  return State::DISCONNECTED;
}

SignalStrategy::Error MuxingSignalStrategy::GetError() const {
  NOTREACHED();
  return Error::NETWORK_ERROR;
}

}  // namespace remoting
