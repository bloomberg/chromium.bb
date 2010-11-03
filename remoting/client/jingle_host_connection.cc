// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/message_loop.h"
#include "remoting/base/constants.h"
// TODO(hclam): Remove this header once MessageDispatcher is used.
#include "remoting/base/multiple_array_input_stream.h"
#include "remoting/client/client_config.h"
#include "remoting/client/jingle_host_connection.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "remoting/protocol/jingle_chromotocol_server.h"
#include "remoting/protocol/video_stub.h"
#include "remoting/protocol/util.h"

namespace remoting {

JingleHostConnection::JingleHostConnection(ClientContext* context)
    : context_(context),
      event_callback_(NULL) {
}

JingleHostConnection::~JingleHostConnection() {
}

void JingleHostConnection::Connect(const ClientConfig& config,
                                   HostEventCallback* event_callback,
                                   VideoStub* video_stub) {
  event_callback_ = event_callback;
  video_stub_ = video_stub;

  // Initialize |jingle_client_|.
  jingle_client_ = new JingleClient(context_->jingle_thread());
  jingle_client_->Init(config.username, config.auth_token,
                       kChromotingTokenServiceName, this);

  // Save jid of the host. The actual connection is created later after
  // |jingle_client_| is connected.
  host_jid_ = config.host_jid;
}

void JingleHostConnection::Disconnect() {
  if (MessageLoop::current() != message_loop()) {
    message_loop()->PostTask(
        FROM_HERE, NewRunnableMethod(this,
                                     &JingleHostConnection::Disconnect));
    return;
  }

  control_reader_.Close();
  event_writer_.Close();
  video_reader_->Close();

  if (connection_) {
    connection_->Close(
        NewRunnableMethod(this, &JingleHostConnection::OnDisconnected));
  } else {
    OnDisconnected();
  }
}

void JingleHostConnection::OnControlMessage(ChromotingHostMessage* msg) {
  event_callback_->HandleMessage(this, msg);
}

void JingleHostConnection::InitConnection() {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  // Initialize |chromotocol_server_|.
  JingleChromotocolServer* chromotocol_server =
      new JingleChromotocolServer(context_->jingle_thread());
  // TODO(ajwong): Make this a command switch when we're more stable.
  chromotocol_server->set_allow_local_ips(true);
  chromotocol_server->Init(
      jingle_client_->GetFullJid(),
      jingle_client_->session_manager(),
      NewCallback(this, &JingleHostConnection::OnNewChromotocolConnection));
  chromotocol_server_ = chromotocol_server;

  CandidateChromotocolConfig* candidate_config =
      CandidateChromotocolConfig::CreateDefault();
  // TODO(sergeyu): Set resolution in the |candidate_config| to the desired
  // resolution.

  // Initialize |connection_|.
  connection_ = chromotocol_server_->Connect(
      host_jid_, candidate_config,
      NewCallback(this, &JingleHostConnection::OnConnectionStateChange));
}

void JingleHostConnection::OnDisconnected() {
  connection_ = NULL;

  if (chromotocol_server_) {
    chromotocol_server_->Close(
        NewRunnableMethod(this, &JingleHostConnection::OnServerClosed));
  } else {
    OnServerClosed();
  }
}

void JingleHostConnection::OnServerClosed() {
  chromotocol_server_ = NULL;
  if (jingle_client_) {
    jingle_client_->Close();
    jingle_client_ = NULL;
  }
}

void JingleHostConnection::SendEvent(const ChromotingClientMessage& msg) {
  // This drops the message if we are not connected yet.
  event_writer_.SendMessage(msg);
}

// JingleClient::Callback interface.
void JingleHostConnection::OnStateChange(JingleClient* client,
                                         JingleClient::State state) {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  DCHECK(client);
  DCHECK(event_callback_);

  if (state == JingleClient::CONNECTED) {
    VLOG(1) << "Connected as: " << client->GetFullJid();
    InitConnection();
  } else if (state == JingleClient::CLOSED) {
    VLOG(1) << "Connection closed.";
    event_callback_->OnConnectionClosed(this);
  }
}

void JingleHostConnection::OnNewChromotocolConnection(
    ChromotocolConnection* connection,
    ChromotocolServer::IncomingConnectionResponse* response) {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  // Client always rejects incoming connections.
  *response = ChromotocolServer::DECLINE;
}

void JingleHostConnection::OnConnectionStateChange(
    ChromotocolConnection::State state) {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  DCHECK(event_callback_);

  switch (state) {
    case ChromotocolConnection::FAILED:
      event_callback_->OnConnectionFailed(this);
      break;

    case ChromotocolConnection::CLOSED:
      event_callback_->OnConnectionClosed(this);
      break;

    case ChromotocolConnection::CONNECTED:
      // Initialize reader and writer.
      control_reader_.Init<ChromotingHostMessage>(
          connection_->control_channel(),
          NewCallback(this, &JingleHostConnection::OnControlMessage));
      event_writer_.Init(connection_->event_channel());
      video_reader_.reset(VideoReader::Create(connection_->config()));
      video_reader_->Init(connection_, video_stub_);
      event_callback_->OnConnectionOpened(this);
      break;

    default:
      // Ignore the other states by default.
      break;
  }
}

MessageLoop* JingleHostConnection::message_loop() {
  return context_->jingle_thread()->message_loop();
}

}  // namespace remoting
