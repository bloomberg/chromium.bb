// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/jingle_chromoting_connection.h"

#include "base/message_loop.h"
#include "net/base/net_errors.h"
#include "remoting/base/constants.h"
#include "remoting/jingle_glue/channel_socket_adapter.h"
#include "remoting/jingle_glue/stream_socket_adapter.h"
#include "remoting/protocol/jingle_chromoting_server.h"
#include "third_party/libjingle/source/talk/base/thread.h"
#include "third_party/libjingle/source/talk/p2p/base/session.h"
#include "third_party/libjingle/source/talk/session/tunnel/pseudotcpchannel.h"

using cricket::BaseSession;
using cricket::PseudoTcpChannel;
using cricket::Session;

namespace remoting {

namespace {
const char kVideoChannelName[] = "video";
const char kVideoRtpChannelName[] = "videortp";
const char kVideoRtcpChannelName[] = "videortcp";
const char kEventsChannelName[] = "events";
}  // namespace

JingleChromotingConnection::JingleChromotingConnection(
    JingleChromotingServer* session_client)
    : session_client_(session_client),
      state_(INITIALIZING),
      closed_(false),
      session_(NULL),
      events_channel_(NULL),
      video_channel_(NULL) {
}

JingleChromotingConnection::~JingleChromotingConnection() {
  DCHECK(closed_);
}

void JingleChromotingConnection::Init(Session* session) {
  DCHECK_EQ(session_client_->message_loop(), MessageLoop::current());

  session_ = session;
  jid_ = session_->remote_name();
  session_->SignalState.connect(
      this, &JingleChromotingConnection::OnSessionState);
}

bool JingleChromotingConnection::HasSession(cricket::Session* session) {
  return session_ == session;
}

Session* JingleChromotingConnection::ReleaseSession() {
  DCHECK_EQ(session_client_->message_loop(), MessageLoop::current());

  SetState(CLOSED);
  Session* session = session_;
  if (session_)
    session_->SignalState.disconnect(this);
  session_ = NULL;
  closed_ = true;
  return session;
}

void JingleChromotingConnection::SetStateChangeCallback(
    StateChangeCallback* callback) {
  DCHECK_EQ(session_client_->message_loop(), MessageLoop::current());
  DCHECK(callback);
  state_change_callback_.reset(callback);
}

// TODO(sergeyu): Remove this method after we switch to RTP.
net::Socket* JingleChromotingConnection::GetVideoChannel() {
  DCHECK_EQ(session_client_->message_loop(), MessageLoop::current());
  return video_channel_adapter_.get();
}

net::Socket* JingleChromotingConnection::GetEventsChannel() {
  DCHECK_EQ(session_client_->message_loop(), MessageLoop::current());
  return events_channel_adapter_.get();
}

net::Socket* JingleChromotingConnection::GetVideoRtpChannel() {
  DCHECK_EQ(session_client_->message_loop(), MessageLoop::current());
  return video_rtp_channel_.get();
}

net::Socket* JingleChromotingConnection::GetVideoRtcpChannel() {
  DCHECK_EQ(session_client_->message_loop(), MessageLoop::current());
  return video_rtcp_channel_.get();
}

void JingleChromotingConnection::Close(Task* closed_task) {
  if (MessageLoop::current() != session_client_->message_loop()) {
    session_client_->message_loop()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &JingleChromotingConnection::Close,
                                     closed_task));
    return;
  }

  if (!closed_) {
    if (events_channel_adapter_.get())
      events_channel_adapter_->Close(net::ERR_CONNECTION_CLOSED);

    if (events_channel_) {
      events_channel_->OnSessionTerminate(session_);
      events_channel_ = NULL;
    }

    if (video_channel_adapter_.get())
      video_channel_adapter_->Close(net::ERR_CONNECTION_CLOSED);

    if (video_channel_) {
      video_channel_->OnSessionTerminate(session_);
      video_channel_ = NULL;
    }

    if (video_rtp_channel_.get())
      video_rtp_channel_->Close(net::ERR_CONNECTION_CLOSED);
    if (video_rtcp_channel_.get())
      video_rtcp_channel_->Close(net::ERR_CONNECTION_CLOSED);

    if (session_)
      session_->Terminate();

    closed_ = true;
  }

  closed_task->Run();
  delete closed_task;
}

void JingleChromotingConnection::OnSessionState(
    BaseSession* session, BaseSession::State state) {
  DCHECK_EQ(session_, session);

  switch (state) {
    case Session::STATE_SENTINITIATE:
      OnInitiate(false);
      break;

    case Session::STATE_RECEIVEDINITIATE:
      OnInitiate(true);
      break;

    case Session::STATE_SENTACCEPT:
    case Session::STATE_RECEIVEDACCEPT:
      OnAccept();
      break;

    case Session::STATE_RECEIVEDTERMINATE:
      OnTerminate();
      break;

    case Session::STATE_DEINIT:
      // Close() must have been called before this.
      NOTREACHED();
      break;

    default:
      break;
  }
}

void JingleChromotingConnection::OnInitiate(bool incoming) {
  jid_ = session_->remote_name();
  if (incoming)
    session_client_->AcceptConnection(this, session_);
  SetState(CONNECTING);
}

void JingleChromotingConnection::OnAccept() {
  const cricket::ContentInfo* content =
      session_->remote_description()->FirstContentByType(
          kChromotingXmlNamespace);
  ASSERT(content != NULL);

  // Create video RTP channels.
  video_rtp_channel_.reset(new TransportChannelSocketAdapter(
      session_->CreateChannel(content->name, kVideoRtpChannelName)));
  video_rtcp_channel_.reset(new TransportChannelSocketAdapter(
      session_->CreateChannel(content->name, kVideoRtcpChannelName)));

  // Create events channel.
  events_channel_ =
      new PseudoTcpChannel(talk_base::Thread::Current(), session_);
  events_channel_->Connect(content->name, kEventsChannelName);
  events_channel_adapter_.reset(new StreamSocketAdapter(
      events_channel_->GetStream()));

  // Create video channel.
  // TODO(sergeyu): Remove video channel when we are ready to switch to RTP.
  video_channel_ =
      new PseudoTcpChannel(talk_base::Thread::Current(), session_);
  video_channel_->Connect(content->name, kVideoChannelName);
  video_channel_adapter_.reset(new StreamSocketAdapter(
      video_channel_->GetStream()));

  SetState(CONNECTED);
}

void JingleChromotingConnection::OnTerminate() {
  if (events_channel_adapter_.get())
    events_channel_adapter_->Close(net::ERR_CONNECTION_ABORTED);
  if (events_channel_) {
    events_channel_->OnSessionTerminate(session_);
    events_channel_ = NULL;
  }

  if (video_channel_adapter_.get())
    video_channel_adapter_->Close(net::ERR_CONNECTION_ABORTED);
  if (video_channel_) {
    video_channel_->OnSessionTerminate(session_);
    video_channel_ = NULL;
  }

  if (video_rtp_channel_.get())
    video_rtp_channel_->Close(net::ERR_CONNECTION_ABORTED);
  if (video_rtcp_channel_.get())
    video_rtcp_channel_->Close(net::ERR_CONNECTION_ABORTED);

  SetState(CLOSED);

  closed_ = true;
}

void JingleChromotingConnection::SetState(State new_state) {
  if (new_state != state_) {
    state_ = new_state;
    if (!closed_ && state_change_callback_.get())
      state_change_callback_->Run(new_state);
  }
}

}  // namespace remoting
