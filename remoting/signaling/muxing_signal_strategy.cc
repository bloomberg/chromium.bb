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

namespace {

constexpr base::TimeDelta kWaitForAllStrategiesConnectedTimeout =
    base::TimeDelta::FromSeconds(5);

}  // namespace

MuxingSignalStrategy::MuxingSignalStrategy(
    std::unique_ptr<FtlSignalStrategy> ftl_signal_strategy,
    std::unique_ptr<XmppSignalStrategy> xmpp_signal_strategy) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  ftl_signal_strategy_ = std::move(ftl_signal_strategy);
  xmpp_signal_strategy_ = std::move(xmpp_signal_strategy);
  DCHECK(ftl_signal_strategy_);
  DCHECK(xmpp_signal_strategy_);
  ftl_signal_strategy_->AddListener(this);
  xmpp_signal_strategy_->AddListener(this);

  UpdateTimerState();
  previous_state_ = GetState();
}

MuxingSignalStrategy::~MuxingSignalStrategy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ftl_signal_strategy_->RemoveListener(this);
  xmpp_signal_strategy_->RemoveListener(this);
}

void MuxingSignalStrategy::Connect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ftl_signal_strategy_->Connect();
  xmpp_signal_strategy_->Connect();
}

SignalStrategy::State MuxingSignalStrategy::GetState() const {
  if (IsEveryStrategyDisconnected()) {
    return State::DISCONNECTED;
  }

  if (IsAnyStrategyConnected() &&
      !wait_for_all_strategies_connected_timeout_timer_.IsRunning()) {
    return State::CONNECTED;
  }

  return State::CONNECTING;
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
    return ftl_signal_strategy_.get();
  }
  return xmpp_signal_strategy_.get();
}

void MuxingSignalStrategy::UpdateTimerState() {
  if (IsEveryStrategyConnected() || IsEveryStrategyDisconnected()) {
    wait_for_all_strategies_connected_timeout_timer_.AbandonAndStop();
  } else if (IsAnyStrategyConnected()) {
    wait_for_all_strategies_connected_timeout_timer_.Start(
        FROM_HERE, kWaitForAllStrategiesConnectedTimeout, this,
        &MuxingSignalStrategy::OnWaitForAllStrategiesConnectedTimeout);
  }
}

void MuxingSignalStrategy::OnWaitForAllStrategiesConnectedTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsEveryStrategyConnected()) {
    LOG(WARNING) << "Timeout waiting for all strategies to be connected.";
    OnSignalStrategyStateChange(/* unused */ State::CONNECTED);
  }
}

void MuxingSignalStrategy::OnSignalStrategyStateChange(State unused) {
  UpdateTimerState();
  State new_state = GetState();
  if (previous_state_ != new_state) {
    for (auto& listener : listeners_) {
      listener.OnSignalStrategyStateChange(new_state);
    }
    previous_state_ = new_state;
  }
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

void MuxingSignalStrategy::Disconnect() {
  NOTREACHED();
}

SignalStrategy::Error MuxingSignalStrategy::GetError() const {
  NOTREACHED();
  return Error::NETWORK_ERROR;
}

bool MuxingSignalStrategy::IsAnyStrategyConnected() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return xmpp_signal_strategy_->GetState() == State::CONNECTED ||
         ftl_signal_strategy_->GetState() == State::CONNECTED;
}

bool MuxingSignalStrategy::IsEveryStrategyConnected() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return xmpp_signal_strategy_->GetState() == State::CONNECTED &&
         ftl_signal_strategy_->GetState() == State::CONNECTED;
}

bool MuxingSignalStrategy::IsEveryStrategyDisconnected() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return xmpp_signal_strategy_->GetState() == State::DISCONNECTED &&
         ftl_signal_strategy_->GetState() == State::DISCONNECTED;
}

}  // namespace remoting
