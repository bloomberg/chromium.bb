// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/jingle_session.h"

#include <stdint.h>

#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/content_description.h"
#include "remoting/protocol/jingle_messages.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/session_config.h"
#include "remoting/protocol/transport.h"
#include "remoting/signaling/iq_sender.h"
#include "third_party/webrtc/libjingle/xmllite/xmlelement.h"
#include "third_party/webrtc/p2p/base/candidate.h"

using buzz::XmlElement;

namespace remoting {
namespace protocol {

namespace {

// How long we should wait for a response from the other end. This value is used
// for all requests except |transport-info|.
const int kDefaultMessageTimeout = 10;

// During a reconnection, it usually takes longer for the peer to respond due to
// pending messages in the channel from the previous session.  From experiment,
// it can take up to 20s for the session to reconnect. To make it safe, setting
// the timeout to 30s.
const int kSessionInitiateAndAcceptTimeout = kDefaultMessageTimeout * 3;

// Timeout for the transport-info messages.
const int kTransportInfoTimeout = 10 * 60;

ErrorCode AuthRejectionReasonToErrorCode(
    Authenticator::RejectionReason reason) {
  switch (reason) {
    case Authenticator::INVALID_CREDENTIALS:
      return AUTHENTICATION_FAILED;
    case Authenticator::PROTOCOL_ERROR:
      return INCOMPATIBLE_PROTOCOL;
    case Authenticator::INVALID_ACCOUNT:
      return INVALID_ACCOUNT;
  }
  NOTREACHED();
  return UNKNOWN_ERROR;
}

}  // namespace

JingleSession::JingleSession(JingleSessionManager* session_manager)
    : session_manager_(session_manager),
      event_handler_(nullptr),
      state_(INITIALIZING),
      error_(OK),
      weak_factory_(this) {
}

JingleSession::~JingleSession() {
  STLDeleteContainerPointers(pending_requests_.begin(),
                             pending_requests_.end());
  STLDeleteContainerPointers(transport_info_requests_.begin(),
                             transport_info_requests_.end());

  session_manager_->SessionDestroyed(this);
}

void JingleSession::SetEventHandler(Session::EventHandler* event_handler) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(event_handler);
  event_handler_ = event_handler;
}

ErrorCode JingleSession::error() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return error_;
}

void JingleSession::StartConnection(
    const std::string& peer_jid,
    std::unique_ptr<Authenticator> authenticator) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(authenticator.get());
  DCHECK_EQ(authenticator->state(), Authenticator::MESSAGE_READY);

  peer_address_ = SignalingAddress(peer_jid);
  authenticator_ = std::move(authenticator);

  // Generate random session ID. There are usually not more than 1
  // concurrent session per host, so a random 64-bit integer provides
  // enough entropy. In the worst case connection will fail when two
  // clients generate the same session ID concurrently.
  session_id_ = base::Uint64ToString(
      base::RandGenerator(std::numeric_limits<uint64_t>::max()));

  // Send session-initiate message.
  JingleMessage message(peer_address_, JingleMessage::SESSION_INITIATE,
                        session_id_);
  message.initiator = session_manager_->signal_strategy_->GetLocalJid();
  message.description.reset(new ContentDescription(
      session_manager_->protocol_config_->Clone(),
      authenticator_->GetNextMessage()));
  SendMessage(message);

  SetState(CONNECTING);
}

void JingleSession::InitializeIncomingConnection(
    const JingleMessage& initiate_message,
    std::unique_ptr<Authenticator> authenticator) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(initiate_message.description.get());
  DCHECK(authenticator.get());
  DCHECK_EQ(authenticator->state(), Authenticator::WAITING_MESSAGE);

  peer_address_ = initiate_message.from;
  authenticator_ = std::move(authenticator);
  session_id_ = initiate_message.sid;

  SetState(ACCEPTING);

  config_ =
      SessionConfig::SelectCommon(initiate_message.description->config(),
                                  session_manager_->protocol_config_.get());
  if (!config_) {
    LOG(WARNING) << "Rejecting connection from " << peer_address_.id()
                 << " because no compatible configuration has been found.";
    Close(INCOMPATIBLE_PROTOCOL);
    return;
  }
}

void JingleSession::AcceptIncomingConnection(
    const JingleMessage& initiate_message) {
  DCHECK(config_);

  // Process the first authentication message.
  const buzz::XmlElement* first_auth_message =
      initiate_message.description->authenticator_message();

  if (!first_auth_message) {
    Close(INCOMPATIBLE_PROTOCOL);
    return;
  }

  DCHECK_EQ(authenticator_->state(), Authenticator::WAITING_MESSAGE);
  // |authenticator_| is owned, so Unretained() is safe here.
  authenticator_->ProcessMessage(first_auth_message, base::Bind(
      &JingleSession::ContinueAcceptIncomingConnection,
      base::Unretained(this)));
}

