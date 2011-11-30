// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/jingle_session.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "crypto/hmac.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/auth_util.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/jingle_datagram_connector.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/jingle_stream_connector.h"
#include "third_party/libjingle/source/talk/base/thread.h"
#include "third_party/libjingle/source/talk/p2p/base/p2ptransportchannel.h"
#include "third_party/libjingle/source/talk/p2p/base/session.h"
#include "third_party/libjingle/source/talk/p2p/base/transport.h"

using cricket::BaseSession;

namespace remoting {
namespace protocol {

JingleSession::JingleSession(
    JingleSessionManager* jingle_session_manager,
    cricket::Session* cricket_session,
    Authenticator* authenticator)
    : jingle_session_manager_(jingle_session_manager),
      authenticator_(authenticator),
      state_(INITIALIZING),
      error_(OK),
      closing_(false),
      cricket_session_(cricket_session),
      config_set_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)) {
  jid_ = cricket_session_->remote_name();
  cricket_session_->SignalState.connect(this, &JingleSession::OnSessionState);
  cricket_session_->SignalError.connect(this, &JingleSession::OnSessionError);
}

JingleSession::~JingleSession() {
  // Reset the callback so that it's not called from Close().
  state_change_callback_.Reset();
  Close();
  jingle_session_manager_->SessionDestroyed(this);
  DCHECK(channel_connectors_.empty());
}

void JingleSession::SendSessionInitiate() {
  DCHECK_EQ(authenticator_->state(), Authenticator::MESSAGE_READY);
  cricket_session_->Initiate(
      jid_, CreateSessionDescription(candidate_config()->Clone(),
                                     authenticator_->GetNextMessage()));
}

void JingleSession::CloseInternal(int result, Error error) {
  DCHECK(CalledOnValidThread());

  if (state_ != FAILED && state_ != CLOSED && !closing_) {
    closing_ = true;

    // Tear down the cricket session, including the cricket transport channels.
    if (cricket_session_) {
      std::string reason;
      switch (error) {
        case OK:
          reason = cricket::STR_TERMINATE_SUCCESS;
          break;
        case SESSION_REJECTED:
        case AUTHENTICATION_FAILED:
          reason = cricket::STR_TERMINATE_DECLINE;
          break;
        case INCOMPATIBLE_PROTOCOL:
          reason = cricket::STR_TERMINATE_INCOMPATIBLE_PARAMETERS;
          break;
        default:
          reason = cricket::STR_TERMINATE_ERROR;
      }
      cricket_session_->TerminateWithReason(reason);
      cricket_session_->SignalState.disconnect(this);
    }

    error_ = error;

    // Inform the StateChangeCallback, so calling code knows not to
    // touch any channels. Needs to be done in the end because the
    // session may be deleted in response to this event.
    if (error != OK) {
      SetState(FAILED);
    } else {
      SetState(CLOSED);
    }
  }
}

bool JingleSession::HasSession(cricket::Session* cricket_session) {
  DCHECK(CalledOnValidThread());
  return cricket_session_ == cricket_session;
}

cricket::Session* JingleSession::ReleaseSession() {
  DCHECK(CalledOnValidThread());

  // Session may be destroyed only after it is closed.
  DCHECK(state_ == FAILED || state_ == CLOSED);

  cricket::Session* session = cricket_session_;
  if (cricket_session_)
    cricket_session_->SignalState.disconnect(this);
  cricket_session_ = NULL;
  return session;
}

void JingleSession::SetStateChangeCallback(
    const StateChangeCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!callback.is_null());
  state_change_callback_ = callback;
}

Session::Error JingleSession::error() {
  DCHECK(CalledOnValidThread());
  return error_;
}

void JingleSession::CreateStreamChannel(
      const std::string& name, const StreamChannelCallback& callback) {
  DCHECK(CalledOnValidThread());

  AddChannelConnector(
      name, new JingleStreamConnector(this, name, callback));
}

void JingleSession::CreateDatagramChannel(
    const std::string& name, const DatagramChannelCallback& callback) {
  DCHECK(CalledOnValidThread());

  AddChannelConnector(
      name, new JingleDatagramConnector(this, name, callback));
}

void JingleSession::CancelChannelCreation(const std::string& name) {
  ChannelConnectorsMap::iterator it = channel_connectors_.find(name);
  if (it != channel_connectors_.end()) {
    delete it->second;
    channel_connectors_.erase(it);
  }
}

const std::string& JingleSession::jid() {
  DCHECK(CalledOnValidThread());
  return jid_;
}

const CandidateSessionConfig* JingleSession::candidate_config() {
  DCHECK(CalledOnValidThread());
  DCHECK(candidate_config_.get());
  return candidate_config_.get();
}

void JingleSession::set_candidate_config(
    const CandidateSessionConfig* candidate_config) {
  DCHECK(CalledOnValidThread());
  DCHECK(!candidate_config_.get());
  DCHECK(candidate_config);
  candidate_config_.reset(candidate_config);
}

const SessionConfig& JingleSession::config() {
  DCHECK(CalledOnValidThread());
  DCHECK(config_set_);
  return config_;
}

