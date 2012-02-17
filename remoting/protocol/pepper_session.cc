// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/pepper_session.h"

#include "base/bind.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "remoting/base/constants.h"
#include "remoting/jingle_glue/iq_sender.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/content_description.h"
#include "remoting/protocol/jingle_messages.h"
#include "remoting/protocol/pepper_session_manager.h"
#include "remoting/protocol/session_config.h"
#include "third_party/libjingle/source/talk/p2p/base/candidate.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

using buzz::XmlElement;

namespace remoting {
namespace protocol {

namespace {
// Delay after candidate creation before sending transport-info
// message. This is neccessary to be able to pack multiple candidates
// into one transport-info messages. The value needs to be greater
// than zero because ports are opened asynchronously in the browser
// process.
const int kTransportInfoSendDelayMs = 2;

Session::Error AuthRejectionReasonToError(
    Authenticator::RejectionReason reason) {
  switch (reason) {
    case Authenticator::INVALID_CREDENTIALS:
      return Session::AUTHENTICATION_FAILED;
    case Authenticator::PROTOCOL_ERROR:
      return Session::INCOMPATIBLE_PROTOCOL;
  }
  NOTREACHED();
  return Session::UNKNOWN_ERROR;
}

}  // namespace

PepperSession::PepperSession(PepperSessionManager* session_manager)
    : session_manager_(session_manager),
      state_(INITIALIZING),
      error_(OK),
      config_is_set_(false) {
}

PepperSession::~PepperSession() {
  STLDeleteContainerPairSecondPointers(channels_.begin(), channels_.end());
  session_manager_->SessionDestroyed(this);
}

void PepperSession::SetStateChangeCallback(
    const StateChangeCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!callback.is_null());
  state_change_callback_ = callback;
}

void PepperSession::SetRouteChangeCallback(
    const RouteChangeCallback& callback) {
  DCHECK(CalledOnValidThread());
  route_change_callback_ = callback;
}

Session::Error PepperSession::error() {
  DCHECK(CalledOnValidThread());
  return error_;
}

void PepperSession::StartConnection(
    const std::string& peer_jid,
    scoped_ptr<Authenticator> authenticator,
    scoped_ptr<CandidateSessionConfig> config,
    const StateChangeCallback& state_change_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(authenticator.get());
  DCHECK_EQ(authenticator->state(), Authenticator::MESSAGE_READY);

  peer_jid_ = peer_jid;
  authenticator_ = authenticator.Pass();
  candidate_config_ = config.Pass();
  state_change_callback_ = state_change_callback;

  // Generate random session ID. There are usually not more than 1
  // concurrent session per host, so a random 64-bit integer provides
  // enough entropy. In the worst case connection will fail when two
  // clients generate the same session ID concurrently.
  session_id_ = base::Int64ToString(base::RandGenerator(kint64max));

  // Send session-initiate message.
  JingleMessage message(peer_jid_, JingleMessage::SESSION_INITIATE,
                        session_id_);
  message.description.reset(
      new ContentDescription(candidate_config_->Clone(),
                             authenticator_->GetNextMessage()));
  initiate_request_.reset(session_manager_->iq_sender()->SendIq(
      message.ToXml(),
      base::Bind(&PepperSession::OnSessionInitiateResponse,
                 base::Unretained(this))));

  SetState(CONNECTING);
}

void PepperSession::InitializeIncomingConnection(
    const JingleMessage& initiate_message,
    scoped_ptr<Authenticator> authenticator) {
  DCHECK(CalledOnValidThread());
  DCHECK(initiate_message.description.get());
  DCHECK(authenticator.get());
  DCHECK_EQ(authenticator->state(), Authenticator::WAITING_MESSAGE);

  peer_jid_ = initiate_message.from;
  authenticator_ = authenticator.Pass();
  session_id_ = initiate_message.sid;
  candidate_config_ = initiate_message.description->config()->Clone();

  SetState(CONNECTING);
}

void PepperSession::AcceptIncomingConnection(
    const JingleMessage& initiate_message) {
  DCHECK(config_is_set_);

  // Process the first authentication message.
  const buzz::XmlElement* first_auth_message =
      initiate_message.description->authenticator_message();

  if (!first_auth_message) {
    CloseInternal(INCOMPATIBLE_PROTOCOL);
    return;
  }

  DCHECK_EQ(authenticator_->state(), Authenticator::WAITING_MESSAGE);
  authenticator_->ProcessMessage(first_auth_message);
  if (authenticator_->state() == Authenticator::REJECTED) {
    CloseInternal(AuthRejectionReasonToError(
        authenticator_->rejection_reason()));
    return;
  }

  // Send the session-accept message.
  JingleMessage message(peer_jid_, JingleMessage::SESSION_ACCEPT,
                        session_id_);

  scoped_ptr<buzz::XmlElement> auth_message;
  if (authenticator_->state() == Authenticator::MESSAGE_READY)
    auth_message = authenticator_->GetNextMessage();

  message.description.reset(
      new ContentDescription(CandidateSessionConfig::CreateFrom(config_),
                             auth_message.Pass()));
  initiate_request_.reset(session_manager_->iq_sender()->SendIq(
      message.ToXml(),
      base::Bind(&PepperSession::OnSessionInitiateResponse,
                 base::Unretained(this))));

  // Update state.
  SetState(CONNECTED);

  if (authenticator_->state() == Authenticator::ACCEPTED) {
    SetState(AUTHENTICATED);
  } else {
    DCHECK_EQ(authenticator_->state(), Authenticator::WAITING_MESSAGE);
  }

  return;
}

void PepperSession::OnSessionInitiateResponse(
    const buzz::XmlElement* response) {
  const std::string& type = response->Attr(buzz::QName("", "type"));
  if (type != "result") {
    LOG(ERROR) << "Received error in response to session-initiate message: \""
               << response->Str()
               << "\". Terminating the session.";

    // TODO(sergeyu): There may be different reasons for error
    // here. Parse the response stanza to find failure reason.
    CloseInternal(PEER_IS_OFFLINE);
  }
}

void PepperSession::CreateStreamChannel(
      const std::string& name,
      const StreamChannelCallback& callback) {
  DCHECK(!channels_[name]);

  scoped_ptr<ChannelAuthenticator> channel_authenticator =
      authenticator_->CreateChannelAuthenticator();
  scoped_ptr<StreamTransport> channel =
      session_manager_->transport_factory_->CreateStreamTransport();
  channel->Initialize(name, session_manager_->transport_config_,
                      this, channel_authenticator.Pass());
  channel->Connect(callback);
  channels_[name] = channel.release();
}

void PepperSession::CreateDatagramChannel(
    const std::string& name,
    const DatagramChannelCallback& callback) {
  DCHECK(!channels_[name]);

  scoped_ptr<ChannelAuthenticator> channel_authenticator =
      authenticator_->CreateChannelAuthenticator();
  scoped_ptr<DatagramTransport> channel =
      session_manager_->transport_factory_->CreateDatagramTransport();
  channel->Initialize(name, session_manager_->transport_config_,
                      this, channel_authenticator.Pass());
  channel->Connect(callback);
  channels_[name] = channel.release();
}

void PepperSession::CancelChannelCreation(const std::string& name) {
  ChannelsMap::iterator it = channels_.find(name);
  if (it != channels_.end() && !it->second->is_connected()) {
    delete it->second;
    DCHECK(!channels_[name]);
  }
}

const std::string& PepperSession::jid() {
  DCHECK(CalledOnValidThread());
  return peer_jid_;
}

const CandidateSessionConfig* PepperSession::candidate_config() {
  DCHECK(CalledOnValidThread());
  return candidate_config_.get();
}

const SessionConfig& PepperSession::config() {
  DCHECK(CalledOnValidThread());
  return config_;
}

void PepperSession::set_config(const SessionConfig& config) {
  DCHECK(CalledOnValidThread());
  DCHECK(!config_is_set_);
  config_ = config;
  config_is_set_ = true;
}

void PepperSession::Close() {
  DCHECK(CalledOnValidThread());

  CloseInternal(OK);
}

void PepperSession::OnTransportCandidate(Transport* transport,
                                         const cricket::Candidate& candidate) {
  pending_candidates_.push_back(candidate);

  if (!transport_infos_timer_.IsRunning()) {
    // Delay sending the new candidates in case we get more candidates
    // that we can send in one message.
    transport_infos_timer_.Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(kTransportInfoSendDelayMs),
        this, &PepperSession::SendTransportInfo);
  }
}