void JingleSession::ContinueAcceptIncomingConnection() {
  DCHECK_NE(authenticator_->state(), Authenticator::PROCESSING_MESSAGE);
  if (authenticator_->state() == Authenticator::REJECTED) {
    Close(AuthRejectionReasonToErrorCode(authenticator_->rejection_reason()));
    return;
  }

  // Send the session-accept message.
  JingleMessage message(peer_address_, JingleMessage::SESSION_ACCEPT,
                        session_id_);

  std::unique_ptr<buzz::XmlElement> auth_message;
  if (authenticator_->state() == Authenticator::MESSAGE_READY)
    auth_message = authenticator_->GetNextMessage();

  message.description.reset(new ContentDescription(
      CandidateSessionConfig::CreateFrom(*config_), std::move(auth_message)));
  SendMessage(message);

  // Update state.
  SetState(ACCEPTED);

  if (authenticator_->state() == Authenticator::ACCEPTED) {
    OnAuthenticated();
  } else {
    DCHECK_EQ(authenticator_->state(), Authenticator::WAITING_MESSAGE);
    if (authenticator_->started()) {
      SetState(AUTHENTICATING);
    }
  }
}

const std::string& JingleSession::jid() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return peer_address_.id();
}

const SessionConfig& JingleSession::config() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return *config_;
}

void JingleSession::SetTransport(Transport* transport) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!transport_);
  DCHECK(transport);
  transport_ = transport;
}

void JingleSession::SendTransportInfo(
    std::unique_ptr<buzz::XmlElement> transport_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, AUTHENTICATED);

  JingleMessage message(peer_address_, JingleMessage::TRANSPORT_INFO,
                        session_id_);
  message.transport_info = std::move(transport_info);

  std::unique_ptr<IqRequest> request = session_manager_->iq_sender()->SendIq(
      message.ToXml(), base::Bind(&JingleSession::OnTransportInfoResponse,
                                  base::Unretained(this)));
  if (request) {
    request->SetTimeout(base::TimeDelta::FromSeconds(kTransportInfoTimeout));
    transport_info_requests_.push_back(request.release());
  } else {
    LOG(ERROR) << "Failed to send a transport-info message";
  }
}

void JingleSession::Close(protocol::ErrorCode error) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (is_session_active()) {
    // Send session-terminate message with the appropriate error code.
    JingleMessage::Reason reason;
    switch (error) {
      case OK:
        reason = JingleMessage::SUCCESS;
        break;
      case SESSION_REJECTED:
      case AUTHENTICATION_FAILED:
      case INVALID_ACCOUNT:
        reason = JingleMessage::DECLINE;
        break;
      case INCOMPATIBLE_PROTOCOL:
        reason = JingleMessage::INCOMPATIBLE_PARAMETERS;
        break;
      case HOST_OVERLOAD:
        reason = JingleMessage::CANCEL;
        break;
      case MAX_SESSION_LENGTH:
        reason = JingleMessage::EXPIRED;
        break;
      case HOST_CONFIGURATION_ERROR:
        reason = JingleMessage::FAILED_APPLICATION;
        break;
      default:
        reason = JingleMessage::GENERAL_ERROR;
    }

    JingleMessage message(peer_address_, JingleMessage::SESSION_TERMINATE,
                          session_id_);
    message.reason = reason;
    message.error_code = error;
    SendMessage(message);
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

void JingleSession::SendMessage(const JingleMessage& message) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::unique_ptr<IqRequest> request = session_manager_->iq_sender()->SendIq(
      message.ToXml(), base::Bind(&JingleSession::OnMessageResponse,
                                  base::Unretained(this), message.action));

  int timeout = kDefaultMessageTimeout;
  if (message.action == JingleMessage::SESSION_INITIATE ||
      message.action == JingleMessage::SESSION_ACCEPT) {
    timeout = kSessionInitiateAndAcceptTimeout;
  }
  if (request) {
    request->SetTimeout(base::TimeDelta::FromSeconds(timeout));
    pending_requests_.insert(request.release());
  } else {
    LOG(ERROR) << "Failed to send a "
               << JingleMessage::GetActionName(message.action) << " message";
  }
}

