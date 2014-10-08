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
  virtual void Connect() override;
  virtual void Disconnect() override;
  virtual State GetState() const override;
  virtual Error GetError() const override;
  virtual std::string GetLocalJid() const override;
  virtual void AddListener(Listener* listener) override;
  virtual void RemoveListener(Listener* listener) override;
  virtual bool SendStanza(scoped_ptr<buzz::XmlElement> stanza) override;
  virtual std::string GetNextId() override;

 private:
  std::string local_jid_;
  SendIqCallback send_iq_callback_;

  ObserverList<Listener> listeners_;

  DISALLOW_COPY_AND_ASSIGN(DelegatingSignalStrategy);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_DELEGATING_SIGNAL_STRATEGY_H_