void PepperSession::OnTransportDeleted(Transport* transport) {
  ChannelsMap::iterator it = channels_.find(transport->name());
  DCHECK_EQ(it->second, transport);
  channels_.erase(it);
}

void PepperSession::OnIncomingMessage(const JingleMessage& message,
                                      const ReplyCallback& reply_callback) {
  DCHECK(CalledOnValidThread());

  if (message.from != peer_jid_) {
    // Ignore messages received from a different Jid.
    reply_callback.Run(JingleMessageReply::INVALID_SID);
    return;
  }

  switch (message.action) {
    case JingleMessage::SESSION_ACCEPT:
      OnAccept(message, reply_callback);
      break;

    case JingleMessage::SESSION_INFO:
      OnSessionInfo(message, reply_callback);
      break;

    case JingleMessage::TRANSPORT_INFO:
      reply_callback.Run(JingleMessageReply::NONE);
      ProcessTransportInfo(message);
      break;

    case JingleMessage::SESSION_TERMINATE:
      OnTerminate(message, reply_callback);
      break;

    default:
      reply_callback.Run(JingleMessageReply::UNEXPECTED_REQUEST);
  }
}

void PepperSession::OnAccept(const JingleMessage& message,
                             const ReplyCallback& reply_callback) {
  if (state_ != CONNECTING) {
    reply_callback.Run(JingleMessageReply::UNEXPECTED_REQUEST);
    return;
  }

  reply_callback.Run(JingleMessageReply::NONE);

  const buzz::XmlElement* auth_message =
      message.description->authenticator_message();
  if (!auth_message) {
    DLOG(WARNING) << "Received session-accept without authentication message "
                  << auth_message->Str();
    CloseInternal(INCOMPATIBLE_PROTOCOL);
    return;
  }

  DCHECK(authenticator_->state() == Authenticator::WAITING_MESSAGE);
  authenticator_->ProcessMessage(auth_message);

  if (!InitializeConfigFromDescription(message.description.get())) {
    CloseInternal(INCOMPATIBLE_PROTOCOL);
    return;
  }

  // In case there is transport information in the accept message.
  ProcessTransportInfo(message);

  SetState(CONNECTED);

  // Process authentication.
  if (authenticator_->state() == Authenticator::ACCEPTED) {
    SetState(AUTHENTICATED);
  } else {
    ProcessAuthenticationStep();
  }
}