void JingleSession::OnMessageResponse(
    JingleMessage::ActionType request_type,
    IqRequest* request,
    const buzz::XmlElement* response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Delete the request from the list of pending requests.
  pending_requests_.erase(request);
  delete request;

  // Ignore all responses after session was closed.
  if (state_ == CLOSED || state_ == FAILED)
    return;

  std::string type_str = JingleMessage::GetActionName(request_type);

  // |response| will be nullptr if the request timed out.
  if (!response) {
    LOG(ERROR) << type_str << " request timed out.";
    Close(SIGNALING_TIMEOUT);
    return;
  } else {
    const std::string& type =
        response->Attr(buzz::QName(std::string(), "type"));
    if (type != "result") {
      LOG(ERROR) << "Received error in response to " << type_str
                 << " message: \"" << response->Str()
                 << "\". Terminating the session.";

      // TODO(sergeyu): There may be different reasons for error
      // here. Parse the response stanza to find failure reason.
      Close(PEER_IS_OFFLINE);
    }
  }
}

void JingleSession::OnTransportInfoResponse(IqRequest* request,
                                            const buzz::XmlElement* response) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!transport_info_requests_.empty());

  // Consider transport-info requests sent before this one lost and delete
  // corresponding IqRequest objects.
  while (transport_info_requests_.front() != request) {
    delete transport_info_requests_.front();
    transport_info_requests_.pop_front();
  }

  // Delete the |request| itself.
  DCHECK_EQ(request, transport_info_requests_.front());
  delete request;
  transport_info_requests_.pop_front();

  // Ignore transport-info timeouts.
  if (!response) {
    LOG(ERROR) << "transport-info request has timed out.";
    return;
  }

  const std::string& type = response->Attr(buzz::QName(std::string(), "type"));
  if (type != "result") {
    LOG(ERROR) << "Received error in response to transport-info message: \""
               << response->Str() << "\". Terminating the session.";
    Close(PEER_IS_OFFLINE);
  }
}

void JingleSession::OnIncomingMessage(const JingleMessage& message,
                                      const ReplyCallback& reply_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (peer_address_ != message.from) {
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
      if (!transport_) {
        LOG(ERROR) << "Received unexpected transport-info message.";
        reply_callback.Run(JingleMessageReply::NONE);
        return;
      }

      if (!message.transport_info ||
          !transport_->ProcessTransportInfo(
              message.transport_info.get())) {
        reply_callback.Run(JingleMessageReply::BAD_REQUEST);
        return;
      }

      reply_callback.Run(JingleMessageReply::NONE);
      break;

    case JingleMessage::SESSION_TERMINATE:
      OnTerminate(message, reply_callback);
      break;

    default:
      reply_callback.Run(JingleMessageReply::UNEXPECTED_REQUEST);
  }
}

void JingleSession::OnAccept(const JingleMessage& message,
                             const ReplyCallback& reply_callback) {
  if (state_ != CONNECTING) {
    reply_callback.Run(JingleMessageReply::UNEXPECTED_REQUEST);
    return;
  }

  reply_callback.Run(JingleMessageReply::NONE);

  const buzz::XmlElement* auth_message =
      message.description->authenticator_message();
  if (!auth_message) {
    DLOG(WARNING) << "Received session-accept without authentication message ";
    Close(INCOMPATIBLE_PROTOCOL);
    return;
  }

  if (!InitializeConfigFromDescription(message.description.get())) {
    Close(INCOMPATIBLE_PROTOCOL);
    return;
  }

  SetState(ACCEPTED);

  DCHECK(authenticator_->state() == Authenticator::WAITING_MESSAGE);
  authenticator_->ProcessMessage(auth_message, base::Bind(
      &JingleSession::ProcessAuthenticationStep,base::Unretained(this)));
}

void JingleSession::OnSessionInfo(const JingleMessage& message,
                                  const ReplyCallback& reply_callback) {
  if (!message.info.get() ||
      !Authenticator::IsAuthenticatorMessage(message.info.get())) {
    reply_callback.Run(JingleMessageReply::UNSUPPORTED_INFO);
    return;
  }

  if ((state_ != ACCEPTED && state_ != AUTHENTICATING) ||
      authenticator_->state() != Authenticator::WAITING_MESSAGE) {
    LOG(WARNING) << "Received unexpected authenticator message "
                 << message.info->Str();
    reply_callback.Run(JingleMessageReply::UNEXPECTED_REQUEST);
    Close(INCOMPATIBLE_PROTOCOL);
    return;
  }

  reply_callback.Run(JingleMessageReply::NONE);

  authenticator_->ProcessMessage(message.info.get(), base::Bind(
      &JingleSession::ProcessAuthenticationStep, base::Unretained(this)));
}

