// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "remoting/base/constants.h"
#include "remoting/client/client_config.h"
#include "remoting/client/jingle_host_connection.h"
#include "remoting/jingle_glue/jingle_thread.h"

namespace remoting {

JingleHostConnection::JingleHostConnection(ClientContext* context)
    : context_(context),
      event_callback_(NULL) {
}

JingleHostConnection::~JingleHostConnection() {
}

void JingleHostConnection::Connect(ClientConfig* config,
                                   HostEventCallback* event_callback) {
  message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &JingleHostConnection::DoConnect,
                        config, event_callback));
}

void JingleHostConnection::Disconnect() {
  message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &JingleHostConnection::DoDisconnect));
}

void JingleHostConnection::OnStateChange(JingleChannel* channel,
                                         JingleChannel::State state) {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  DCHECK(event_callback_);

  switch (state) {
    case JingleChannel::FAILED:
      event_callback_->OnConnectionFailed(this);
      break;

    case JingleChannel::CLOSED:
      event_callback_->OnConnectionClosed(this);
      break;

    case JingleChannel::OPEN:
      event_callback_->OnConnectionOpened(this);
      break;

    default:
      // Ignore the other states by default.
      break;
  }
}

void JingleHostConnection::OnPacketReceived(
    JingleChannel* channel,
    scoped_refptr<media::DataBuffer> buffer) {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  DCHECK(event_callback_);

  HostMessageList list;
  decoder_.ParseHostMessages(buffer, &list);
  event_callback_->HandleMessages(this, &list);
}

// JingleClient::Callback interface.
void JingleHostConnection::OnStateChange(JingleClient* client,
                                         JingleClient::State state) {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  DCHECK(client);
  DCHECK(event_callback_);

  if (state == JingleClient::CONNECTED) {
    LOG(INFO) << "Connected as: " << client->GetFullJid();
  } else if (state == JingleClient::CLOSED) {
    LOG(INFO) << "Connection closed.";
    event_callback_->OnConnectionClosed(this);
  }
}

bool JingleHostConnection::OnAcceptConnection(
    JingleClient* client,
    const std::string& jid,
    JingleChannel::Callback** callback) {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  // Client rejects all connection.
  return false;
}

void JingleHostConnection::OnNewConnection(
    JingleClient* client,
    scoped_refptr<JingleChannel> channel) {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  // TODO(ajwong): Should we log more aggressively on this and above? We
  // shouldn't be getting any inbound connections.
  NOTREACHED() << "Clients can't accept inbound connections.";
}

MessageLoop* JingleHostConnection::message_loop() {
  return context_->jingle_thread()->message_loop();
}

void JingleHostConnection::DoConnect(ClientConfig* config,
                                     HostEventCallback* event_callback) {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  event_callback_ = event_callback;

  jingle_client_ = new JingleClient(context_->jingle_thread());
  jingle_client_->Init(config->username(), config->auth_token(),
                       kChromotingTokenServiceName, this);
  jingle_channel_ = jingle_client_->Connect(config->host_jid(), this);
}

void JingleHostConnection::DoDisconnect() {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  if (jingle_channel_.get()) {
    jingle_channel_->Close();
    jingle_channel_ = NULL;
  }

  if (jingle_client_.get()) {
    jingle_client_->Close();
    jingle_client_ = NULL;
  }
}

}  // namespace remoting
