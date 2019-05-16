// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/muxing_signal_strategy.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/timer/timer.h"
#include "remoting/signaling/ftl_signal_strategy.h"
#include "remoting/signaling/signaling_address.h"
#include "remoting/signaling/xmpp_signal_strategy.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {

namespace {

constexpr base::TimeDelta kWaitForAllStrategiesConnectedTimeout =
    base::TimeDelta::FromSeconds(5);

}  // namespace

class MuxingSignalStrategy::Core final : public SignalStrategy::Listener {
 public:
  Core(std::unique_ptr<FtlSignalStrategy> ftl_signal_strategy,
       std::unique_ptr<XmppSignalStrategy> xmpp_signal_strategy);
  ~Core() override;

  void Invalidate();

  void Connect();
  State GetState() const;
  const SignalingAddress& GetLocalAddress() const;
  void AddListener(SignalStrategy::Listener* listener);
  void RemoveListener(SignalStrategy::Listener* listener);
  bool SendStanza(std::unique_ptr<jingle_xmpp::XmlElement> stanza);

  FtlSignalStrategy* ftl_signal_strategy() {
    return ftl_signal_strategy_.get();
  }

  XmppSignalStrategy* xmpp_signal_strategy() {
    return xmpp_signal_strategy_.get();
  }

 private:
  SignalStrategy* GetSignalStrategyForStanza(
      const jingle_xmpp::XmlElement* stanza);
  void UpdateTimerState();

  void OnWaitForAllStrategiesConnectedTimeout();

  // SignalStrategy::Listener implementations.
  void OnSignalStrategyStateChange(SignalStrategy::State state) override;
  bool OnSignalStrategyIncomingStanza(
      const jingle_xmpp::XmlElement* stanza) override;

  bool IsAnyStrategyConnected() const;
  bool IsEveryStrategyConnected() const;
  bool IsEveryStrategyDisconnected() const;

  base::ObserverList<SignalStrategy::Listener> listeners_;

  std::unique_ptr<FtlSignalStrategy> ftl_signal_strategy_;
  std::unique_ptr<XmppSignalStrategy> xmpp_signal_strategy_;

  SignalingAddress current_local_address_;
  State previous_state_;

  base::OneShotTimer wait_for_all_strategies_connected_timeout_timer_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<Core> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(Core);
};

MuxingSignalStrategy::Core::Core(
    std::unique_ptr<FtlSignalStrategy> ftl_signal_strategy,
    std::unique_ptr<XmppSignalStrategy> xmpp_signal_strategy)
    : weak_factory_(this) {
  ftl_signal_strategy_ = std::move(ftl_signal_strategy);
  xmpp_signal_strategy_ = std::move(xmpp_signal_strategy);
  DCHECK(ftl_signal_strategy_);
  DCHECK(xmpp_signal_strategy_);
  ftl_signal_strategy_->AddListener(this);
  xmpp_signal_strategy_->AddListener(this);

  UpdateTimerState();
  previous_state_ = GetState();
}

MuxingSignalStrategy::Core::~Core() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!ftl_signal_strategy_);
  DCHECK(!xmpp_signal_strategy_);
}

void MuxingSignalStrategy::Core::Invalidate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ftl_signal_strategy_->RemoveListener(this);
  xmpp_signal_strategy_->RemoveListener(this);
  ftl_signal_strategy_.reset();
  xmpp_signal_strategy_.reset();
}

void MuxingSignalStrategy::Core::Connect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ftl_signal_strategy_->Connect();
  xmpp_signal_strategy_->Connect();
}

SignalStrategy::State MuxingSignalStrategy::Core::GetState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsEveryStrategyDisconnected()) {
    return State::DISCONNECTED;
  }

  if (IsAnyStrategyConnected() &&
      !wait_for_all_strategies_connected_timeout_timer_.IsRunning()) {
    return State::CONNECTED;
  }

  return State::CONNECTING;
}

const SignalingAddress& MuxingSignalStrategy::Core::GetLocalAddress() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!current_local_address_.empty())
      << "GetLocalAddress() can only be called inside "
      << "OnSignalStrategyIncomingStanza().";
  return current_local_address_;
}

void MuxingSignalStrategy::Core::AddListener(
    SignalStrategy::Listener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  listeners_.AddObserver(listener);
}

void MuxingSignalStrategy::Core::RemoveListener(
    SignalStrategy::Listener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  listeners_.RemoveObserver(listener);
}

bool MuxingSignalStrategy::Core::SendStanza(
    std::unique_ptr<jingle_xmpp::XmlElement> stanza) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SignalStrategy* strategy = GetSignalStrategyForStanza(stanza.get());
  if (!strategy) {
    return false;
  }
  return strategy->SendStanza(std::move(stanza));
}