void JingleSession::OnTerminate(const JingleMessage& message,
                                const ReplyCallback& reply_callback) {
  if (!is_session_active()) {
    LOG(WARNING) << "Received unexpected session-terminate message.";
    reply_callback.Run(JingleMessageReply::UNEXPECTED_REQUEST);
    return;
  }

  reply_callback.Run(JingleMessageReply::NONE);

  error_ = message.error_code;
  if (error_ == UNKNOWN_ERROR) {
    // get error code from message.reason for compatibility with older versions
    // that do not add <error-code>.
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
      case JingleMessage::CANCEL:
        error_ = HOST_OVERLOAD;
        break;
      case JingleMessage::EXPIRED:
        error_ = MAX_SESSION_LENGTH;
        break;
      case JingleMessage::INCOMPATIBLE_PARAMETERS:
        error_ = INCOMPATIBLE_PROTOCOL;
        break;
      case JingleMessage::FAILED_APPLICATION:
        error_ = HOST_CONFIGURATION_ERROR;
        break;
      case JingleMessage::GENERAL_ERROR:
        error_ = CHANNEL_CONNECTION_ERROR;
        break;
      default:
        error_ = UNKNOWN_ERROR;
    }
  } else if (error_ == SESSION_REJECTED) {
    // For backward compatibility, we still use AUTHENTICATION_FAILED for
    // SESSION_REJECTED error.
    // TODO(zijiehe): Handle SESSION_REJECTED error in WebApp. Tracked by
    // http://crbug.com/618036.
    error_ = AUTHENTICATION_FAILED;
  }

  if (error_ != OK) {
    SetState(FAILED);
  } else {
    SetState(CLOSED);
  }
}

bool JingleSession::InitializeConfigFromDescription(
    const ContentDescription* description) {
  DCHECK(description);
  config_ = SessionConfig::GetFinalConfig(description->config());
  if (!config_) {
    LOG(ERROR) << "session-accept does not specify configuration";
    return false;
  }
  if (!session_manager_->protocol_config_->IsSupported(*config_)) {
    LOG(ERROR) << "session-accept specifies an invalid configuration";
    return false;
  }

  return true;
}

void JingleSession::ProcessAuthenticationStep() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(authenticator_->state(), Authenticator::PROCESSING_MESSAGE);

  if (state_ != ACCEPTED && state_ != AUTHENTICATING) {
    DCHECK(state_ == FAILED || state_ == CLOSED);
    // The remote host closed the connection while the authentication was being
    // processed asynchronously, nothing to do.
    return;
  }

  if (authenticator_->state() == Authenticator::MESSAGE_READY) {
    JingleMessage message(peer_address_, JingleMessage::SESSION_INFO,
                          session_id_);
    message.info = authenticator_->GetNextMessage();
    DCHECK(message.info.get());
    SendMessage(message);
  }
  DCHECK_NE(authenticator_->state(), Authenticator::MESSAGE_READY);

  // The current JingleSession object can be destroyed by event_handler of
  // SetState(AUTHENTICATING) and cause subsequent dereferencing of the this
  // pointer to crash.  To protect against it, we run ContinueAuthenticationStep
  // asychronously using a weak pointer.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
    FROM_HERE,
    base::Bind(&JingleSession::ContinueAuthenticationStep,
               weak_factory_.GetWeakPtr()));

  if (authenticator_->started()) {
    SetState(AUTHENTICATING);
  }
}

void JingleSession::ContinueAuthenticationStep() {
  if (authenticator_->state() == Authenticator::ACCEPTED) {
    OnAuthenticated();
  } else if (authenticator_->state() == Authenticator::REJECTED) {
    Close(AuthRejectionReasonToErrorCode(
        authenticator_->rejection_reason()));
  }
}

void JingleSession::OnAuthenticated() {
  transport_->Start(authenticator_.get(),
                    base::Bind(&JingleSession::SendTransportInfo,
                               weak_factory_.GetWeakPtr()));

  SetState(AUTHENTICATED);
}

void JingleSession::SetState(State new_state) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (new_state != state_) {
    DCHECK_NE(state_, CLOSED);
    DCHECK_NE(state_, FAILED);

    state_ = new_state;
    if (event_handler_)
      event_handler_->OnSessionStateChange(new_state);
  }
}

bool JingleSession::is_session_active() {
  return state_ == CONNECTING || state_ == ACCEPTING || state_ == ACCEPTED ||
        state_ == AUTHENTICATING || state_ == AUTHENTICATED;
}

}  // namespace protocol
}  // namespace remoting
