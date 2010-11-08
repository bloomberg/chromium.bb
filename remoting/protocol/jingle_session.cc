// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/jingle_session.h"

#include "base/message_loop.h"
#include "net/base/net_errors.h"
#include "remoting/base/constants.h"
#include "remoting/jingle_glue/channel_socket_adapter.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "remoting/jingle_glue/stream_socket_adapter.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "third_party/libjingle/source/talk/base/thread.h"
#include "third_party/libjingle/source/talk/p2p/base/session.h"
#include "third_party/libjingle/source/talk/session/tunnel/pseudotcpchannel.h"

using cricket::BaseSession;
using cricket::PseudoTcpChannel;

namespace remoting {

namespace protocol {

namespace {
const char kControlChannelName[] = "control";
const char kEventChannelName[] = "event";
const char kVideoChannelName[] = "video";
const char kVideoRtpChannelName[] = "videortp";
const char kVideoRtcpChannelName[] = "videortcp";
}  // namespace

const char JingleSession::kChromotingContentName[] = "chromoting";

JingleSession::JingleSession(
    JingleSessionManager* jingle_session_manager)
    : jingle_session_manager_(jingle_session_manager),
      state_(INITIALIZING),
      closed_(false),
      cricket_session_(NULL),
      event_channel_(NULL),
      video_channel_(NULL) {
}

JingleSession::~JingleSession() {
  DCHECK(closed_);
}

void JingleSession::Init(cricket::Session* cricket_session) {
  DCHECK_EQ(jingle_session_manager_->message_loop(), MessageLoop::current());

  cricket_session_ = cricket_session;
  jid_ = cricket_session_->remote_name();
  cricket_session_->SignalState.connect(
      this, &JingleSession::OnSessionState);
}

bool JingleSession::HasSession(cricket::Session* cricket_session) {
  return cricket_session_ == cricket_session;
}

cricket::Session* JingleSession::ReleaseSession() {
  DCHECK_EQ(jingle_session_manager_->message_loop(), MessageLoop::current());

  SetState(CLOSED);
  cricket::Session* session = cricket_session_;
  if (cricket_session_)
    cricket_session_->SignalState.disconnect(this);
  cricket_session_ = NULL;
  closed_ = true;
  return session;
}

void JingleSession::SetStateChangeCallback(
    StateChangeCallback* callback) {
  DCHECK_EQ(jingle_session_manager_->message_loop(), MessageLoop::current());
  DCHECK(callback);
  state_change_callback_.reset(callback);
}

net::Socket* JingleSession::control_channel() {
  DCHECK_EQ(jingle_session_manager_->message_loop(), MessageLoop::current());
  return control_channel_adapter_.get();
}

net::Socket* JingleSession::event_channel() {
  DCHECK_EQ(jingle_session_manager_->message_loop(), MessageLoop::current());
  return event_channel_adapter_.get();
}

// TODO(sergeyu): Remove this method after we switch to RTP.
net::Socket* JingleSession::video_channel() {
  DCHECK_EQ(jingle_session_manager_->message_loop(), MessageLoop::current());
  return video_channel_adapter_.get();
}

net::Socket* JingleSession::video_rtp_channel() {
  DCHECK_EQ(jingle_session_manager_->message_loop(), MessageLoop::current());
  return video_rtp_channel_.get();
}

net::Socket* JingleSession::video_rtcp_channel() {
  DCHECK_EQ(jingle_session_manager_->message_loop(), MessageLoop::current());
  return video_rtcp_channel_.get();
}

const std::string& JingleSession::jid() {
  // No synchronization is needed because jid_ is not changed
  // after new connection is passed to JingleChromotocolServer callback.
  return jid_;
}

MessageLoop* JingleSession::message_loop() {
  return jingle_session_manager_->message_loop();
}

const CandidateSessionConfig*
JingleSession::candidate_config() {
  DCHECK(candidate_config_.get());
  return candidate_config_.get();
}

void JingleSession::set_candidate_config(
    const CandidateSessionConfig* candidate_config) {
  DCHECK(!candidate_config_.get());
  DCHECK(candidate_config);
  candidate_config_.reset(candidate_config);
}

const SessionConfig* JingleSession::config() {
  DCHECK(config_.get());
  return config_.get();
}

void JingleSession::set_config(const SessionConfig* config) {
  DCHECK(!config_.get());
  DCHECK(config);
  config_.reset(config);
}

void JingleSession::Close(Task* closed_task) {
  if (MessageLoop::current() != jingle_session_manager_->message_loop()) {
    jingle_session_manager_->message_loop()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &JingleSession::Close,
                                     closed_task));
    return;
  }

