// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/pepper_session_manager.h"

#include "base/bind.h"
#include "remoting/jingle_glue/iq_sender.h"
#include "remoting/jingle_glue/jingle_info_request.h"
#include "remoting/jingle_glue/signal_strategy.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/jingle_messages.h"
#include "remoting/protocol/pepper_session.h"
#include "third_party/libjingle/source/talk/base/socketaddress.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

using buzz::QName;

namespace remoting {
namespace protocol {

PepperSessionManager::PepperSessionManager(pp::Instance* pp_instance)
    : pp_instance_(pp_instance),
      signal_strategy_(NULL),
      listener_(NULL),
      allow_nat_traversal_(false) {
}

PepperSessionManager::~PepperSessionManager() {
  Close();
}

void PepperSessionManager::Init(
    const std::string& local_jid,
    SignalStrategy* signal_strategy,
    SessionManager::Listener* listener,
    bool allow_nat_traversal) {
  listener_ = listener;
  local_jid_ = local_jid;
  signal_strategy_ = signal_strategy;
  iq_sender_.reset(new IqSender(signal_strategy_));
  allow_nat_traversal_ = allow_nat_traversal;

  signal_strategy_->AddListener(this);

  // If NAT traversal is enabled then we need to request STUN/Relay info.
  if (allow_nat_traversal) {
    jingle_info_request_.reset(new JingleInfoRequest(signal_strategy_));
    jingle_info_request_->Send(base::Bind(&PepperSessionManager::OnJingleInfo,
                                          base::Unretained(this)));
  } else {
    listener_->OnSessionManagerInitialized();
  }
}

void PepperSessionManager::OnJingleInfo(
    const std::string& relay_token,
    const std::vector<std::string>& relay_hosts,
    const std::vector<talk_base::SocketAddress>& stun_hosts) {
  DCHECK(CalledOnValidThread());

  // TODO(sergeyu): Add support for multiple STUN/relay servers when
  // it's implemented in libjingle and P2P Transport API.
  transport_config_.stun_server = stun_hosts[0].ToString();
  transport_config_.relay_server = relay_hosts[0];
  transport_config_.relay_token = relay_token;
  VLOG(1) << "STUN server: " << transport_config_.stun_server
          << " Relay server: " << transport_config_.relay_server
          << " Relay token: " << transport_config_.relay_token;

  listener_->OnSessionManagerInitialized();
}

Session* PepperSessionManager::Connect(
    const std::string& host_jid,
    Authenticator* authenticator,
    CandidateSessionConfig* config,
    const Session::StateChangeCallback& state_change_callback) {
  PepperSession* session = new PepperSession(this);
  session->StartConnection(host_jid, authenticator, config,
                           state_change_callback);
  sessions_[session->session_id_] = session;
  return session;
}

void PepperSessionManager::Close() {
  DCHECK(CalledOnValidThread());

  // Close() can be called only after all sessions are destroyed.
  DCHECK(sessions_.empty());

  listener_ = NULL;
  jingle_info_request_.reset();

  signal_strategy_->RemoveListener(this);
}

void PepperSessionManager::set_authenticator_factory(
    AuthenticatorFactory* authenticator_factory) {
  DCHECK(CalledOnValidThread());
  authenticator_factory_.reset(authenticator_factory);
}

bool PepperSessionManager::OnIncomingStanza(const buzz::XmlElement* stanza) {
  if (!JingleMessage::IsJingleMessage(stanza))
    return false;

  JingleMessage message;
  std::string error;
  if (!message.ParseXml(stanza, &error)) {
    SendReply(stanza, JingleMessageReply(JingleMessageReply::BAD_REQUEST));
    return true;
  }

  if (message.action == JingleMessage::SESSION_INITIATE) {
    SendReply(stanza, JingleMessageReply(
        JingleMessageReply::NOT_IMPLEMENTED,
        "Can't accept sessions on the client"));
    return true;
  }

  SessionsMap::iterator it = sessions_.find(message.sid);
  if (it == sessions_.end()) {
    SendReply(stanza, JingleMessageReply(JingleMessageReply::INVALID_SID));
    return true;
  }

  JingleMessageReply reply;
  it->second->OnIncomingMessage(message, &reply);
  SendReply(stanza, reply);
  return true;
}

void PepperSessionManager::SendReply(const buzz::XmlElement* original_stanza,
                                     const JingleMessageReply& reply) {
  buzz::XmlElement* stanza = reply.ToXml(original_stanza);
  signal_strategy_->SendStanza(stanza);
}

void PepperSessionManager::SessionDestroyed(PepperSession* session) {
  sessions_.erase(session->session_id_);
}

}  // namespace protocol
}  // namespace remoting
