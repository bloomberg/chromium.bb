// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/jingle_signaling_connector.h"

#include "remoting/jingle_glue/javascript_iq_request.h"
#include "third_party/libjingle/source/talk/p2p/base/sessionmanager.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"
#include "third_party/libjingle/source/talk/xmpp/xmppclient.h"

namespace remoting {

JingleSignalingConnector::JingleSignalingConnector(
    JavascriptIqRequest* request,
    cricket::SessionManager* session_manager)
    : request_(request),
      session_manager_(session_manager) {
  request_->set_callback(
      NewCallback(this, &JingleSignalingConnector::OnResponse));
}

JingleSignalingConnector::~JingleSignalingConnector() {
}

void JingleSignalingConnector::Run() {
  session_manager_->SignalOutgoingMessage.connect(
      this, &JingleSignalingConnector::OnOutgoingMessage);

  // TODO(ajwong): Why are we connecting SessionManager to itself?
  session_manager_->SignalRequestSignaling.connect(
      session_manager_, &cricket::SessionManager::OnSignalingReady);
  request_->BecomeDefaultHandler();
}

void JingleSignalingConnector::OnResponse(const buzz::XmlElement* response) {
  // TODO(ajwong): Techncially, when SessionManager sends IQ packets, it
  // actually expects a response in SessionSendTask(). However, if you look in
  // SessionManager::OnIncomingResponse(), it does nothing with the response.
  // Also, if no response is found, we are supposed to call
  // SessionManager::OnFailedSend().
  //
  // However, for right now, we just ignore those, and only propagate
  // messages outside of the request/reply framework to
  // SessionManager::OnIncomingMessage.

  if (session_manager_->IsSessionMessage(response)) {
    session_manager_->OnIncomingMessage(response);
  }
}

void JingleSignalingConnector::OnOutgoingMessage(
    cricket::SessionManager* session_manager,
    const buzz::XmlElement* stanza) {
  // TODO(ajwong): Are we just supposed to not use |session_manager|?
  DCHECK_EQ(session_manager, session_manager_);
  scoped_ptr<buzz::XmlElement> stanza_copy(new buzz::XmlElement(*stanza));
  request_->SendRawIq(stanza_copy.get());
}

}  // namespace remoting
