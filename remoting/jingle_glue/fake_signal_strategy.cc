// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/fake_signal_strategy.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
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
      listener_(NULL),
      last_id_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)) {

}

FakeSignalStrategy::~FakeSignalStrategy() {
  while (!pending_messages_.empty()) {
    delete pending_messages_.front();
    pending_messages_.pop();
  }
}

void FakeSignalStrategy::Init(StatusObserver* observer) {
  observer->OnStateChange(StatusObserver::START);
  observer->OnStateChange(StatusObserver::CONNECTING);
  observer->OnJidChange(jid_);
  observer->OnStateChange(StatusObserver::CONNECTED);
}

void FakeSignalStrategy::Close() {
  DCHECK(CalledOnValidThread());
  listener_ = NULL;
}

void FakeSignalStrategy::SetListener(Listener* listener) {
  DCHECK(CalledOnValidThread());

  // Don't overwrite an listener without explicitly going
  // through "NULL" first.
  DCHECK(listener_ == NULL || listener == NULL);
  listener_ = listener;
}

void FakeSignalStrategy::SendStanza(buzz::XmlElement* stanza) {
  DCHECK(CalledOnValidThread());

  stanza->SetAttr(buzz::QN_FROM, jid_);

  if (peer_) {
    peer_->OnIncomingMessage(stanza);
  } else {
    delete stanza;
  }
}

std::string FakeSignalStrategy::GetNextId() {
  ++last_id_;
  return base::IntToString(last_id_);
}

IqRequest* FakeSignalStrategy::CreateIqRequest() {
  DCHECK(CalledOnValidThread());

  return new JavascriptIqRequest(this, &iq_registry_);
}

void FakeSignalStrategy::OnIncomingMessage(buzz::XmlElement* stanza) {
  pending_messages_.push(stanza);
  MessageLoop::current()->PostTask(
      FROM_HERE, task_factory_.NewRunnableMethod(
          &FakeSignalStrategy::DeliverIncomingMessages));
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

    if (listener_)
      listener_->OnIncomingStanza(stanza);
    iq_registry_.OnIncomingStanza(stanza);

    pending_messages_.pop();
    delete stanza;
  }
}

}  // namespace remoting
