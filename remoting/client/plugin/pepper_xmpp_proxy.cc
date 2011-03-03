// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_xmpp_proxy.h"

namespace remoting {

PepperXmppProxy::PepperXmppProxy(MessageLoop* jingle_message_loop,
                                 MessageLoop* pepper_message_loop)
    : jingle_message_loop_(jingle_message_loop),
      pepper_message_loop_(pepper_message_loop) {
}

void PepperXmppProxy::AttachScriptableObject(
    ChromotingScriptableObject* scriptable_object,
    Task* done) {
  DCHECK_EQ(jingle_message_loop_, MessageLoop::Current());
  scriptable_object_ = scriptable_object;
  scriptable_object_->AttachXmppProxy(this);
}

void PepperXmppProxy::AttachJavascriptIqRequest(
    JavascriptIqRequest* javascript_iq_request) {
  DCHECK_EQ(pepper_message_loop_, MessageLoop::Current());
  javascript_iq_request_ = javascript_iq_request;
}

void PepperXmppProxy::SendIq(const std::string& iq_request_xml) {
  if (MessageLoop::Current() != pepper_message_loop_) {
    pepper_message_loop_->PostTask(
        NewRunnableMethod(this,
                          &PepperXmppProxy::SendIq,
                          iq_request_xml));
    return;
  }

  if (scriptable_object_) {
    scriptable_object_->SendIq(iq_request_xml, this);
  }
}

void PepperXmppProxy::ReceiveIq(const std::string& iq_response_xml) {
  if (MessageLoop::Current() != jingle_message_loop_) {
    jingle_message_loop_->PostTask(
        NewRunnableMethod(this,
                          &PepperXmppProxy::ReceiveIq,
                          iq_response_xml));
    return;
  }

  if (javascript_iq_request_) {
    javascript_iq_request_->handleIqResponse(iq_response_xml);
  }
}

}  // namespace remoting
