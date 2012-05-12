// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/fake_signal_strategy.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"

namespace remoting {

// static
void FakeSignalStrategy::Connect(FakeSignalStrategy* peer1,
                                 FakeSignalStrategy* peer2) {
  peer1->peer_ = peer2;
  peer2->peer_ = peer1;
}

FakeSignalStrategy::FakeSignalStrategy(const std::string& jid)
    : jid_(jid),
      peer_(NULL),
      last_id_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {

}

FakeSignalStrategy::~FakeSignalStrategy() {
  while (!received_messages_.empty()) {
    delete received_messages_.front();
    received_messages_.pop_front();
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

  if (peer_) {
    peer_->OnIncomingMessage(stanza.Pass());
    return true;
  } else {
    return false;
  }
}

std::string FakeSignalStrategy::GetNextId() {
  ++last_id_;
  return base::IntToString(last_id_);
}

void FakeSignalStrategy::OnIncomingMessage(
    scoped_ptr<buzz::XmlElement> stanza) {
  pending_messages_.push(stanza.get());
  received_messages_.push_back(stanza.release());
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&FakeSignalStrategy::DeliverIncomingMessages,
                            weak_factory_.GetWeakPtr()));
}

void FakeSignalStrategy::DeliverIncomingMessages() {
  while (!pending_messages_.empty()) {
    buzz::XmlElement* stanza = pending_messages_.front();
    const std::string& to_field = stanza->Attr(buzz::QN_TO);
    if (to_field != jid_) {
      LOG(WARNING) << "Dropping stanza that is addressed to " << to_field
                   << ". Local jid: " << jid_
                   << ". Message content: " << stanza->Str();
      return;
    }

    ObserverListBase<Listener>::Iterator it(listeners_);
    Listener* listener;
    while ((listener = it.GetNext()) != NULL) {
      if (listener->OnSignalStrategyIncomingStanza(stanza))
        break;
    }

    pending_messages_.pop();
  }
}

}  // namespace remoting