void JingleSession::set_config(const SessionConfig& config) {
  DCHECK(CalledOnValidThread());
  DCHECK(!config_set_);
  config_ = config;
  config_set_ = true;
}

void JingleSession::Close() {
  DCHECK(CalledOnValidThread());

  CloseInternal(net::ERR_CONNECTION_CLOSED, OK);
}

void JingleSession::OnSessionState(
    BaseSession* session, BaseSession::State state) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(cricket_session_, session);

  if (state_ == FAILED || state_ == CLOSED) {
    // Don't do anything if we already closed.
    return;
  }

  switch (state) {
    case cricket::Session::STATE_SENTINITIATE:
    case cricket::Session::STATE_RECEIVEDINITIATE:
      OnInitiate();
      break;

    case cricket::Session::STATE_SENTACCEPT:
    case cricket::Session::STATE_RECEIVEDACCEPT:
      OnAccept();
      break;

    case cricket::Session::STATE_SENTTERMINATE:
    case cricket::Session::STATE_RECEIVEDTERMINATE:
    case cricket::Session::STATE_SENTREJECT:
    case cricket::Session::STATE_RECEIVEDREJECT:
      OnTerminate();
      break;

    case cricket::Session::STATE_DEINIT:
      // Close() must have been called before this.
      NOTREACHED();
      break;

    default:
      // We don't care about other steates.
      break;
  }
}

void JingleSession::OnSessionError(
    BaseSession* session, BaseSession::Error error) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(cricket_session_, session);

  if (error != cricket::Session::ERROR_NONE) {
    // TODO(sergeyu): Report different errors depending on |error|.
    CloseInternal(net::ERR_CONNECTION_ABORTED, CHANNEL_CONNECTION_ERROR);
  }
}

void JingleSession::OnInitiate() {
  DCHECK(CalledOnValidThread());
  jid_ = cricket_session_->remote_name();

  if (cricket_session_->initiator()) {
    // Set state to CONNECTING if this is an outgoing message. We need
    // to post this task because channel creation works only after we
    // return from this method. This is because
    // JingleChannelConnector::Connect() needs to call
    // set_incoming_only() on P2PTransportChannel, but
    // P2PTransportChannel is created only after we return from this
    // method.
    // TODO(sergeyu): Add set_incoming_only() in TransportChannelProxy.
    jingle_session_manager_->message_loop_->PostTask(
        FROM_HERE, task_factory_.NewRunnableMethod(
            &JingleSession::SetState, CONNECTING));
  } else {
    jingle_session_manager_->message_loop_->PostTask(
        FROM_HERE, task_factory_.NewRunnableMethod(
            &JingleSession::AcceptConnection));
  }
}

bool JingleSession::InitializeConfigFromDescription(
    const cricket::SessionDescription* description) {
  // We should only be called after ParseContent has succeeded, in which
  // case there will always be a Chromoting session configuration.
  const cricket::ContentInfo* content =
      description->FirstContentByType(kChromotingXmlNamespace);
  CHECK(content);
  const protocol::ContentDescription* content_description =
      static_cast<const protocol::ContentDescription*>(content->description);
  CHECK(content_description);

  // Process authenticator message.
  const buzz::XmlElement* auth_message =
      content_description->authenticator_message();
  if (!auth_message) {
    DLOG(WARNING) << "Received session-accept without authentication message "
                  << auth_message->Str();
    return false;
  }

  DCHECK(authenticator_->state() == Authenticator::WAITING_MESSAGE);
  authenticator_->ProcessMessage(auth_message);
  // Support for more than two auth message is not implemented yet.
  DCHECK(authenticator_->state() != Authenticator::WAITING_MESSAGE &&
         authenticator_->state() != Authenticator::MESSAGE_READY);

  if (authenticator_->state() != Authenticator::ACCEPTED) {
    return false;
  }

  SessionConfig config;
  if (!content_description->config()->GetFinalConfig(&config)) {
    LOG(ERROR) << "Connection response does not specify configuration";
    return false;
  }
  if (!candidate_config()->IsSupported(config)) {
    LOG(ERROR) << "Connection response specifies an invalid configuration";
    return false;
  }

  set_config(config);
  return true;
}

void JingleSession::OnAccept() {
  DCHECK(CalledOnValidThread());

  // If we initiated the session, store the candidate configuration that the
  // host responded with, to refer to later.
  if (cricket_session_->initiator()) {
    if (!InitializeConfigFromDescription(
            cricket_session_->remote_description())) {
      CloseInternal(net::ERR_CONNECTION_FAILED, INCOMPATIBLE_PROTOCOL);
      return;
    }
  }

  SetState(CONNECTED);
}

void JingleSession::OnTerminate() {
  DCHECK(CalledOnValidThread());
  CloseInternal(net::ERR_CONNECTION_ABORTED, OK);
}

