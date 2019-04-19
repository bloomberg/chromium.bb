// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_MUXING_SIGNAL_STRATEGY_H_
#define REMOTING_SIGNALING_MUXING_SIGNAL_STRATEGY_H_

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
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
  MuxingSignalStrategy(FtlSignalStrategy* ftl_signal_strategy,
                       XmppSignalStrategy* xmpp_signal_strategy);
  ~MuxingSignalStrategy() override;

  // SignalStrategy implementations.

  // GetLocalAddress() can only be called inside
  // OnSignalStrategyIncomingStanza().
  const SignalingAddress& GetLocalAddress() const override;

  // Note that OnSignalStrategyStateChange() WON'T be called on the listener.
  void AddListener(SignalStrategy::Listener* listener) override;
  void RemoveListener(SignalStrategy::Listener* listener) override;

  bool SendStanza(std::unique_ptr<jingle_xmpp::XmlElement> stanza) override;
  std::string GetNextId() override;

 private:
  SignalStrategy* SignalStrategyForStanza(
      const jingle_xmpp::XmlElement* stanza);

  // These methods are not supported. Caller should directly call them on the
  // underlying signal strategies instead.
  void Connect() override;
  void Disconnect() override;
  State GetState() const override;
  Error GetError() const override;

  // SignalStrategy::Listener implementations.
  void OnSignalStrategyStateChange(State state) override;
  bool OnSignalStrategyIncomingStanza(
      const jingle_xmpp::XmlElement* stanza) override;

  base::ObserverList<SignalStrategy::Listener> listeners_;

  FtlSignalStrategy* ftl_signal_strategy_ = nullptr;
  XmppSignalStrategy* xmpp_signal_strategy_ = nullptr;

  SignalingAddress current_local_address_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(MuxingSignalStrategy);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_MUXING_SIGNAL_STRATEGY_H_
