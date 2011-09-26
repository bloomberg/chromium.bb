// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/pepper_session.h"

#include "base/bind.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "remoting/base/constants.h"
#include "remoting/jingle_glue/iq_request.h"
#include "remoting/protocol/content_description.h"
#include "remoting/protocol/jingle_messages.h"
#include "remoting/protocol/pepper_session_manager.h"
#include "remoting/protocol/pepper_stream_channel.h"
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
}  // namespace

PepperSession::PepperSession(PepperSessionManager* session_manager)
    : session_manager_(session_manager),
      state_(INITIALIZING),
      error_(ERROR_NO_ERROR) {
}

PepperSession::~PepperSession() {
  control_channel_socket_.reset();
  event_channel_socket_.reset();
  STLDeleteContainerPairSecondPointers(channels_.begin(), channels_.end());
  session_manager_->SessionDestroyed(this);
}

PepperSession::Error PepperSession::error() {
  DCHECK(CalledOnValidThread());
  return error_;
}

void PepperSession::SetStateChangeCallback(StateChangeCallback* callback) {
  DCHECK(CalledOnValidThread());
  state_change_callback_.reset(callback);
}

void PepperSession::StartConnection(
    const std::string& peer_jid,
    const std::string& peer_public_key,
    const std::string& client_token,
    CandidateSessionConfig* config,
    Session::StateChangeCallback* state_change_callback) {
  DCHECK(CalledOnValidThread());

  peer_jid_ = peer_jid;
  peer_public_key_ = peer_public_key;
  initiator_token_ = client_token;
  candidate_config_.reset(config);
  state_change_callback_.reset(state_change_callback);

  // Generate random session ID. There are usually not more than 1
  // concurrent session per host, so a random 64-bit integer provides
  // enough entropy. In the worst case connection will fail when two
  // clients generate the same session ID concurrently.
  session_id_ = base::Int64ToString(base::RandGenerator(kint64max));

  // Send session-initiate message.
  JingleMessage message(peer_jid_, JingleMessage::SESSION_INITIATE,
                        session_id_);
  message.from = session_manager_->local_jid_;
  message.description.reset(
      new ContentDescription(candidate_config_->Clone(), initiator_token_, ""));
  initiate_request_.reset(session_manager_->CreateIqRequest());
  initiate_request_->set_callback(base::Bind(
      &PepperSession::OnSessionInitiateResponse, base::Unretained(this)));
  initiate_request_->SendIq(message.ToXml());

  SetState(CONNECTING);
}

void PepperSession::OnSessionInitiateResponse(
    const buzz::XmlElement* response) {
  const std::string& type = response->Attr(buzz::QName("", "type"));
  if (type != "result") {
    LOG(ERROR) << "Received error in response to session-initiate message: \""
               << response->Str()
               << "\" Terminating the session.";

    // TODO(sergeyu): There may be different reasons for error
    // here. Parse the response stanza to find failure reason.
    OnError(ERROR_PEER_IS_OFFLINE);
  }
}

void PepperSession::OnError(Error error) {
  error_ = error;
  CloseInternal(true);
}

void PepperSession::CreateStreamChannel(
      const std::string& name,
      const StreamChannelCallback& callback) {
  DCHECK(!channels_[name]);

  PepperStreamChannel* channel = new PepperStreamChannel(this, name, callback);
  channels_[name] = channel;
  channel->Connect(session_manager_->pp_instance_,
                   session_manager_->transport_config_, remote_cert_);
}

void PepperSession::CreateDatagramChannel(
    const std::string& name,
    const DatagramChannelCallback& callback) {
  // TODO(sergeyu): Implement datagram channel support.
  NOTREACHED();
}

net::Socket* PepperSession::control_channel() {
  DCHECK(CalledOnValidThread());
  return control_channel_socket_.get();
}

net::Socket* PepperSession::event_channel() {
  DCHECK(CalledOnValidThread());
  return event_channel_socket_.get();
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
  // set_config() should never be called on the client.
  NOTREACHED();
}

const std::string& PepperSession::initiator_token() {
  DCHECK(CalledOnValidThread());
  return initiator_token_;
}

void PepperSession::set_initiator_token(const std::string& initiator_token) {
  DCHECK(CalledOnValidThread());
  initiator_token_ = initiator_token;
}

const std::string& PepperSession::receiver_token() {
  DCHECK(CalledOnValidThread());
  return receiver_token_;
}

void PepperSession::set_receiver_token(const std::string& receiver_token) {
  DCHECK(CalledOnValidThread());
  // set_receiver_token() should not be called on the client side.
  NOTREACHED();
}

void PepperSession::set_shared_secret(const std::string& secret) {
  DCHECK(CalledOnValidThread());
  shared_secret_ = secret;
}

const std::string& PepperSession::shared_secret() {
  DCHECK(CalledOnValidThread());
  return shared_secret_;
}

