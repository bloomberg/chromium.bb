// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/javascript_iq_request.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "remoting/jingle_glue/signal_strategy.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"

namespace remoting {

JavascriptIqRegistry::JavascriptIqRegistry() {
}

JavascriptIqRegistry::~JavascriptIqRegistry() {
}

void JavascriptIqRegistry::RemoveAllRequests(JavascriptIqRequest* request) {
  IqRequestMap::iterator it = requests_.begin();
  while (it != requests_.end()) {
    IqRequestMap::iterator cur = it;
    ++it;
    if (cur->second == request) {
      requests_.erase(cur);
    }
  }
}

void JavascriptIqRegistry::OnIncomingStanza(const buzz::XmlElement* stanza) {
  // TODO(ajwong): Can we cleanup this dispatch at all?  The send is from
  // JavascriptIqRequest but the return is in JavascriptIqRegistry.

  if (stanza->Name() != buzz::QN_IQ) {
    LOG(WARNING) << "Received unexpected non-IQ packet" << stanza->Str();
    return;
  }

  if (!stanza->HasAttr(buzz::QN_ID)) {
    LOG(WARNING) << "IQ packet missing id" << stanza->Str();
    return;
  }

  const std::string& id = stanza->Attr(buzz::QN_ID);

  IqRequestMap::iterator it = requests_.find(id);
  if (it == requests_.end()) {
    VLOG(1) << "Dropping IQ packet with no request id: " << stanza->Str();
  } else {
    // TODO(ajwong): We should look at the logic inside libjingle's
    // XmppTask::MatchResponseIq() and make sure we're fully in sync.
    // They check more fields and conditions than us.

    // TODO(ajwong): This logic is weird.  We add to the register in
    // JavascriptIqRequest::SendIq(), but remove in
    // JavascriptIqRegistry::OnIq().  We should try to keep the
    // registration/deregistration in one spot.
    if (!it->second->callback_.is_null()) {
      it->second->callback_.Run(stanza);
      it->second->callback_.Reset();
    } else {
      VLOG(1) << "No callback, so dropping: " << stanza->Str();
    }
    requests_.erase(it);
  }
}

void JavascriptIqRegistry::RegisterRequest(
    JavascriptIqRequest* request, const std::string& id) {
  DCHECK(requests_.find(id) == requests_.end());
  requests_[id] = request;
}

JavascriptIqRequest::JavascriptIqRequest(SignalStrategy* signal_strategy,
                                         JavascriptIqRegistry* registry)
    : signal_strategy_(signal_strategy),
      registry_(registry) {
}

JavascriptIqRequest::~JavascriptIqRequest() {
  registry_->RemoveAllRequests(this);
}

void JavascriptIqRequest::SendIq(buzz::XmlElement* stanza) {
  std::string id = signal_strategy_->GetNextId();
  stanza->AddAttr(buzz::QN_ID, id);
  registry_->RegisterRequest(this, id);
  signal_strategy_->SendStanza(stanza);
}

void JavascriptIqRequest::set_callback(const ReplyCallback& callback) {
  callback_ = callback;
}

}  // namespace remoting
