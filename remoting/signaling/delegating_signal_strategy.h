// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_DELEGATING_SIGNAL_STRATEGY_H_
#define REMOTING_CLIENT_PLUGIN_DELEGATING_SIGNAL_STRATEGY_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "remoting/signaling/signal_strategy.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

// A signaling strategy class that delegates IQ sending and receiving.
//
// Notes on thread safety:
// 1. This object can be created on any thread.
// 2. OnIncomingMessage() must be called on the same thread on which this object
//    is created.
// 3. |send_iq_callback| will always be called on the thread that it is created.
//    Note that |send_iq_callback| may be called after this object is destroyed.
// 4. The caller should invoke all methods on the SignalStrategy interface on
//    the |client_task_runner|.
// 5. All listeners will be called on |client_task_runner| as well.
// 6. The destructor should always be called on the |client_task_runner|.
class DelegatingSignalStrategy : public SignalStrategy {
 public:
  typedef base::Callback<void(const std::string&)> SendIqCallback;

  DelegatingSignalStrategy(
      std::string local_jid,
      scoped_refptr<base::SingleThreadTaskRunner> client_task_runner,
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
  bool SendStanza(std::unique_ptr<buzz::XmlElement> stanza) override;
  std::string GetNextId() override;

 private:
  std::string local_jid_;
  scoped_refptr<base::SingleThreadTaskRunner> delegate_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> client_task_runner_;

  SendIqCallback send_iq_callback_;
  base::ObserverList<Listener> listeners_;

  base::WeakPtrFactory<DelegatingSignalStrategy> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DelegatingSignalStrategy);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_DELEGATING_SIGNAL_STRATEGY_H_