void PepperSession::Close() {
  DCHECK(CalledOnValidThread());

  if (state_ == CONNECTING || state_ == CONNECTED ||
      state_ == CONNECTED_CHANNELS) {
    // Send session-terminate message.
    JingleMessage message(peer_jid_, JingleMessage::SESSION_TERMINATE,
                          session_id_);
    scoped_ptr<IqRequest>  terminate_request(
        session_manager_->CreateIqRequest());
    terminate_request->SendIq(message.ToXml());
  }

  CloseInternal(false);
}

void PepperSession::OnIncomingMessage(const JingleMessage& message,
                                      JingleMessageReply* reply) {
  DCHECK(CalledOnValidThread());

  if (message.from != peer_jid_) {
    // Ignore messages received from a different Jid.
    *reply = JingleMessageReply(JingleMessageReply::INVALID_SID);
    return;
  }

  switch (message.action) {
    case JingleMessage::SESSION_ACCEPT:
      OnAccept(message, reply);
      break;

    case JingleMessage::TRANSPORT_INFO:
      ProcessTransportInfo(message);
      break;

    case JingleMessage::SESSION_TERMINATE:
      OnTerminate(message, reply);
      break;

    default:
      *reply = JingleMessageReply(JingleMessageReply::UNEXPECTED_REQUEST);
  }
}

void PepperSession::OnAccept(const JingleMessage& message,
                             JingleMessageReply* reply) {
  if (state_ != CONNECTING) {
    *reply = JingleMessageReply(JingleMessageReply::UNEXPECTED_REQUEST);
    return;
  }

  if (!InitializeConfigFromDescription(message.description.get())) {
    OnError(ERROR_INCOMPATIBLE_PROTOCOL);
    return;
  }

  CreateChannels();
  SetState(CONNECTED);

   // In case there is transport information in the accept message.
  ProcessTransportInfo(message);
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
    channel->second->AddRemoveCandidate(*it);
  }
}

void PepperSession::OnTerminate(const JingleMessage& message,
                                JingleMessageReply* reply) {
  if (state_ == CONNECTING) {
    OnError(ERROR_SESSION_REJECTED);
    return;
  }

  CloseInternal(false);
}

bool PepperSession::InitializeConfigFromDescription(
    const ContentDescription* description) {
  DCHECK(description);

  remote_cert_ = description->certificate();
  if (remote_cert_.empty()) {
    LOG(ERROR) << "session-accept does not specify certificate";
    return false;
  }

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

void PepperSession::AddLocalCandidate(const cricket::Candidate& candidate) {
  pending_candidates_.push_back(candidate);

  if (!transport_infos_timer_.IsRunning()) {
    // Delay sending the new candidates in case we get more candidates
    // that we can send in one message.
    transport_infos_timer_.Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(kTransportInfoSendDelayMs),
        this, &PepperSession::SendTransportInfo);
  }
}

void PepperSession::OnDeleteChannel(PepperChannel* channel) {
  ChannelsMap::iterator it = channels_.find(channel->name());
  DCHECK_EQ(it->second, channel);
  channels_.erase(it);
}

void PepperSession::SendTransportInfo() {
  JingleMessage message(peer_jid_, JingleMessage::TRANSPORT_INFO, session_id_);
  message.candidates.swap(pending_candidates_);
  scoped_ptr<IqRequest> request(session_manager_->CreateIqRequest());
  request->SendIq(message.ToXml());
}

void PepperSession::CreateChannels() {
   CreateStreamChannel(
      kControlChannelName,
      base::Bind(&PepperSession::OnChannelConnected,
                 base::Unretained(this), &control_channel_socket_));
  CreateStreamChannel(
      kEventChannelName,
      base::Bind(&PepperSession::OnChannelConnected,
                 base::Unretained(this), &event_channel_socket_));
}

void PepperSession::OnChannelConnected(
    scoped_ptr<net::Socket>* socket_container,
    net::StreamSocket* socket) {
  if (!socket) {
    LOG(ERROR) << "Failed to connect control or events channel. "
               << "Terminating connection";
    OnError(ERROR_CHANNEL_CONNECTION_FAILURE);
    return;
  }

  socket_container->reset(socket);

  if (control_channel_socket_.get() && event_channel_socket_.get())
    SetState(CONNECTED_CHANNELS);
}

void PepperSession::CloseInternal(bool failed) {
  DCHECK(CalledOnValidThread());

  if (state_ != FAILED && state_ != CLOSED) {
    control_channel_socket_.reset();
    event_channel_socket_.reset();

    if (failed)
      SetState(FAILED);
    else
      SetState(CLOSED);
  }
}

void PepperSession::SetState(State new_state) {
  DCHECK(CalledOnValidThread());

  if (new_state != state_) {
    DCHECK_NE(state_, CLOSED);
    DCHECK_NE(state_, FAILED);

    state_ = new_state;
    if (state_change_callback_.get())
      state_change_callback_->Run(new_state);
  }
}

}  // namespace protocol
}  // namespace remoting
