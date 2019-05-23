// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_MUXING_SIGNAL_STRATEGY_H_
#define REMOTING_SIGNALING_MUXING_SIGNAL_STRATEGY_H_

#include <memory>

#include "base/macros.h"
#include "remoting/signaling/signal_strategy.h"

namespace remoting {

// WARNING: This class is designed to be used exclusively by
// JingleSessionManager on the host during the XMPP->FTL signaling migration
// process. It doesn't support anything other than sending and receiving
// stanzas. Use (Ftl|Xmpp)SignalStrategy directly if possible.
//
// MuxingSignalStrategy multiplexes FtlSignalStrategy and XmppSignalStrategy.
// It can accept stanzas with FTL or XMPP receiver and forward them to the
// proper SignalStrategy.
//
// Caller can create MuxingSignalStrategy on one thread and thereafter use it
// on another thread.
class MuxingSignalStrategy final : public SignalStrategy {
 public:
  MuxingSignalStrategy(std::unique_ptr<SignalStrategy> ftl_signal_strategy,
                       std::unique_ptr<SignalStrategy> xmpp_signal_strategy);
  ~MuxingSignalStrategy() override;

  // SignalStrategy implementations.

  // This will connect both |ftl_signal_strategy_| and |xmpp_signal_strategy_|.
  void Connect() override;

  // The state is a mapping of the MuxingState (defined in
  // MuxingSignalStrategy::Core):
  //
  // ALL_DISCONNECTED -> DISCONNECTED
  // SOME_CONNECTING, ONLY_ONE_CONNECTED_BEFORE_TIMEOUT -> CONNECTING
  // ALL_CONNECTED, ONLY_ONE_CONNECTED_AFTER_TIMEOUT -> CONNECTED
  //
  // Note that MuxingSignalStrategy will notify listeners whenever the muxing
  // state is changed, which means listeners may get notified for
  // CONNECTING->CONNECTING and CONNECTED->CONNECTED transitions. This is to
  // allow heartbeat sender to send new heartbeat when a strategy is connected
  // or disconnected after the timeout.
  State GetState() const override;

  // GetLocalAddress() can only be called inside
  // OnSignalStrategyIncomingStanza().
  const SignalingAddress& GetLocalAddress() const override;

  void AddListener(SignalStrategy::Listener* listener) override;
  void RemoveListener(SignalStrategy::Listener* listener) override;

  bool SendStanza(std::unique_ptr<jingle_xmpp::XmlElement> stanza) override;
  std::string GetNextId() override;

  SignalStrategy* ftl_signal_strategy();
  SignalStrategy* xmpp_signal_strategy();

 private:
  class Core;

  // These methods are not supported. Caller should directly call them on the
  // underlying signal strategies instead.
  void Disconnect() override;
  Error GetError() const override;

  // Returns pointer to the core, and creates a core if it has not been created.
  //
  // The Core constructor will bind the underlying signal strategies to the
  // current sequence, so we delay construction of the core so that user can
  // create MuxingSignalStrategy on one sequence and use it on another sequence.
  Core* GetCore();
  const Core* GetCore() const;
  Core* GetCoreImpl();

  // These will be moved to |core_| once the core is created.
  std::unique_ptr<SignalStrategy> ftl_signal_strategy_;
  std::unique_ptr<SignalStrategy> xmpp_signal_strategy_;

  std::unique_ptr<Core> core_;
  DISALLOW_COPY_AND_ASSIGN(MuxingSignalStrategy);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_MUXING_SIGNAL_STRATEGY_H_
