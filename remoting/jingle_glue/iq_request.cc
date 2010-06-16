// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/iq_request.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "remoting/jingle_glue/jingle_client.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"
#include "third_party/libjingle/source/talk/xmpp/xmppengine.h"

namespace remoting {

IqRequest::IqRequest(JingleClient* jingle_client)
    : jingle_client_(jingle_client),
      cookie_(NULL) {
  DCHECK(jingle_client_ != NULL);
  DCHECK(MessageLoop::current() == jingle_client_->message_loop());
}

IqRequest::~IqRequest() {
  DCHECK(MessageLoop::current() == jingle_client_->message_loop());
  Unregister();
}

void IqRequest::SendIq(const std::string& type,
                       const std::string& addressee,
                       buzz::XmlElement* iq_body) {
  DCHECK(MessageLoop::current() == jingle_client_->message_loop());

  // Unregister the handler if it is already registered.
  Unregister();

  DCHECK_GT(type.length(), 0U);
  DCHECK_GT(addressee.length(), 0U);

  buzz::XmppClient* xmpp_client = jingle_client_->xmpp_client();
  DCHECK(xmpp_client);  // Expect that connection is active.

  scoped_ptr<buzz::XmlElement> stanza(MakeIqStanza(type, addressee, iq_body,
                                                   xmpp_client->NextId()));

  xmpp_client->engine()->SendIq(stanza.get(), this, &cookie_);
}

// static
buzz::XmlElement* IqRequest::MakeIqStanza(const std::string& type,
                                          const std::string& addressee,
                                          buzz::XmlElement* iq_body,
                                          const std::string& id) {
  buzz::XmlElement* stanza = new buzz::XmlElement(buzz::QN_IQ);
  stanza->AddAttr(buzz::QN_TYPE, type);
  stanza->AddAttr(buzz::QN_TO, addressee);
  stanza->AddAttr(buzz::QN_ID, id);
  stanza->AddElement(iq_body);
  return stanza;
}

void IqRequest::Unregister() {
  if (cookie_) {
    buzz::XmppClient* xmpp_client = jingle_client_->xmpp_client();
    // No need to unregister the handler if the client has been destroyed.
    if (xmpp_client) {
      xmpp_client->engine()->RemoveIqHandler(cookie_, NULL);
    }
    cookie_ = NULL;
  }
}

void IqRequest::IqResponse(buzz::XmppIqCookie cookie,
                           const buzz::XmlElement* stanza) {
  if (callback_.get() != NULL) {
    callback_->Run(stanza);
  }
}

}  // namespace remoting
