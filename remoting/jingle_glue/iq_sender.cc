// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/iq_sender.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "remoting/jingle_glue/signal_strategy.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"

namespace remoting {

// static
scoped_ptr<buzz::XmlElement> IqSender::MakeIqStanza(
    const std::string& type,
    const std::string& addressee,
    scoped_ptr<buzz::XmlElement> iq_body) {
  scoped_ptr<buzz::XmlElement> stanza(new buzz::XmlElement(buzz::QN_IQ));
  stanza->AddAttr(buzz::QN_TYPE, type);
  if (!addressee.empty())
    stanza->AddAttr(buzz::QN_TO, addressee);
  stanza->AddElement(iq_body.release());
  return stanza.Pass();
}

IqSender::IqSender(SignalStrategy* signal_strategy)
    : signal_strategy_(signal_strategy) {
  signal_strategy_->AddListener(this);
}

IqSender::~IqSender() {
  signal_strategy_->RemoveListener(this);
}

scoped_ptr<IqRequest> IqSender::SendIq(scoped_ptr<buzz::XmlElement> stanza,
                                       const ReplyCallback& callback) {
  std::string addressee = stanza->Attr(buzz::QN_TO);
  std::string id = signal_strategy_->GetNextId();
  stanza->AddAttr(buzz::QN_ID, id);
  if (!signal_strategy_->SendStanza(stanza.Pass())) {
    return scoped_ptr<IqRequest>(NULL);
  }
  DCHECK(requests_.find(id) == requests_.end());
  scoped_ptr<IqRequest> request(new IqRequest(this, callback, addressee));
  if (!callback.is_null())
    requests_[id] = request.get();
  return request.Pass();
}

scoped_ptr<IqRequest> IqSender::SendIq(const std::string& type,
                                       const std::string& addressee,
                                       scoped_ptr<buzz::XmlElement> iq_body,
                                       const ReplyCallback& callback) {
  return SendIq(MakeIqStanza(type, addressee, iq_body.Pass()), callback);
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

  std::string from = stanza->Attr(buzz::QN_FROM);

  IqRequestMap::iterator it = requests_.find(id);

  // This is a hack to workaround the issue with the WCS changing IDs
  // of IQ responses sent from client to host. Whenever we receive a
  // stanza with an unknown ID we try to match it with an outstanding
  // request sent to the same peer.
  if (it == requests_.end()) {
    for (it = requests_.begin(); it != requests_.end(); ++it) {
      if (it->second->addressee_ == from) {
        break;
      }
    }
  }

  if (it == requests_.end()) {
    return false;
  }

  IqRequest* request = it->second;

  if (request->addressee_ != from) {
    LOG(ERROR) << "Received IQ response from from a invalid JID. Ignoring it."
               << " Message received from: " << from
               << " Original JID: " << request->addressee_;
    return false;
  }

  requests_.erase(it);
  request->OnResponse(stanza);

  return true;
}

IqRequest::IqRequest(IqSender* sender, const IqSender::ReplyCallback& callback,
                     const std::string& addressee)
    : sender_(sender),
      callback_(callback),
      addressee_(addressee) {
}

IqRequest::~IqRequest() {
  sender_->RemoveRequest(this);
}

void IqRequest::SetTimeout(base::TimeDelta timeout) {
  base::MessageLoopProxy::current()->PostDelayedTask(
      FROM_HERE, base::Bind(&IqRequest::OnTimeout, AsWeakPtr()),
      timeout);
}

void IqRequest::CallCallback(const buzz::XmlElement* stanza) {
  if (!callback_.is_null()) {
    IqSender::ReplyCallback callback(callback_);
    callback_.Reset();
    callback.Run(this, stanza);
  }
}

void IqRequest::OnTimeout() {
  CallCallback(NULL);
}

void IqRequest::OnResponse(const buzz::XmlElement* stanza) {
  // It's unsafe to delete signal strategy here, and the callback may
  // want to do that, so we post task to invoke the callback later.
  scoped_ptr<buzz::XmlElement> stanza_copy(new buzz::XmlElement(*stanza));
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(&IqRequest::DeliverResponse, AsWeakPtr(),
                            base::Passed(&stanza_copy)));
}

void IqRequest::DeliverResponse(scoped_ptr<buzz::XmlElement> stanza) {
  CallCallback(stanza.get());
}

}  // namespace remoting
