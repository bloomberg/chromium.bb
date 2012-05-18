// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/jingle_session_manager.h"

#include "base/bind.h"
#include "remoting/jingle_glue/iq_sender.h"
#include "remoting/jingle_glue/jingle_info_request.h"
#include "remoting/jingle_glue/signal_strategy.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/content_description.h"
#include "remoting/protocol/jingle_messages.h"
#include "remoting/protocol/jingle_session.h"
#include "remoting/protocol/transport.h"
#include "remoting/protocol/transport_config.h"
#include "third_party/libjingle/source/talk/base/socketaddress.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

using buzz::QName;

namespace remoting {
namespace protocol {

JingleSessionManager::JingleSessionManager(
    scoped_ptr<TransportFactory> transport_factory,
    bool fetch_stun_relay_config)
    : transport_factory_(transport_factory.Pass()),
      fetch_stun_relay_config_(fetch_stun_relay_config),
      signal_strategy_(NULL),
      listener_(NULL),
      ready_(false) {
}

JingleSessionManager::~JingleSessionManager() {
  Close();
}

void JingleSessionManager::Init(
    SignalStrategy* signal_strategy,
    SessionManager::Listener* listener) {
  listener_ = listener;
  signal_strategy_ = signal_strategy;
  iq_sender_.reset(new IqSender(signal_strategy_));

  signal_strategy_->AddListener(this);

  OnSignalStrategyStateChange(signal_strategy_->GetState());
}

void JingleSessionManager::OnJingleInfo(
    const std::string& relay_token,
    const std::vector<std::string>& relay_hosts,
    const std::vector<talk_base::SocketAddress>& stun_hosts) {
  DCHECK(CalledOnValidThread());

  // TODO(sergeyu): Add support for multiple STUN/relay servers when
  // it's implemented in libjingle and P2P Transport API.
  TransportConfig config;
  config.stun_server = stun_hosts[0].ToString();
  config.relay_server = relay_hosts[0];
  config.relay_token = relay_token;
  transport_factory_->SetTransportConfig(config);

  VLOG(1) << "STUN server: " << config.stun_server
          << " Relay server: " << config.relay_server
          << " Relay token: " << config.relay_token;


  if (!ready_) {
    ready_ = true;
    listener_->OnSessionManagerReady();
  }
}

scoped_ptr<Session> JingleSessionManager::Connect(
    const std::string& host_jid,
    scoped_ptr<Authenticator> authenticator,
    scoped_ptr<CandidateSessionConfig> config,
    const Session::StateChangeCallback& state_change_callback) {
  scoped_ptr<JingleSession> session(new JingleSession(this));
  session->StartConnection(host_jid, authenticator.Pass(), config.Pass(),
                           state_change_callback);
  sessions_[session->session_id_] = session.get();
  return session.PassAs<Session>();
}

void JingleSessionManager::Close() {
  DCHECK(CalledOnValidThread());

  // Close() can be called only after all sessions are destroyed.
  DCHECK(sessions_.empty());

  listener_ = NULL;
  jingle_info_request_.reset();

  if (signal_strategy_) {
    signal_strategy_->RemoveListener(this);
    signal_strategy_ = NULL;
  }
}

void JingleSessionManager::set_authenticator_factory(
    scoped_ptr<AuthenticatorFactory> authenticator_factory) {
  DCHECK(CalledOnValidThread());
  authenticator_factory_ = authenticator_factory.Pass();
}

void JingleSessionManager::OnSignalStrategyStateChange(
    SignalStrategy::State state) {
  if (state == SignalStrategy::CONNECTED) {
    // Request STUN/Relay info if necessary.
    if (fetch_stun_relay_config_) {
      jingle_info_request_.reset(new JingleInfoRequest(signal_strategy_));
      jingle_info_request_->Send(base::Bind(&JingleSessionManager::OnJingleInfo,
                                            base::Unretained(this)));
    } else if (!ready_) {
      ready_ = true;
      listener_->OnSessionManagerReady();
    }
  }
}

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
    session->InitializeIncomingConnection(message, authenticator.Pass());
    sessions_[session->session_id_] = session;

    IncomingSessionResponse response = SessionManager::DECLINE;
    listener_->OnIncomingSession(session, &response);

    if (response == SessionManager::ACCEPT) {
      session->AcceptIncomingConnection(message);
    } else {
      ErrorCode error;
      switch (response) {
        case INCOMPATIBLE:
          error = INCOMPATIBLE_PROTOCOL;
          break;

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

      session->CloseInternal(error);
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
