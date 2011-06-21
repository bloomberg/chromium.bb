// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/javascript_signal_strategy.h"

#include "remoting/jingle_glue/iq_request.h"
#include "remoting/jingle_glue/xmpp_proxy.h"

namespace remoting {

JavascriptSignalStrategy::JavascriptSignalStrategy(const std::string& your_jid)
    : your_jid_(your_jid) {
}

JavascriptSignalStrategy::~JavascriptSignalStrategy() {
}

void JavascriptSignalStrategy::AttachXmppProxy(
    scoped_refptr<XmppProxy> xmpp_proxy) {
  xmpp_proxy_ = xmpp_proxy;
  xmpp_proxy_->AttachCallback(iq_registry_.AsWeakPtr());
}

void JavascriptSignalStrategy::Init(StatusObserver* observer) {
  // Blast through each state since for a JavascriptSignalStrategy, we're
  // already connected.
  //
  // TODO(ajwong): Clarify the status API contract to see if we have to actually
  // walk through each state.
  observer->OnStateChange(StatusObserver::START);
  observer->OnStateChange(StatusObserver::CONNECTING);
  observer->OnJidChange(your_jid_);
  observer->OnStateChange(StatusObserver::CONNECTED);
}

void JavascriptSignalStrategy::StartSession(
    cricket::SessionManager* session_manager) {
  session_start_request_.reset(
      new SessionStartRequest(CreateIqRequest(), session_manager));
  session_start_request_->Run();
}

void JavascriptSignalStrategy::EndSession() {
  if (xmpp_proxy_) {
    xmpp_proxy_->DetachCallback();
  }
  xmpp_proxy_ = NULL;
}

JavascriptIqRequest* JavascriptSignalStrategy::CreateIqRequest() {
  return new JavascriptIqRequest(&iq_registry_, xmpp_proxy_);
}

}  // namespace remoting