void JingleSession::AcceptConnection() {
  SetState(CONNECTING);

  const cricket::SessionDescription* session_description =
      cricket_session_->remote_description();
  const cricket::ContentInfo* content =
      session_description->FirstContentByType(kChromotingXmlNamespace);

  CHECK(content);
  const ContentDescription* content_description =
      static_cast<const ContentDescription*>(content->description);
  candidate_config_.reset(content_description->config()->Clone());

  SessionManager::IncomingSessionResponse response =
      jingle_session_manager_->AcceptConnection(this);
  if (response != SessionManager::ACCEPT) {
    if (response == SessionManager::INCOMPATIBLE) {
      cricket_session_->TerminateWithReason(
          cricket::STR_TERMINATE_INCOMPATIBLE_PARAMETERS);
    } else {
      cricket_session_->TerminateWithReason(cricket::STR_TERMINATE_DECLINE);
    }
    Close();
    // Release session so that JingleSessionManager::SessionDestroyed()
    // doesn't try to call cricket::SessionManager::DestroySession() for it.
    ReleaseSession();
    delete this;
    return;
  }

  const buzz::XmlElement* auth_message =
      content_description->authenticator_message();
  if (!auth_message) {
    DLOG(WARNING) << "Received session-initiate without authenticator message.";
    CloseInternal(net::ERR_CONNECTION_FAILED, INCOMPATIBLE_PROTOCOL);
    return;
  }

  authenticator_.reset(
      jingle_session_manager_->CreateAuthenticator(jid(), auth_message));
  if (!authenticator_.get()) {
    CloseInternal(net::ERR_CONNECTION_FAILED, INCOMPATIBLE_PROTOCOL);
    return;
  }

  DCHECK(authenticator_->state() == Authenticator::WAITING_MESSAGE);
  authenticator_->ProcessMessage(auth_message);
  // Support for more than two auth message is not implemented yet.
  DCHECK(authenticator_->state() != Authenticator::WAITING_MESSAGE);
  if (authenticator_->state() == Authenticator::REJECTED) {
    CloseInternal(net::ERR_CONNECTION_FAILED, AUTHENTICATION_FAILED);
    return;
  }

  // Connection must be configured by the AcceptConnection() callback.
  CandidateSessionConfig* candidate_config =
      CandidateSessionConfig::CreateFrom(config());

  buzz::XmlElement* auth_reply = NULL;
  if (authenticator_->state() == Authenticator::MESSAGE_READY)
    auth_reply = authenticator_->GetNextMessage();
  DCHECK_EQ(authenticator_->state(), Authenticator::ACCEPTED);
  cricket_session_->Accept(
      CreateSessionDescription(candidate_config, auth_reply));
}

void JingleSession::AddChannelConnector(
    const std::string& name, JingleChannelConnector* connector) {
  DCHECK(channel_connectors_.find(name) == channel_connectors_.end());

  const std::string& content_name = GetContentInfo()->name;
  cricket::TransportChannel* raw_channel =
      cricket_session_->CreateChannel(content_name, name);

  if (!jingle_session_manager_->allow_nat_traversal_ &&
      !cricket_session_->initiator()) {
    // Don't make outgoing connections from the host to client when
    // NAT traversal is disabled.
    raw_channel->GetP2PChannel()->set_incoming_only(true);
  }

  channel_connectors_[name] = connector;
  ChannelAuthenticator* authenticator =
      authenticator_->CreateChannelAuthenticator();
  connector->Connect(authenticator, raw_channel);

  // Workaround bug in libjingle - it doesn't connect channels if they
  // are created after the session is accepted. See crbug.com/89384.
  // TODO(sergeyu): Fix the bug and remove this line.
  cricket_session_->GetTransport(content_name)->ConnectChannels();
}

void JingleSession::OnChannelConnectorFinished(
    const std::string& name, JingleChannelConnector* connector) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(channel_connectors_[name], connector);
  channel_connectors_.erase(name);
}

const cricket::ContentInfo* JingleSession::GetContentInfo() const {
  const cricket::SessionDescription* session_description;
  // If we initiate the session, we get to specify the content name. When
  // accepting one, the remote end specifies it.
  if (cricket_session_->initiator()) {
    session_description = cricket_session_->local_description();
  } else {
    session_description = cricket_session_->remote_description();
  }
  const cricket::ContentInfo* content =
      session_description->FirstContentByType(kChromotingXmlNamespace);
  CHECK(content);
  return content;
}

void JingleSession::SetState(State new_state) {
  DCHECK(CalledOnValidThread());

  if (new_state != state_) {
    DCHECK_NE(state_, CLOSED);
    DCHECK_NE(state_, FAILED);

    state_ = new_state;
    if (!state_change_callback_.is_null())
      state_change_callback_.Run(new_state);
  }
}

// static
cricket::SessionDescription* JingleSession::CreateSessionDescription(
    const CandidateSessionConfig* config,
    const buzz::XmlElement* authenticator_message) {
  cricket::SessionDescription* desc = new cricket::SessionDescription();
  desc->AddContent(
      ContentDescription::kChromotingContentName, kChromotingXmlNamespace,
      new ContentDescription(config, authenticator_message));
  return desc;
}

}  // namespace protocol
}  // namespace remoting
