// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_FAKE_SIGNAL_STRATEGY_H_
#define REMOTING_SIGNALING_FAKE_SIGNAL_STRATEGY_H_

#include <list>
#include <queue>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/non_thread_safe.h"
#include "remoting/signaling/iq_sender.h"
#include "remoting/signaling/signal_strategy.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class FakeSignalStrategy : public SignalStrategy,
                           public base::NonThreadSafe {
 public:
  // Calls ConenctTo() to connect |peer1| and |peer2|. Both |peer1| and |peer2|
  // must belong to the current thread.
  static void Connect(FakeSignalStrategy* peer1, FakeSignalStrategy* peer2);

  FakeSignalStrategy(const std::string& jid);
  virtual ~FakeSignalStrategy();

  const std::list<buzz::XmlElement*>& received_messages() {
    return received_messages_;
  }

  // Connects current FakeSignalStrategy to receive messages from |peer|.
  void ConnectTo(FakeSignalStrategy* peer);

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
  typedef base::Callback<void(scoped_ptr<buzz::XmlElement> message)>
      PeerCallback;

  static void DeliverMessageOnThread(
      scoped_refptr<base::SingleThreadTaskRunner> thread,
      base::WeakPtr<FakeSignalStrategy> target,
      scoped_ptr<buzz::XmlElement> stanza);

  // Called by the |peer_|. Takes ownership of |stanza|.
  void OnIncomingMessage(scoped_ptr<buzz::XmlElement> stanza);
  void SetPeerCallback(const PeerCallback& peer_callback);

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_;

  std::string jid_;
  PeerCallback peer_callback_;
  ObserverList<Listener, true> listeners_;

  int last_id_;

  // All received messages, includes thouse still in |pending_messages_|.
  std::list<buzz::XmlElement*> received_messages_;

  base::WeakPtrFactory<FakeSignalStrategy> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeSignalStrategy);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_FAKE_SIGNAL_STRATEGY_H_
