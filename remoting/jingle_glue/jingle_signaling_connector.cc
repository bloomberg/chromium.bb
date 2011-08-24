// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/jingle_signaling_connector.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "remoting/jingle_glue/javascript_iq_request.h"
#include "third_party/libjingle/source/talk/p2p/base/sessionmanager.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"
#include "third_party/libjingle/source/talk/xmpp/xmppclient.h"

namespace remoting {

namespace {

// GTalk sometimes generates service-unavailable error messages with
// incorrect namespace. This method fixes such messages.
// TODO(sergeyu): Fix this on the server side.
void FixErrorStanza(buzz::XmlElement* stanza) {
  if (!stanza->FirstNamed(buzz::QN_ERROR)) {
    buzz::XmlElement* error = stanza->FirstNamed(buzz::QName("", "error"));
    error->SetName(buzz::QN_ERROR);
  }
}

}  // namespace

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
  STLDeleteContainerPairSecondPointers(pending_requests_.begin(),
                                       pending_requests_.end());
}

bool JingleSignalingConnector::OnIncomingStanza(
    const buzz::XmlElement* stanza) {
  if (session_manager_->IsSessionMessage(stanza)) {
    session_manager_->OnIncomingMessage(stanza);
    return true;
  }

  if (stanza->Name() == buzz::QN_IQ) {
    std::string type = stanza->Attr(buzz::QN_TYPE);
    std::string id = stanza->Attr(buzz::QN_ID);
    if ((type == "error" || type == "result") && !id.empty()) {
      IqRequestsMap::iterator it = pending_requests_.find(id);
      if (it != pending_requests_.end()) {
        if (type == "result") {
          session_manager_->OnIncomingResponse(it->second, stanza);
        } else {
          scoped_ptr<buzz::XmlElement> stanza_copy(
              new buzz::XmlElement(*stanza));
          FixErrorStanza(stanza_copy.get());
          session_manager_->OnFailedSend(it->second, stanza_copy.get());
        }
        delete it->second;
        pending_requests_.erase(it);
        return true;
      }
    }
  }

  return false;
}

void JingleSignalingConnector::OnOutgoingMessage(
    cricket::SessionManager* session_manager,
    const buzz::XmlElement* stanza) {
  DCHECK_EQ(session_manager, session_manager_);
  scoped_ptr<buzz::XmlElement> stanza_copy(new buzz::XmlElement(*stanza));

  if (stanza_copy->Name() == buzz::QN_IQ) {
    std::string type = stanza_copy->Attr(buzz::QN_TYPE);
    if (type == "set" || type == "get") {
      std::string id = stanza_copy->Attr(buzz::QN_ID);

      // Add ID attribute for set and get stanzas if it is not there.
      if (id.empty()) {
        id = signal_strategy_->GetNextId();
        stanza_copy->SetAttr(buzz::QN_ID, id);
      }

      // Save the outgoing request for OnIncomingResponse().
      pending_requests_[id] = new buzz::XmlElement(*stanza);
    }
  }

  signal_strategy_->SendStanza(stanza_copy.release());
}

}  // namespace remoting
