// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_FAKE_SIGNAL_STRATEGY_H_
#define REMOTING_JINGLE_GLUE_FAKE_SIGNAL_STRATEGY_H_

#include <string>

#include "base/threading/non_thread_safe.h"
#include "remoting/jingle_glue/javascript_iq_request.h"
#include "remoting/jingle_glue/signal_strategy.h"

namespace remoting {

class FakeSignalStrategy : public SignalStrategy,
                           public base::NonThreadSafe {
 public:
  static void Connect(FakeSignalStrategy* peer1, FakeSignalStrategy* peer2);

  FakeSignalStrategy(const std::string& jid);
  virtual ~FakeSignalStrategy();

  // SignalStrategy interface.
  virtual void Init(StatusObserver* observer) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void SetListener(Listener* listener) OVERRIDE;
  virtual void SendStanza(buzz::XmlElement* stanza) OVERRIDE;
  virtual std::string GetNextId() OVERRIDE;
  virtual IqRequest* CreateIqRequest() OVERRIDE;

 private:
  // Called by the |peer_|. Takes ownership of |stanza|.
  void OnIncomingMessage(buzz::XmlElement* stanza);

  std::string jid_;
  FakeSignalStrategy* peer_;
  Listener* listener_;
  JavascriptIqRegistry iq_registry_;

  int last_id_;

  DISALLOW_COPY_AND_ASSIGN(FakeSignalStrategy);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_FAKE_SIGNAL_STRATEGY_H_
