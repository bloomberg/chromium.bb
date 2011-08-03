// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/jingle_signaling_connector.h"

#include "base/logging.h"
#include "remoting/jingle_glue/javascript_iq_request.h"
#include "third_party/libjingle/source/talk/p2p/base/sessionmanager.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"
#include "third_party/libjingle/source/talk/xmpp/xmppclient.h"

namespace remoting {

JingleSignalingConnector::JingleSignalingConnector(
    SignalStrategy* signal_strategy,
    cricket::SessionManager* session_manager)
    : signal_strategy_(signal_strategy),
      session_manager_(session_manager) {

  session_manager_->SignalOutgoingMessage.connect(
      this, &JingleSignalingConnector::OnOutgoingMessage);

  signal_strategy_->SetListener(this);

  // Assume that signaling is ready from the beginning.
  session_manager_->SignalRequestSignaling.connect(
      session_manager_, &cricket::SessionManager::OnSignalingReady);
}

JingleSignalingConnector::~JingleSignalingConnector() {
  signal_strategy_->SetListener(NULL);
}

bool JingleSignalingConnector::OnIncomingStanza(
    const buzz::XmlElement* stanza) {
  // TODO(ajwong): Techncially, when SessionManager sends IQ packets, it
  // actually expects a response in SessionSendTask(). However, if you look in
  // SessionManager::OnIncomingResponse(), it does nothing with the response.
  // Also, if no response is found, we are supposed to call
  // SessionManager::OnFailedSend().
  //
  // However, for right now, we just ignore those, and only propagate
  // messages outside of the request/reply framework to
  // SessionManager::OnIncomingMessage.

  if (session_manager_->IsSessionMessage(stanza)) {
    session_manager_->OnIncomingMessage(stanza);
    return true;
  }

  return false;
}

void JingleSignalingConnector::OnOutgoingMessage(
    cricket::SessionManager* session_manager,
    const buzz::XmlElement* stanza) {
  DCHECK_EQ(session_manager, session_manager_);
  scoped_ptr<buzz::XmlElement> stanza_copy(new buzz::XmlElement(*stanza));

  // Add ID attribute for Iq stanzas if it is not there.
  if (stanza_copy->Name() == buzz::QN_IQ &&
      stanza_copy->Attr(buzz::QN_ID).empty()) {
    stanza_copy->SetAttr(buzz::QN_ID, signal_strategy_->GetNextId());
  }

  signal_strategy_->SendStanza(stanza_copy.release());
}

}  // namespace remoting
