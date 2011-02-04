// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/jingle_connection_to_host.h"

#include "base/callback.h"
#include "base/message_loop.h"
#include "remoting/base/constants.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "remoting/proto/auth.pb.h"
#include "remoting/protocol/client_message_dispatcher.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/host_control_sender.h"
#include "remoting/protocol/input_sender.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/video_reader.h"
#include "remoting/protocol/video_stub.h"
#include "remoting/protocol/util.h"

namespace remoting {
namespace protocol {

JingleConnectionToHost::JingleConnectionToHost(JingleThread* thread)
    : thread_(thread),
      event_callback_(NULL),
      dispatcher_(new ClientMessageDispatcher()) {
}

JingleConnectionToHost::~JingleConnectionToHost() {
}

void JingleConnectionToHost::Connect(const std::string& username,
                                     const std::string& auth_token,
                                     const std::string& host_jid,
                                     HostEventCallback* event_callback,
                                     ClientStub* client_stub,
                                     VideoStub* video_stub) {
  event_callback_ = event_callback;
  client_stub_ = client_stub;
  video_stub_ = video_stub;

  // Initialize |jingle_client_|.
  jingle_client_ = new JingleClient(thread_);
  jingle_client_->Init(username, auth_token,
                       kChromotingTokenServiceName, this);

  // Save jid of the host. The actual connection is created later after
  // |jingle_client_| is connected.
  host_jid_ = host_jid;
}

void JingleConnectionToHost::Disconnect() {
  if (MessageLoop::current() != message_loop()) {
    message_loop()->PostTask(
        FROM_HERE, NewRunnableMethod(this,
                                     &JingleConnectionToHost::Disconnect));
    return;
  }

  if (session_) {
    session_->Close(
        NewRunnableMethod(this, &JingleConnectionToHost::OnDisconnected));
  } else {
    OnDisconnected();
  }
}

void JingleConnectionToHost::InitSession() {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  // Initialize chromotocol |session_manager_|.
  JingleSessionManager* session_manager =
      new JingleSessionManager(thread_);
  // TODO(ajwong): Make this a command switch when we're more stable.
  session_manager->set_allow_local_ips(true);
  session_manager->Init(
      jingle_client_->GetFullJid(),
      jingle_client_->session_manager(),
      NewCallback(this, &JingleConnectionToHost::OnNewSession),
      NULL, NULL);
  session_manager_ = session_manager;

  CandidateSessionConfig* candidate_config =
      CandidateSessionConfig::CreateDefault();
  // TODO(sergeyu): Set resolution in the |candidate_config| to the desired
  // resolution.

  ClientAuthToken auth_token_proto;
  auth_token_proto.set_host_full_jid(host_jid_);
  auth_token_proto.set_client_full_jid(jingle_client_->GetFullJid());
  // TODO(ajwong): Use real token.
  auth_token_proto.set_client_oauth_token("");

  // TODO(ajwong): We should encrypt this based on the host's public key.
  std::string client_token = auth_token_proto.SerializeAsString();

  // Initialize |session_|.
  session_ = session_manager_->Connect(
      host_jid_, client_token, candidate_config,
      NewCallback(this, &JingleConnectionToHost::OnSessionStateChange));
}

void JingleConnectionToHost::OnDisconnected() {
  session_ = NULL;

  if (session_manager_) {
    session_manager_->Close(
        NewRunnableMethod(this, &JingleConnectionToHost::OnServerClosed));
  } else {
    OnServerClosed();
  }
}

void JingleConnectionToHost::OnServerClosed() {
  session_manager_ = NULL;
  if (jingle_client_) {
    jingle_client_->Close();
    jingle_client_ = NULL;
  }
}

const SessionConfig* JingleConnectionToHost::config() {
  return session_->config();
}

// JingleClient::Callback interface.
void JingleConnectionToHost::OnStateChange(JingleClient* client,
                                         JingleClient::State state) {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  DCHECK(client);
  DCHECK(event_callback_);

  if (state == JingleClient::CONNECTED) {
    VLOG(1) << "Connected as: " << client->GetFullJid();
    InitSession();
  } else if (state == JingleClient::CLOSED) {
    VLOG(1) << "Connection closed.";
    event_callback_->OnConnectionClosed(this);
  }
}

void JingleConnectionToHost::OnNewSession(Session* session,
    SessionManager::IncomingSessionResponse* response) {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  // Client always rejects incoming sessions.
  *response = SessionManager::DECLINE;
}

void JingleConnectionToHost::OnSessionStateChange(
    Session::State state) {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  DCHECK(event_callback_);

  switch (state) {
    case Session::FAILED:
      event_callback_->OnConnectionFailed(this);
      break;

    case Session::CLOSED:
      event_callback_->OnConnectionClosed(this);
      break;

    case Session::CONNECTED:
      // Initialize reader and writer.
      video_reader_.reset(VideoReader::Create(session_->config()));
      video_reader_->Init(session_, video_stub_);
      input_stub_.reset(new InputSender(session_->event_channel()));
      host_stub_.reset(new HostControlSender(session_->control_channel()));
      dispatcher_->Initialize(session_.get(), client_stub_);
      event_callback_->OnConnectionOpened(this);
      break;

    default:
      // Ignore the other states by default.
      break;
  }
}

MessageLoop* JingleConnectionToHost::message_loop() {
  return thread_->message_loop();
}

}  // namespace protocol
}  // namespace remoting