void PepperSession::OnSessionInfo(const JingleMessage& message,
                                  const ReplyCallback& reply_callback) {
  if (!message.info.get() ||
      !Authenticator::IsAuthenticatorMessage(message.info.get())) {
    reply_callback.Run(JingleMessageReply::UNSUPPORTED_INFO);
    return;
  }

  if (state_ != CONNECTED ||
      authenticator_->state() != Authenticator::WAITING_MESSAGE) {
    LOG(WARNING) << "Received unexpected authenticator message "
                 << message.info->Str();
    reply_callback.Run(JingleMessageReply::UNEXPECTED_REQUEST);
    CloseInternal(INCOMPATIBLE_PROTOCOL);
    return;
  }

  reply_callback.Run(JingleMessageReply::NONE);

  authenticator_->ProcessMessage(message.info.get());
  ProcessAuthenticationStep();
}

void PepperSession::ProcessTransportInfo(const JingleMessage& message) {
  for (std::list<cricket::Candidate>::const_iterator it =
           message.candidates.begin();
       it != message.candidates.end(); ++it) {
    ChannelsMap::iterator channel = channels_.find(it->name());
    if (channel == channels_.end()) {
      LOG(WARNING) << "Received candidate for unknown channel " << it->name();
      continue;
    }
    channel->second->AddRemoteCandidate(*it);
  }
}

void PepperSession::OnTerminate(const JingleMessage& message,
                                const ReplyCallback& reply_callback) {
  if (state_ != CONNECTING && state_ != CONNECTED && state_ != AUTHENTICATED) {
    LOG(WARNING) << "Received unexpected session-terminate message.";
    reply_callback.Run(JingleMessageReply::UNEXPECTED_REQUEST);
    return;
  }

  reply_callback.Run(JingleMessageReply::NONE);

  switch (message.reason) {
    case JingleMessage::SUCCESS:
      if (state_ == CONNECTING) {
        error_ = SESSION_REJECTED;
      } else {
        error_ = OK;
      }
      break;
    case JingleMessage::DECLINE:
      error_ = AUTHENTICATION_FAILED;
      break;
    case JingleMessage::GENERAL_ERROR:
      error_ = CHANNEL_CONNECTION_ERROR;
      break;
    case JingleMessage::INCOMPATIBLE_PARAMETERS:
      error_ = INCOMPATIBLE_PROTOCOL;
      break;
    default:
      error_ = UNKNOWN_ERROR;
  }

  if (error_ != OK) {
    SetState(FAILED);
  } else {
    SetState(CLOSED);
  }
}

