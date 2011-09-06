// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/xmpp_iq_request.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"
#include "third_party/libjingle/source/talk/xmpp/xmppclient.h"

namespace remoting {

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

void XmppIqRequest::SendIq(buzz::XmlElement* stanza) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  // Unregister the handler if it is already registered.
  Unregister();

  stanza->AddAttr(buzz::QN_ID, xmpp_client_->NextId());
  xmpp_client_->engine()->SendIq(stanza, this, &cookie_);
}

void XmppIqRequest::set_callback(const ReplyCallback& callback) {
  callback_ = callback;
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
  if (!callback_.is_null()) {
    callback_.Run(stanza);
    callback_.Reset();
  }
}

}  // namespace remoting
