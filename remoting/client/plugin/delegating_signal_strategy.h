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
  ~DelegatingSignalStrategy() override;

  void OnIncomingMessage(const std::string& message);

  // SignalStrategy interface.
  void Connect() override;
  void Disconnect() override;
  State GetState() const override;
  Error GetError() const override;
  std::string GetLocalJid() const override;
  void AddListener(Listener* listener) override;
  void RemoveListener(Listener* listener) override;
  bool SendStanza(scoped_ptr<buzz::XmlElement> stanza) override;
  std::string GetNextId() override;

 private:
  std::string local_jid_;
  SendIqCallback send_iq_callback_;

  base::ObserverList<Listener> listeners_;

  DISALLOW_COPY_AND_ASSIGN(DelegatingSignalStrategy);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_DELEGATING_SIGNAL_STRATEGY_H_
