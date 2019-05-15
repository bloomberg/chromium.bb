// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_MUXING_SIGNAL_STRATEGY_H_
#define REMOTING_SIGNALING_MUXING_SIGNAL_STRATEGY_H_

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "remoting/signaling/signal_strategy.h"
#include "remoting/signaling/signaling_address.h"

namespace remoting {

class FtlSignalStrategy;
class XmppSignalStrategy;

// WARNING: This class is designed to be used exclusively by
// JingleSessionManager on the host during the XMPP->FTL signaling migration
// process. It doesn't support anything other than sending and receiving
// stanzas. Use (Ftl|Xmpp)SignalStrategy directly if possible.
//
// MuxingSignalStrategy multiplexes FtlSignalStrategy and XmppSignalStrategy.
// It can accept stanzas with FTL or XMPP receiver and forward them to the
// proper SignalStrategy.
class MuxingSignalStrategy final : public SignalStrategy,
                                   public SignalStrategy::Listener {
 public:
  // Raw pointers must outlive |this|.
  MuxingSignalStrategy(
      std::unique_ptr<FtlSignalStrategy> ftl_signal_strategy,
      std::unique_ptr<XmppSignalStrategy> xmpp_signal_strategy);
  ~MuxingSignalStrategy() override;

  // SignalStrategy implementations.

  // This will connect both |ftl_signal_strategy_| and |xmpp_signal_strategy_|.
  void Connect() override;

  // Returns:
  // * DISCONNECTED if both of the signal strategies are disconnected
  // * CONNECTED if both of the signal strategies are connected, or only one of
  //   the strategy is connected while a timeout has been elapsed (the other
  //   strategy can be either disconnected or connecting)
  // * CONNECTING in other cases
  State GetState() const override;

  // GetLocalAddress() can only be called inside
  // OnSignalStrategyIncomingStanza().
  const SignalingAddress& GetLocalAddress() const override;

  void AddListener(SignalStrategy::Listener* listener) override;
  void RemoveListener(SignalStrategy::Listener* listener) override;

  bool SendStanza(std::unique_ptr<jingle_xmpp::XmlElement> stanza) override;
  std::string GetNextId() override;

  FtlSignalStrategy* ftl_signal_strategy() {
    return ftl_signal_strategy_.get();
  }

  XmppSignalStrategy* xmpp_signal_strategy() {
    return xmpp_signal_strategy_.get();
  }

 private:
  SignalStrategy* SignalStrategyForStanza(
      const jingle_xmpp::XmlElement* stanza);
  void UpdateTimerState();

  void OnWaitForAllStrategiesConnectedTimeout();

  // These methods are not supported. Caller should directly call them on the
  // underlying signal strategies instead.
  void Disconnect() override;
  Error GetError() const override;

  // SignalStrategy::Listener implementations.
  void OnSignalStrategyStateChange(State state) override;
  bool OnSignalStrategyIncomingStanza(
      const jingle_xmpp::XmlElement* stanza) override;

  bool IsAnyStrategyConnected() const;
  bool IsEveryStrategyConnected() const;
  bool IsEveryStrategyDisconnected() const;

  base::ObserverList<SignalStrategy::Listener> listeners_;

  std::unique_ptr<FtlSignalStrategy> ftl_signal_strategy_ = nullptr;
  std::unique_ptr<XmppSignalStrategy> xmpp_signal_strategy_ = nullptr;

  SignalingAddress current_local_address_;
  State previous_state_;

  base::OneShotTimer wait_for_all_strategies_connected_timeout_timer_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(MuxingSignalStrategy);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_MUXING_SIGNAL_STRATEGY_H_
