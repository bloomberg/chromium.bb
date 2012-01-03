// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_FAKE_SIGNAL_STRATEGY_H_
#define REMOTING_JINGLE_GLUE_FAKE_SIGNAL_STRATEGY_H_

#include <queue>
#include <string>

#include "base/observer_list.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "remoting/jingle_glue/iq_sender.h"
#include "remoting/jingle_glue/signal_strategy.h"

namespace remoting {

class FakeSignalStrategy : public SignalStrategy,
                           public base::NonThreadSafe {
 public:
  static void Connect(FakeSignalStrategy* peer1, FakeSignalStrategy* peer2);

  FakeSignalStrategy(const std::string& jid);
  virtual ~FakeSignalStrategy();

  // SignalStrategy interface.
  virtual void Connect() OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual State GetState() const OVERRIDE;
  virtual std::string GetLocalJid() const OVERRIDE;
  virtual void AddListener(Listener* listener) OVERRIDE;
  virtual void RemoveListener(Listener* listener) OVERRIDE;
  virtual bool SendStanza(buzz::XmlElement* stanza) OVERRIDE;
  virtual std::string GetNextId() OVERRIDE;

 private:
  // Called by the |peer_|. Takes ownership of |stanza|.
  void OnIncomingMessage(buzz::XmlElement* stanza);

  void DeliverIncomingMessages();

  std::string jid_;
  FakeSignalStrategy* peer_;
  ObserverList<Listener, true> listeners_;

  int last_id_;

  std::queue<buzz::XmlElement*> pending_messages_;

  base::WeakPtrFactory<FakeSignalStrategy> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeSignalStrategy);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_FAKE_SIGNAL_STRATEGY_H_