SignalStrategy* MuxingSignalStrategy::Core::GetSignalStrategyForStanza(
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

void MuxingSignalStrategy::Core::UpdateTimerState() {
  if (IsEveryStrategyConnected() || IsEveryStrategyDisconnected()) {
    wait_for_all_strategies_connected_timeout_timer_.AbandonAndStop();
  } else if (IsAnyStrategyConnected()) {
    wait_for_all_strategies_connected_timeout_timer_.Start(
        FROM_HERE, kWaitForAllStrategiesConnectedTimeout, this,
        &MuxingSignalStrategy::Core::OnWaitForAllStrategiesConnectedTimeout);
  }
}

void MuxingSignalStrategy::Core::OnWaitForAllStrategiesConnectedTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsEveryStrategyConnected()) {
    LOG(WARNING) << "Timeout waiting for all strategies to be connected.";
    OnSignalStrategyStateChange(/* unused */ State::CONNECTED);
  }
}

void MuxingSignalStrategy::Core::OnSignalStrategyStateChange(
    SignalStrategy::State unused) {
  UpdateTimerState();
  State new_state = GetState();
  if (previous_state_ != new_state) {
    for (auto& listener : listeners_) {
      listener.OnSignalStrategyStateChange(new_state);
    }
    previous_state_ = new_state;
  }
}

bool MuxingSignalStrategy::Core::OnSignalStrategyIncomingStanza(
    const jingle_xmpp::XmlElement* stanza) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SignalStrategy* strategy = GetSignalStrategyForStanza(stanza);
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

bool MuxingSignalStrategy::Core::IsAnyStrategyConnected() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return xmpp_signal_strategy_->GetState() == State::CONNECTED ||
         ftl_signal_strategy_->GetState() == State::CONNECTED;
}

bool MuxingSignalStrategy::Core::IsEveryStrategyConnected() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return xmpp_signal_strategy_->GetState() == State::CONNECTED &&
         ftl_signal_strategy_->GetState() == State::CONNECTED;
}

bool MuxingSignalStrategy::Core::IsEveryStrategyDisconnected() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return xmpp_signal_strategy_->GetState() == State::DISCONNECTED &&
         ftl_signal_strategy_->GetState() == State::DISCONNECTED;
}

MuxingSignalStrategy::MuxingSignalStrategy(
    std::unique_ptr<FtlSignalStrategy> ftl_signal_strategy,
    std::unique_ptr<XmppSignalStrategy> xmpp_signal_strategy)
    : ftl_signal_strategy_(std::move(ftl_signal_strategy)),
      xmpp_signal_strategy_(std::move(xmpp_signal_strategy)) {}

MuxingSignalStrategy::~MuxingSignalStrategy() {
  if (!core_) {
    // The core has never been created. Just do nothing.
    return;
  }
  GetCore()->Invalidate();
  base::SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                     std::move(core_));
}

void MuxingSignalStrategy::Connect() {
  GetCore()->Connect();
}

SignalStrategy::State MuxingSignalStrategy::GetState() const {
  return GetCore()->GetState();
}

const SignalingAddress& MuxingSignalStrategy::GetLocalAddress() const {
  return GetCore()->GetLocalAddress();
}

void MuxingSignalStrategy::AddListener(SignalStrategy::Listener* listener) {
  GetCore()->AddListener(listener);
}

void MuxingSignalStrategy::RemoveListener(SignalStrategy::Listener* listener) {
  GetCore()->RemoveListener(listener);
}

bool MuxingSignalStrategy::SendStanza(
    std::unique_ptr<jingle_xmpp::XmlElement> stanza) {
  return GetCore()->SendStanza(std::move(stanza));
}

std::string MuxingSignalStrategy::GetNextId() {
  return base::NumberToString(base::RandUint64());
}

FtlSignalStrategy* MuxingSignalStrategy::ftl_signal_strategy() {
  return GetCore()->ftl_signal_strategy();
}

XmppSignalStrategy* MuxingSignalStrategy::xmpp_signal_strategy() {
  return GetCore()->xmpp_signal_strategy();
}

void MuxingSignalStrategy::Disconnect() {
  NOTREACHED();
}

SignalStrategy::Error MuxingSignalStrategy::GetError() const {
  NOTREACHED();
  return Error::NETWORK_ERROR;
}

MuxingSignalStrategy::Core* MuxingSignalStrategy::GetCore() {
  return GetCoreImpl();
}

const MuxingSignalStrategy::Core* MuxingSignalStrategy::GetCore() const {
  return const_cast<MuxingSignalStrategy*>(this)->GetCoreImpl();
}

MuxingSignalStrategy::Core* MuxingSignalStrategy::GetCoreImpl() {
  if (!core_) {
    core_ = std::make_unique<Core>(std::move(ftl_signal_strategy_),
                                   std::move(xmpp_signal_strategy_));
  }
  return core_.get();
}

}  // namespace remoting
