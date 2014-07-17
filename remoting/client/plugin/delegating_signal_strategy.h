// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_DELEGATING_SIGNAL_STRATEGY_H_
#define REMOTING_CLIENT_PLUGIN_DELEGATING_SIGNAL_STRATEGY_H_

#include "base/callback.h"
#include "base/observer_list.h"
#include "remoting/signaling/signal_strategy.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class DelegatingSignalStrategy : public SignalStrategy {
 public:
  typedef base::Callback<void(const std::string&)> SendIqCallback;

  DelegatingSignalStrategy(std::string local_jid,
                       const SendIqCallback& send_iq_callback);
  virtual ~DelegatingSignalStrategy();

  void OnIncomingMessage(const std::string& message);

  // SignalStrategy interface.
  virtual void Connect() OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual State GetState() const OVERRIDE;
  virtual Error GetError() const OVERRIDE;
  virtual std::string GetLocalJid() const OVERRIDE;
  virtual void AddListener(Listener* listener) OVERRIDE;
  virtual void RemoveListener(Listener* listener) OVERRIDE;
  virtual bool SendStanza(scoped_ptr<buzz::XmlElement> stanza) OVERRIDE;
  virtual std::string GetNextId() OVERRIDE;

 private:
  std::string local_jid_;
  SendIqCallback send_iq_callback_;

  ObserverList<Listener> listeners_;

  int last_id_;

  DISALLOW_COPY_AND_ASSIGN(DelegatingSignalStrategy);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_DELEGATING_SIGNAL_STRATEGY_H_