bool PepperSession::InitializeConfigFromDescription(
    const ContentDescription* description) {
  DCHECK(description);

  if (!description->config()->GetFinalConfig(&config_)) {
    LOG(ERROR) << "session-accept does not specify configuration";
    return false;
  }
  if (!candidate_config()->IsSupported(config_)) {
    LOG(ERROR) << "session-accept specifies an invalid configuration";
    return false;
  }

  return true;
}

void PepperSession::ProcessAuthenticationStep() {
  DCHECK_EQ(state_, CONNECTED);

  if (authenticator_->state() == Authenticator::MESSAGE_READY) {
    JingleMessage message(peer_jid_, JingleMessage::SESSION_INFO, session_id_);
    message.info = authenticator_->GetNextMessage();
    DCHECK(message.info.get());

    session_info_request_.reset(session_manager_->iq_sender()->SendIq(
        message.ToXml(), base::Bind(
            &PepperSession::OnSessionInfoResponse,
            base::Unretained(this))));
  }
  DCHECK_NE(authenticator_->state(), Authenticator::MESSAGE_READY);

  if (authenticator_->state() == Authenticator::ACCEPTED) {
    SetState(AUTHENTICATED);
  } else if (authenticator_->state() == Authenticator::REJECTED) {
    CloseInternal(AuthRejectionReasonToError(
        authenticator_->rejection_reason()));
  }
}

void PepperSession::OnSessionInfoResponse(const buzz::XmlElement* response) {
  const std::string& type = response->Attr(buzz::QName("", "type"));
  if (type != "result") {
    LOG(ERROR) << "Received error in response to session-info message: \""
               << response->Str()
               << "\". Terminating the session.";
    CloseInternal(INCOMPATIBLE_PROTOCOL);
  }
}

void PepperSession::OnTransportInfoResponse(const buzz::XmlElement* response) {
  const std::string& type = response->Attr(buzz::QName("", "type"));
  if (type != "result") {
    LOG(ERROR) << "Received error in response to session-initiate message: \""
               << response->Str()
               << "\". Terminating the session.";

    if (state_ == CONNECTING) {
      CloseInternal(PEER_IS_OFFLINE);
    } else {
      // Host has disconnected without sending session-terminate message.
      CloseInternal(OK);
    }
  }
}

void PepperSession::SendTransportInfo() {
  JingleMessage message(peer_jid_, JingleMessage::TRANSPORT_INFO, session_id_);
  message.candidates.swap(pending_candidates_);
  transport_info_request_.reset(session_manager_->iq_sender()->SendIq(
      message.ToXml(), base::Bind(
          &PepperSession::OnTransportInfoResponse,
          base::Unretained(this))));
}


void PepperSession::CloseInternal(Error error) {
  DCHECK(CalledOnValidThread());

  if (state_ == CONNECTING || state_ == CONNECTED || state_ == AUTHENTICATED) {
    // Send session-terminate message with the appropriate error code.
    JingleMessage::Reason reason;
    switch (error) {
      case OK:
        reason = JingleMessage::SUCCESS;
        break;
      case SESSION_REJECTED:
      case AUTHENTICATION_FAILED:
        reason = JingleMessage::DECLINE;
        break;
      case INCOMPATIBLE_PROTOCOL:
        reason = JingleMessage::INCOMPATIBLE_PARAMETERS;
        break;
      default:
        reason = JingleMessage::GENERAL_ERROR;
    }

    JingleMessage message(peer_jid_, JingleMessage::SESSION_TERMINATE,
                          session_id_);
    message.reason = reason;
    scoped_ptr<IqRequest> terminate_request(
        session_manager_->iq_sender()->SendIq(
            message.ToXml(), IqSender::ReplyCallback()));
  }

  error_ = error;

  if (state_ != FAILED && state_ != CLOSED) {
    if (error != OK) {
      SetState(FAILED);
    } else {
      SetState(CLOSED);
    }
  }
}

void PepperSession::SetState(State new_state) {
  DCHECK(CalledOnValidThread());

  if (new_state != state_) {
    DCHECK_NE(state_, CLOSED);
    DCHECK_NE(state_, FAILED);

    state_ = new_state;
    if (!state_change_callback_.is_null())
      state_change_callback_.Run(new_state);
  }
}

}  // namespace protocol
}  // namespace remoting
