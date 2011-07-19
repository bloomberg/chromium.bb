// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "base/message_loop.h"
#include "remoting/client/plugin/chromoting_scriptable_object.h"
#include "remoting/client/plugin/pepper_util.h"
#include "remoting/client/plugin/pepper_xmpp_proxy.h"

namespace remoting {

PepperXmppProxy::PepperXmppProxy(
    base::WeakPtr<ChromotingScriptableObject> scriptable_object,
    MessageLoop* callback_message_loop)
    : scriptable_object_(scriptable_object),
      callback_message_loop_(callback_message_loop) {
  CHECK(CurrentlyOnPluginThread());
}

PepperXmppProxy::~PepperXmppProxy() {
}

void PepperXmppProxy::AttachCallback(base::WeakPtr<ResponseCallback> callback) {
  CHECK_EQ(callback_message_loop_, MessageLoop::current());
  callback_ = callback;
}

void PepperXmppProxy::DetachCallback() {
  callback_.reset();
}

void PepperXmppProxy::SendIq(const std::string& request_xml) {
  if (!CurrentlyOnPluginThread()) {
    RunTaskOnPluginThread(NewRunnableMethod(this,
                                            &PepperXmppProxy::SendIq,
                                            request_xml));
    return;
  }

  if (scriptable_object_) {
    scriptable_object_->SendIq(request_xml);
  }
}

void PepperXmppProxy::OnIq(const std::string& response_xml) {
  if (MessageLoop::current() != callback_message_loop_) {
    callback_message_loop_->PostTask(
        FROM_HERE,NewRunnableMethod(this,
                                    &PepperXmppProxy::OnIq,
                                    response_xml));
    return;
  }

  if (callback_) {
    callback_->OnIq(response_xml);
  }
}

}  // namespace remoting
