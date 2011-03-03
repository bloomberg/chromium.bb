// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/iq_request.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"
#include "third_party/libjingle/source/talk/xmpp/xmppclient.h"

namespace remoting {

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

XmppIqRequest::XmppIqRequest(MessageLoop* message_loop,
                             buzz::XmppClient* xmpp_client)
    : message_loop_(message_loop),
      xmpp_client_(xmpp_client),
      cookie_(NULL) {
  DCHECK(xmpp_client_);
  DCHECK_EQ(MessageLoop::current(), message_loop_);
}

XmppIqRequest::~XmppIqRequest() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  Unregister();
}

void XmppIqRequest::SendIq(const std::string& type,
                       const std::string& addressee,
                       buzz::XmlElement* iq_body) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  // Unregister the handler if it is already registered.
  Unregister();

  DCHECK_GT(type.length(), 0U);
  DCHECK_GT(addressee.length(), 0U);

  scoped_ptr<buzz::XmlElement> stanza(MakeIqStanza(type, addressee, iq_body,
                                                   xmpp_client_->NextId()));

  xmpp_client_->engine()->SendIq(stanza.get(), this, &cookie_);
}

void XmppIqRequest::set_callback(ReplyCallback* callback) {
  callback_.reset(callback);
}

void XmppIqRequest::Unregister() {
  if (cookie_) {
    // No need to unregister the handler if the client has been destroyed.
    if (xmpp_client_) {
      xmpp_client_->engine()->RemoveIqHandler(cookie_, NULL);
    }
    cookie_ = NULL;
  }
}

void XmppIqRequest::IqResponse(buzz::XmppIqCookie cookie,
                           const buzz::XmlElement* stanza) {
  if (callback_.get() != NULL) {
    callback_->Run(stanza);
  }
}

JavascriptIqRequest::JavascriptIqRequest() {
}

JavascriptIqRequest::~JavascriptIqRequest() {
}

void JavascriptIqRequest::SendIq(const std::string& type,
                                 const std::string& addressee,
                                 buzz::XmlElement* iq_body) {
  NOTIMPLEMENTED();
  // TODO(ajwong): The "1" below is completely wrong. Need to change to use a
  // sequence that just increments or something.
  scoped_ptr<buzz::XmlElement> stanza(
      MakeIqStanza(type, addressee, iq_body, "1"));

  xmpp_proxy_->SendIq(stanza->Str());
}

void JavascriptIqRequest::ReceiveIq(const std::string& iq_response) {
  // TODO(ajwong): Somehow send this to callback_ here.
  LOG(ERROR) << "Got IQ!!!!!!\n" << iq_response;
}

void JavascriptIqRequest::set_callback(ReplyCallback* callback) {
  callback_.reset(callback);
}

}  // namespace remoting
