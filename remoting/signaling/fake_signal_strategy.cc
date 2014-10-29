// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/fake_signal_strategy.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"
#include "third_party/webrtc/libjingle/xmllite/xmlelement.h"

namespace remoting {

// static
void FakeSignalStrategy::Connect(FakeSignalStrategy* peer1,
                                 FakeSignalStrategy* peer2) {
  DCHECK(peer1->main_thread_->BelongsToCurrentThread());
  DCHECK(peer2->main_thread_->BelongsToCurrentThread());
  peer1->ConnectTo(peer2);
  peer2->ConnectTo(peer1);
}

FakeSignalStrategy::FakeSignalStrategy(const std::string& jid)
    : main_thread_(base::ThreadTaskRunnerHandle::Get()),
      jid_(jid),
      last_id_(0),
      weak_factory_(this) {

}

FakeSignalStrategy::~FakeSignalStrategy() {
  while (!received_messages_.empty()) {
    delete received_messages_.front();
    received_messages_.pop_front();
  }
}

void FakeSignalStrategy::ConnectTo(FakeSignalStrategy* peer) {
  PeerCallback peer_callback =
      base::Bind(&FakeSignalStrategy::DeliverMessageOnThread,
                 main_thread_,
                 weak_factory_.GetWeakPtr());
  if (peer->main_thread_->BelongsToCurrentThread()) {
    peer->SetPeerCallback(peer_callback);
  } else {
    peer->main_thread_->PostTask(
        FROM_HERE,
        base::Bind(&FakeSignalStrategy::SetPeerCallback,
                   base::Unretained(peer),
                   peer_callback));
  }
}

void FakeSignalStrategy::Connect() {
  DCHECK(CalledOnValidThread());
  FOR_EACH_OBSERVER(Listener, listeners_,
                    OnSignalStrategyStateChange(CONNECTED));
}

void FakeSignalStrategy::Disconnect() {
  DCHECK(CalledOnValidThread());
  FOR_EACH_OBSERVER(Listener, listeners_,
                    OnSignalStrategyStateChange(DISCONNECTED));
}

SignalStrategy::State FakeSignalStrategy::GetState() const {
  return CONNECTED;
}

SignalStrategy::Error FakeSignalStrategy::GetError() const {
  return OK;
}

std::string FakeSignalStrategy::GetLocalJid() const {
  DCHECK(CalledOnValidThread());
  return jid_;
}

void FakeSignalStrategy::AddListener(Listener* listener) {
  DCHECK(CalledOnValidThread());
  listeners_.AddObserver(listener);
}

void FakeSignalStrategy::RemoveListener(Listener* listener) {
  DCHECK(CalledOnValidThread());
  listeners_.RemoveObserver(listener);
}

bool FakeSignalStrategy::SendStanza(scoped_ptr<buzz::XmlElement> stanza) {
  DCHECK(CalledOnValidThread());

  stanza->SetAttr(buzz::QN_FROM, jid_);

  if (!peer_callback_.is_null()) {
    peer_callback_.Run(stanza.Pass());
    return true;
  } else {
    return false;
  }
}

std::string FakeSignalStrategy::GetNextId() {
  ++last_id_;
  return base::IntToString(last_id_);
}

// static
void FakeSignalStrategy::DeliverMessageOnThread(
    scoped_refptr<base::SingleThreadTaskRunner> thread,
    base::WeakPtr<FakeSignalStrategy> target,
    scoped_ptr<buzz::XmlElement> stanza) {
  thread->PostTask(FROM_HERE,
                   base::Bind(&FakeSignalStrategy::OnIncomingMessage,
                              target, base::Passed(&stanza)));
}

void FakeSignalStrategy::OnIncomingMessage(
    scoped_ptr<buzz::XmlElement> stanza) {
  DCHECK(CalledOnValidThread());

  buzz::XmlElement* stanza_ptr = stanza.get();
  received_messages_.push_back(stanza.release());

  const std::string& to_field = stanza_ptr->Attr(buzz::QN_TO);
  if (to_field != jid_) {
    LOG(WARNING) << "Dropping stanza that is addressed to " << to_field
                 << ". Local jid: " << jid_
                 << ". Message content: " << stanza_ptr->Str();
    return;
  }

  ObserverListBase<Listener>::Iterator it(listeners_);
  Listener* listener;
  while ((listener = it.GetNext()) != NULL) {
    if (listener->OnSignalStrategyIncomingStanza(stanza_ptr))
      break;
  }
}

void FakeSignalStrategy::SetPeerCallback(const PeerCallback& peer_callback) {
  peer_callback_ = peer_callback;
}

}  // namespace remoting
