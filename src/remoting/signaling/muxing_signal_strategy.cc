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
#include "remoting/signaling/signaling_address.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {

namespace {

constexpr base::TimeDelta kWaitForAllStrategiesConnectedTimeout =
    base::TimeDelta::FromSeconds(5);

}  // namespace

class MuxingSignalStrategy::Core final : public SignalStrategy::Listener {
 public:
  Core(std::unique_ptr<SignalStrategy> ftl_signal_strategy,
       std::unique_ptr<SignalStrategy> xmpp_signal_strategy);
  ~Core() override;

  void Invalidate();

  void Connect();
  State GetState() const;
  const SignalingAddress& GetLocalAddress() const;
  void AddListener(SignalStrategy::Listener* listener);
  void RemoveListener(SignalStrategy::Listener* listener);
  bool SendStanza(std::unique_ptr<jingle_xmpp::XmlElement> stanza);

  SignalStrategy* ftl_signal_strategy() { return ftl_signal_strategy_.get(); }

  SignalStrategy* xmpp_signal_strategy() { return xmpp_signal_strategy_.get(); }

 private:
  enum class MuxingState {
    ALL_DISCONNECTED,
    SOME_CONNECTING,
    ONLY_ONE_CONNECTED_BEFORE_TIMEOUT,
    ALL_CONNECTED,
    ONLY_ONE_CONNECTED_AFTER_TIMEOUT,
  };

  SignalStrategy* GetSignalStrategyForStanza(
      const jingle_xmpp::XmlElement* stanza);

  // Returns true if the state is updated.
  bool UpdateState();

  void OnWaitForAllStrategiesConnectedTimeout();

  // SignalStrategy::Listener implementations.
  void OnSignalStrategyStateChange(SignalStrategy::State state) override;
  bool OnSignalStrategyIncomingStanza(
      const jingle_xmpp::XmlElement* stanza) override;

  bool IsAnyStrategyConnected() const;
  bool IsEveryStrategyConnected() const;
  bool IsEveryStrategyDisconnected() const;

  base::ObserverList<SignalStrategy::Listener> listeners_;

  std::unique_ptr<SignalStrategy> ftl_signal_strategy_;
  std::unique_ptr<SignalStrategy> xmpp_signal_strategy_;

  SignalingAddress current_local_address_;
  MuxingState state_ = MuxingState::ALL_DISCONNECTED;

  base::OneShotTimer wait_for_all_strategies_connected_timeout_timer_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<Core> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(Core);
};

MuxingSignalStrategy::Core::Core(
    std::unique_ptr<SignalStrategy> ftl_signal_strategy,
    std::unique_ptr<SignalStrategy> xmpp_signal_strategy)
    : weak_factory_(this) {
  ftl_signal_strategy_ = std::move(ftl_signal_strategy);
  xmpp_signal_strategy_ = std::move(xmpp_signal_strategy);
  DCHECK(ftl_signal_strategy_);
  DCHECK(xmpp_signal_strategy_);
  DCHECK_EQ(State::DISCONNECTED, ftl_signal_strategy_->GetState());
  DCHECK_EQ(State::DISCONNECTED, xmpp_signal_strategy_->GetState());
  ftl_signal_strategy_->AddListener(this);
  xmpp_signal_strategy_->AddListener(this);

  UpdateState();
}

MuxingSignalStrategy::Core::~Core() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!ftl_signal_strategy_);
  DCHECK(!xmpp_signal_strategy_);
}

void MuxingSignalStrategy::Core::Invalidate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  wait_for_all_strategies_connected_timeout_timer_.AbandonAndStop();
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
  switch (state_) {
    case MuxingState::ALL_DISCONNECTED:
      return State::DISCONNECTED;
    case MuxingState::SOME_CONNECTING:
    case MuxingState::ONLY_ONE_CONNECTED_BEFORE_TIMEOUT:
      return State::CONNECTING;
    case MuxingState::ONLY_ONE_CONNECTED_AFTER_TIMEOUT:
    case MuxingState::ALL_CONNECTED:
      return State::CONNECTED;
    default:
      NOTREACHED();
      return State::DISCONNECTED;
  }
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
    DCHECK(ftl_signal_strategy_->GetLocalAddress().empty() ||
           ftl_signal_strategy_->GetLocalAddress().channel() ==
               SignalingAddress::Channel::FTL)
        << "|ftl_signal_strategy_|'s local address channel is not FTL. "
        << "You might have flipped the signal strategies. "
        << "Local address: " << ftl_signal_strategy_->GetLocalAddress().jid();
    return ftl_signal_strategy_.get();
  } else {
    DCHECK(xmpp_signal_strategy_->GetLocalAddress().empty() ||
           xmpp_signal_strategy_->GetLocalAddress().channel() !=
               SignalingAddress::Channel::FTL)
        << "|xmpp_signal_strategy_|'s local address channel is FTL. "
        << "You might have flipped the signal strategies. "
        << "Local address: " << xmpp_signal_strategy_->GetLocalAddress().jid();
  }
  return xmpp_signal_strategy_.get();
}

bool MuxingSignalStrategy::Core::UpdateState() {
  MuxingState new_state = state_;
  if (IsEveryStrategyConnected()) {
    wait_for_all_strategies_connected_timeout_timer_.AbandonAndStop();
    new_state = MuxingState::ALL_CONNECTED;
  } else if (IsEveryStrategyDisconnected()) {
    wait_for_all_strategies_connected_timeout_timer_.AbandonAndStop();
    new_state = MuxingState::ALL_DISCONNECTED;
  } else if (IsAnyStrategyConnected()) {
    if (state_ == MuxingState::ALL_CONNECTED  // One connection is dropped
        || (state_ == MuxingState::ONLY_ONE_CONNECTED_BEFORE_TIMEOUT &&
            !wait_for_all_strategies_connected_timeout_timer_.IsRunning())) {
      new_state = MuxingState::ONLY_ONE_CONNECTED_AFTER_TIMEOUT;
    } else if (state_ != MuxingState::ONLY_ONE_CONNECTED_AFTER_TIMEOUT) {
      new_state = MuxingState::ONLY_ONE_CONNECTED_BEFORE_TIMEOUT;
      if (!wait_for_all_strategies_connected_timeout_timer_.IsRunning()) {
        wait_for_all_strategies_connected_timeout_timer_.Start(
            FROM_HERE, kWaitForAllStrategiesConnectedTimeout, this,
            &MuxingSignalStrategy::Core::
                OnWaitForAllStrategiesConnectedTimeout);
      }
    }
    // Otherwise we are not changing the state unless all strategies are
    // connected or all strategies are disconnected.
  } else {
    new_state = MuxingState::SOME_CONNECTING;
  }
  if (state_ == new_state) {
    return false;
  }
  state_ = new_state;
  return true;
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
  bool is_state_changed = UpdateState();
  if (is_state_changed) {
    for (auto& listener : listeners_) {
      listener.OnSignalStrategyStateChange(GetState());
    }
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
    std::unique_ptr<SignalStrategy> ftl_signal_strategy,
    std::unique_ptr<SignalStrategy> xmpp_signal_strategy)
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

SignalStrategy* MuxingSignalStrategy::ftl_signal_strategy() {
  return GetCore()->ftl_signal_strategy();
}

SignalStrategy* MuxingSignalStrategy::xmpp_signal_strategy() {
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
