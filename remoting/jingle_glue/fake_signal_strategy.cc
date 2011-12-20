// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
  while (!pending_messages_.empty()) {
    delete pending_messages_.front();
    pending_messages_.pop();
  }
  DCHECK(listeners_.empty());
}

void FakeSignalStrategy::Init(StatusObserver* observer) {
  observer->OnStateChange(StatusObserver::START);
  observer->OnStateChange(StatusObserver::CONNECTING);
  observer->OnJidChange(jid_);
  observer->OnStateChange(StatusObserver::CONNECTED);
}

void FakeSignalStrategy::Close() {
  DCHECK(CalledOnValidThread());
}

void FakeSignalStrategy::AddListener(Listener* listener) {
  DCHECK(CalledOnValidThread());
  DCHECK(std::find(listeners_.begin(), listeners_.end(), listener) ==
         listeners_.end());
  listeners_.push_back(listener);
}

void FakeSignalStrategy::RemoveListener(Listener* listener) {
  DCHECK(CalledOnValidThread());
  std::vector<Listener*>::iterator it =
      std::find(listeners_.begin(), listeners_.end(), listener);
  CHECK(it != listeners_.end());
  listeners_.erase(it);
}

bool FakeSignalStrategy::SendStanza(buzz::XmlElement* stanza) {
  DCHECK(CalledOnValidThread());

  stanza->SetAttr(buzz::QN_FROM, jid_);

  if (peer_) {
    peer_->OnIncomingMessage(stanza);
    return true;
  } else {
    delete stanza;
    return false;
  }
}

std::string FakeSignalStrategy::GetNextId() {
  ++last_id_;
  return base::IntToString(last_id_);
}

void FakeSignalStrategy::OnIncomingMessage(buzz::XmlElement* stanza) {
  pending_messages_.push(stanza);
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

    for (std::vector<Listener*>::iterator it = listeners_.begin();
         it != listeners_.end(); ++it) {
      if ((*it)->OnIncomingStanza(stanza))
        break;
    }

    pending_messages_.pop();
    delete stanza;
  }
}

}  // namespace remoting
