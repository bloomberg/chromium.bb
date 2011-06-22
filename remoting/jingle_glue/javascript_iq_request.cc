// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/javascript_iq_request.h"

#include "base/string_number_conversions.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"

namespace remoting {

JavascriptIqRegistry::JavascriptIqRegistry()
   : current_id_(0),
     default_handler_(NULL) {
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

void JavascriptIqRegistry::SetDefaultHandler(JavascriptIqRequest* new_handler) {
  // Should only allow a new handler if |default_handler_| is NULL.
  CHECK(default_handler_ == NULL || !new_handler);
  default_handler_ = new_handler;
}

void JavascriptIqRegistry::OnIq(const std::string& response_xml) {
  // TODO(ajwong): Can we cleanup this dispatch at all?  The send is from
  // JavascriptIqRequest but the return is in JavascriptIqRegistry.
  scoped_ptr<buzz::XmlElement> stanza(buzz::XmlElement::ForStr(response_xml));

  if (!stanza.get()) {
    LOG(WARNING) << "Malformed XML received" << response_xml;
    return;
  }

  LOG(ERROR) << "IQ Received: " << stanza->Str();
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
    if (!default_handler_) {
      VLOG(1) << "Dropping IQ packet with no request id: " << stanza->Str();
    } else {
      if (default_handler_->callback_.get()) {
        default_handler_->callback_->Run(stanza.get());
      } else {
        VLOG(1) << "default handler has no callback, so dropping: "
                << stanza->Str();
      }
    }
  } else {
    // TODO(ajwong): We should look at the logic inside libjingle's
    // XmppTask::MatchResponseIq() and make sure we're fully in sync.
    // They check more fields and conditions than us.

    // TODO(ajwong): This logic is weird.  We add to the register in
    // JavascriptIqRequest::SendIq(), but remove in
    // JavascriptIqRegistry::OnIq().  We should try to keep the
    // registration/deregistration in one spot.
    if (it->second->callback_.get()) {
      it->second->callback_->Run(stanza.get());
    } else {
      VLOG(1) << "No callback, so dropping: " << stanza->Str();
    }
    requests_.erase(it);
  }
}

std::string JavascriptIqRegistry::RegisterRequest(
    JavascriptIqRequest* request) {
  ++current_id_;
  std::string id_as_string = base::IntToString(current_id_);

  requests_[id_as_string] = request;
  return id_as_string;
}

JavascriptIqRequest::JavascriptIqRequest(JavascriptIqRegistry* registry,
                                         scoped_refptr<XmppProxy> xmpp_proxy)
    : xmpp_proxy_(xmpp_proxy),
      registry_(registry),
      is_default_handler_(false) {
}

JavascriptIqRequest::~JavascriptIqRequest() {
  registry_->RemoveAllRequests(this);
  if (is_default_handler_) {
    registry_->SetDefaultHandler(NULL);
  }
}

void JavascriptIqRequest::SendIq(const std::string& type,
                                 const std::string& addressee,
                                 buzz::XmlElement* iq_body) {
  scoped_ptr<buzz::XmlElement> stanza(
      MakeIqStanza(type, addressee, iq_body,
                   registry_->RegisterRequest(this)));

  xmpp_proxy_->SendIq(stanza->Str());
}

void JavascriptIqRequest::SendRawIq(buzz::XmlElement* stanza) {
  xmpp_proxy_->SendIq(stanza->Str());
}

void JavascriptIqRequest::BecomeDefaultHandler() {
  is_default_handler_ = true;
  registry_->SetDefaultHandler(this);
}

void JavascriptIqRequest::set_callback(ReplyCallback* callback) {
  callback_.reset(callback);
}

}  // namespace remoting