  if (!closed_) {
    if (control_channel_adapter_.get())
      control_channel_adapter_->Close(net::ERR_CONNECTION_CLOSED);

    if (control_channel_) {
      control_channel_->OnSessionTerminate(cricket_session_);
      control_channel_ = NULL;
    }

    if (event_channel_adapter_.get())
      event_channel_adapter_->Close(net::ERR_CONNECTION_CLOSED);

    if (event_channel_) {
      event_channel_->OnSessionTerminate(cricket_session_);
      event_channel_ = NULL;
    }

    if (video_channel_adapter_.get())
      video_channel_adapter_->Close(net::ERR_CONNECTION_CLOSED);

    if (video_channel_) {
      video_channel_->OnSessionTerminate(cricket_session_);
      video_channel_ = NULL;
    }

    if (video_rtp_channel_.get())
      video_rtp_channel_->Close(net::ERR_CONNECTION_CLOSED);
    if (video_rtcp_channel_.get())
      video_rtcp_channel_->Close(net::ERR_CONNECTION_CLOSED);

    if (cricket_session_)
      cricket_session_->Terminate();

    SetState(CLOSED);

    closed_ = true;
  }

  closed_task->Run();
  delete closed_task;
}

void JingleSession::OnSessionState(
    BaseSession* session, BaseSession::State state) {
  DCHECK_EQ(cricket_session_, session);

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

void JingleSession::OnInitiate() {
  jid_ = cricket_session_->remote_name();

  std::string content_name;
  // If we initiate the session, we get to specify the content name. When
  // accepting one, the remote end specifies it.
  if (cricket_session_->initiator()) {
    content_name = kChromotingContentName;
  } else {
    const cricket::ContentInfo* content;
    content = cricket_session_->remote_description()->FirstContentByType(
        kChromotingXmlNamespace);
    CHECK(content);
    content_name = content->name;
  }

  // Create video RTP channels.
  video_rtp_channel_.reset(new TransportChannelSocketAdapter(
      cricket_session_->CreateChannel(content_name, kVideoRtpChannelName)));
  video_rtcp_channel_.reset(new TransportChannelSocketAdapter(
      cricket_session_->CreateChannel(content_name, kVideoRtcpChannelName)));

  // Create control channel.
  control_channel_ = new PseudoTcpChannel(
      jingle_session_manager_->jingle_thread(), cricket_session_);
  control_channel_->Connect(content_name, kControlChannelName);
  control_channel_adapter_.reset(new StreamSocketAdapter(
      control_channel_->GetStream()));

  // Create event channel.
  event_channel_ = new PseudoTcpChannel(
      jingle_session_manager_->jingle_thread(), cricket_session_);
  event_channel_->Connect(content_name, kEventChannelName);
  event_channel_adapter_.reset(new StreamSocketAdapter(
      event_channel_->GetStream()));

  // Create video channel.
  // TODO(sergeyu): Remove video channel when we are ready to switch to RTP.
  video_channel_ = new PseudoTcpChannel(
      jingle_session_manager_->jingle_thread(), cricket_session_);
  video_channel_->Connect(content_name, kVideoChannelName);
  video_channel_adapter_.reset(new StreamSocketAdapter(
      video_channel_->GetStream()));

  if (!cricket_session_->initiator())
    jingle_session_manager_->AcceptConnection(this, cricket_session_);

  SetState(CONNECTING);
}

void JingleSession::OnAccept() {
  // Set the config if we are the one who initiated the session.
  if (cricket_session_->initiator()) {
    const cricket::ContentInfo* content =
        cricket_session_->remote_description()->FirstContentByType(
            kChromotingXmlNamespace);
    CHECK(content);

    const protocol::ContentDescription* content_description =
        static_cast<const protocol::ContentDescription*>(content->description);
    SessionConfig* config = content_description->config()->GetFinalConfig();

    // Terminate the session if the config we received is invalid.
    if (!config || !candidate_config()->IsSupported(config)) {
      // TODO(sergeyu): Inform the user that the host is misbehaving?
      LOG(ERROR) << "Terminating outgoing session after an "
          "invalid session description has been received.";
      cricket_session_->Terminate();
      return;
    }

    set_config(config);
  }

  SetState(CONNECTED);
}

void JingleSession::OnTerminate() {
  if (control_channel_adapter_.get())
    control_channel_adapter_->Close(net::ERR_CONNECTION_ABORTED);
  if (control_channel_) {
    control_channel_->OnSessionTerminate(cricket_session_);
    control_channel_ = NULL;
  }

  if (event_channel_adapter_.get())
    event_channel_adapter_->Close(net::ERR_CONNECTION_ABORTED);
  if (event_channel_) {
    event_channel_->OnSessionTerminate(cricket_session_);
    event_channel_ = NULL;
  }

  if (video_channel_adapter_.get())
    video_channel_adapter_->Close(net::ERR_CONNECTION_ABORTED);
  if (video_channel_) {
    video_channel_->OnSessionTerminate(cricket_session_);
    video_channel_ = NULL;
  }

  if (video_rtp_channel_.get())
    video_rtp_channel_->Close(net::ERR_CONNECTION_ABORTED);
  if (video_rtcp_channel_.get())
    video_rtcp_channel_->Close(net::ERR_CONNECTION_ABORTED);

  SetState(CLOSED);

  closed_ = true;
}

void JingleSession::SetState(State new_state) {
  if (new_state != state_) {
    state_ = new_state;
    if (!closed_ && state_change_callback_.get())
      state_change_callback_->Run(new_state);
  }
}

}  // namespace protocol

}  // namespace remoting
