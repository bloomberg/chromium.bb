// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/iq_sender.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "remoting/jingle_glue/signal_strategy.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"

namespace remoting {

// static
buzz::XmlElement* IqSender::MakeIqStanza(const std::string& type,
                                         const std::string& addressee,
                                         buzz::XmlElement* iq_body) {
  buzz::XmlElement* stanza = new buzz::XmlElement(buzz::QN_IQ);
  stanza->AddAttr(buzz::QN_TYPE, type);
  if (!addressee.empty())
    stanza->AddAttr(buzz::QN_TO, addressee);
  stanza->AddElement(iq_body);
  return stanza;
}

IqSender::IqSender(SignalStrategy* signal_strategy)
    : signal_strategy_(signal_strategy) {
  signal_strategy_->AddListener(this);
}

IqSender::~IqSender() {
  signal_strategy_->RemoveListener(this);
}

IqRequest* IqSender::SendIq(buzz::XmlElement* stanza,
                            const ReplyCallback& callback) {
  std::string id = signal_strategy_->GetNextId();
  stanza->AddAttr(buzz::QN_ID, id);
  if (!signal_strategy_->SendStanza(stanza)) {
    return NULL;
  }
  DCHECK(requests_.find(id) == requests_.end());
  IqRequest* request = new IqRequest(this, callback);
  if (!callback.is_null())
    requests_[id] = request;
  return request;
}

IqRequest* IqSender::SendIq(const std::string& type,
                            const std::string& addressee,
                            buzz::XmlElement* iq_body,
                            const ReplyCallback& callback) {
  return SendIq(MakeIqStanza(type, addressee, iq_body), callback);
}

void IqSender::RemoveRequest(IqRequest* request) {
  IqRequestMap::iterator it = requests_.begin();
  while (it != requests_.end()) {
    IqRequestMap::iterator cur = it;
    ++it;
    if (cur->second == request) {
      requests_.erase(cur);
      break;
    }
  }
}

void IqSender::OnSignalStrategyStateChange(SignalStrategy::State state) {
}

bool IqSender::OnSignalStrategyIncomingStanza(const buzz::XmlElement* stanza) {
  if (stanza->Name() != buzz::QN_IQ) {
    LOG(WARNING) << "Received unexpected non-IQ packet " << stanza->Str();
    return false;
  }

  const std::string& type = stanza->Attr(buzz::QN_TYPE);
  if (type.empty()) {
    LOG(WARNING) << "IQ packet missing type " << stanza->Str();
    return false;
  }

  if (type != "result" && type != "error") {
    return false;
  }

  const std::string& id = stanza->Attr(buzz::QN_ID);
  if (id.empty()) {
    LOG(WARNING) << "IQ packet missing id " << stanza->Str();
    return false;
  }

  IqRequestMap::iterator it = requests_.find(id);
  if (it == requests_.end()) {
    return false;
  }

  IqRequest* request = it->second;
  requests_.erase(it);

  request->OnResponse(stanza);
  return true;
}

IqRequest::IqRequest(IqSender* sender, const IqSender::ReplyCallback& callback)
    : sender_(sender),
      callback_(callback) {
}

IqRequest::~IqRequest() {
  sender_->RemoveRequest(this);
}

void IqRequest::OnResponse(const buzz::XmlElement* stanza) {
  DCHECK(!callback_.is_null());
  IqSender::ReplyCallback callback(callback_);
  callback_.Reset();
  callback.Run(stanza);
}

}  // namespace remoting
