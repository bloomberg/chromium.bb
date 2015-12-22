// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/jingle_session_manager.h"

#include "base/bind.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/content_description.h"
#include "remoting/protocol/jingle_messages.h"
#include "remoting/protocol/jingle_session.h"
#include "remoting/protocol/transport.h"
#include "remoting/signaling/iq_sender.h"
#include "remoting/signaling/signal_strategy.h"
#include "third_party/webrtc/base/socketaddress.h"
#include "third_party/webrtc/libjingle/xmllite/xmlelement.h"

using buzz::QName;

namespace remoting {
namespace protocol {

JingleSessionManager::JingleSessionManager(
    scoped_ptr<TransportFactory> transport_factory,
    SignalStrategy* signal_strategy)
    : transport_factory_(std::move(transport_factory)),
      signal_strategy_(signal_strategy),
      protocol_config_(CandidateSessionConfig::CreateDefault()),
      iq_sender_(new IqSender(signal_strategy_)) {
  signal_strategy_->AddListener(this);
}

JingleSessionManager::~JingleSessionManager() {
  DCHECK(sessions_.empty());
  signal_strategy_->RemoveListener(this);
}

void JingleSessionManager::AcceptIncoming(
    const IncomingSessionCallback& incoming_session_callback) {
  incoming_session_callback_ = incoming_session_callback;

}

void JingleSessionManager::set_protocol_config(
    scoped_ptr<CandidateSessionConfig> config) {
  protocol_config_ = std::move(config);
}

scoped_ptr<Session> JingleSessionManager::Connect(
    const std::string& host_jid,
    scoped_ptr<Authenticator> authenticator) {
  scoped_ptr<JingleSession> session(new JingleSession(this));
  session->StartConnection(host_jid, std::move(authenticator));
  sessions_[session->session_id_] = session.get();
  return std::move(session);
}

void JingleSessionManager::set_authenticator_factory(
    scoped_ptr<AuthenticatorFactory> authenticator_factory) {
  DCHECK(CalledOnValidThread());
  authenticator_factory_ = std::move(authenticator_factory);
}

void JingleSessionManager::OnSignalStrategyStateChange(
    SignalStrategy::State state) {}

bool JingleSessionManager::OnSignalStrategyIncomingStanza(
    const buzz::XmlElement* stanza) {
  if (!JingleMessage::IsJingleMessage(stanza))
    return false;

  JingleMessage message;
  std::string error;
  if (!message.ParseXml(stanza, &error)) {
    SendReply(stanza, JingleMessageReply::BAD_REQUEST);
    return true;
  }

  if (message.action == JingleMessage::SESSION_INITIATE) {
    // Description must be present in session-initiate messages.
    DCHECK(message.description.get());

    SendReply(stanza, JingleMessageReply::NONE);

    scoped_ptr<Authenticator> authenticator =
        authenticator_factory_->CreateAuthenticator(
            signal_strategy_->GetLocalJid(), message.from,
            message.description->authenticator_message());

    JingleSession* session = new JingleSession(this);
    session->InitializeIncomingConnection(message, std::move(authenticator));
    sessions_[session->session_id_] = session;

    // Destroy the session if it was rejected due to incompatible protocol.
    if (session->state_ != Session::ACCEPTING) {
      delete session;
      DCHECK(sessions_.find(message.sid) == sessions_.end());
      return true;
    }

    IncomingSessionResponse response = SessionManager::DECLINE;
    if (!incoming_session_callback_.is_null())
      incoming_session_callback_.Run(session, &response);

    if (response == SessionManager::ACCEPT) {
      session->AcceptIncomingConnection(message);
    } else {
      ErrorCode error;
      switch (response) {
        case OVERLOAD:
          error = HOST_OVERLOAD;
          break;

        case DECLINE:
          error = SESSION_REJECTED;
          break;

        default:
          NOTREACHED();
          error = SESSION_REJECTED;
      }

      session->Close(error);
      delete session;
      DCHECK(sessions_.find(message.sid) == sessions_.end());
    }

    return true;
  }

  SessionsMap::iterator it = sessions_.find(message.sid);
  if (it == sessions_.end()) {
    SendReply(stanza, JingleMessageReply::INVALID_SID);
    return true;
  }

  it->second->OnIncomingMessage(message, base::Bind(
      &JingleSessionManager::SendReply, base::Unretained(this), stanza));
  return true;
}

void JingleSessionManager::SendReply(const buzz::XmlElement* original_stanza,
                                     JingleMessageReply::ErrorType error) {
  signal_strategy_->SendStanza(
      JingleMessageReply(error).ToXml(original_stanza));
}

void JingleSessionManager::SessionDestroyed(JingleSession* session) {
  sessions_.erase(session->session_id_);
}

}  // namespace protocol
}  // namespace remoting
