// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/iq_request.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "remoting/jingle_glue/signal_strategy.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"

namespace remoting {

IqRegistry::IqRegistry() {
}

IqRegistry::~IqRegistry() {
}

void IqRegistry::RemoveAllRequests(IqRequest* request) {
  IqRequestMap::iterator it = requests_.begin();
  while (it != requests_.end()) {
    IqRequestMap::iterator cur = it;
    ++it;
    if (cur->second == request) {
      requests_.erase(cur);
    }
  }
}

bool IqRegistry::OnIncomingStanza(const buzz::XmlElement* stanza) {
  // TODO(ajwong): Can we cleanup this dispatch at all?  The send is from
  // IqRequest but the return is in IqRegistry.

  if (stanza->Name() != buzz::QN_IQ) {
    LOG(WARNING) << "Received unexpected non-IQ packet" << stanza->Str();
    return false;
  }

  if (!stanza->HasAttr(buzz::QN_ID)) {
    LOG(WARNING) << "IQ packet missing id" << stanza->Str();
    return false;
  }

  const std::string& id = stanza->Attr(buzz::QN_ID);

  IqRequestMap::iterator it = requests_.find(id);
  if (it == requests_.end()) {
    return false;
  }

  // TODO(ajwong): We should look at the logic inside libjingle's
  // XmppTask::MatchResponseIq() and make sure we're fully in sync.
  // They check more fields and conditions than us.

  // TODO(ajwong): This logic is weird.  We add to the register in
  // IqRequest::SendIq(), but remove in
  // IqRegistry::OnIncomingStanza(). We should try to keep the
  // registration/deregistration in one spot.
  if (!it->second->callback_.is_null()) {
    it->second->callback_.Run(stanza);
    it->second->callback_.Reset();
  } else {
    VLOG(1) << "No callback, so dropping: " << stanza->Str();
  }
  requests_.erase(it);
  return true;
}

void IqRegistry::RegisterRequest(IqRequest* request, const std::string& id) {
  DCHECK(requests_.find(id) == requests_.end());
  requests_[id] = request;
}

// static
buzz::XmlElement* IqRequest::MakeIqStanza(const std::string& type,
                                          const std::string& addressee,
                                          buzz::XmlElement* iq_body) {
  buzz::XmlElement* stanza = new buzz::XmlElement(buzz::QN_IQ);
  stanza->AddAttr(buzz::QN_TYPE, type);
  if (!addressee.empty())
    stanza->AddAttr(buzz::QN_TO, addressee);
  stanza->AddElement(iq_body);
  return stanza;
}

IqRequest::IqRequest()
    : signal_strategy_(NULL),
      registry_(NULL) {
}

IqRequest::IqRequest(SignalStrategy* signal_strategy, IqRegistry* registry)
    : signal_strategy_(signal_strategy),
      registry_(registry) {
}

IqRequest::~IqRequest() {
  if (registry_)
    registry_->RemoveAllRequests(this);
}

void IqRequest::SendIq(buzz::XmlElement* stanza) {
  std::string id = signal_strategy_->GetNextId();
  stanza->AddAttr(buzz::QN_ID, id);
  registry_->RegisterRequest(this, id);
  signal_strategy_->SendStanza(stanza);
}

void IqRequest::set_callback(const ReplyCallback& callback) {
  callback_ = callback;
}

}  // namespace remoting
