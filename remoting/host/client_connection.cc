// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(hclam): Remove this header once MessageDispatcher is used.
#include "remoting/base/multiple_array_input_stream.h"
#include "remoting/host/client_connection.h"

#include "google/protobuf/message.h"
#include "net/base/io_buffer.h"
#include "remoting/protocol/util.h"

namespace remoting {

// Determine how many update streams we should count to find the size of
// average update stream.
static const size_t kAverageUpdateStream = 10;

ClientConnection::ClientConnection(MessageLoop* message_loop,
                                   EventHandler* handler)
    : loop_(message_loop),
      handler_(handler) {
  DCHECK(loop_);
  DCHECK(handler_);
}

ClientConnection::~ClientConnection() {
  // TODO(hclam): When we shut down the viewer we may have to close the
  // connection.
}

void ClientConnection::Init(ChromotocolConnection* connection) {
  DCHECK_EQ(connection->message_loop(), MessageLoop::current());

  connection_ = connection;
  connection_->SetStateChangeCallback(
      NewCallback(this, &ClientConnection::OnConnectionStateChange));
}

ChromotocolConnection* ClientConnection::connection() {
  return connection_;
}

void ClientConnection::SendInitClientMessage(int width, int height) {
  DCHECK_EQ(loop_, MessageLoop::current());

  // If we are disconnected then return.
  if (!connection_)
    return;

  ChromotingHostMessage msg;
  msg.mutable_init_client()->set_width(width);
  msg.mutable_init_client()->set_height(height);
  DCHECK(msg.IsInitialized());
  control_writer_.SendMessage(msg);
}

void ClientConnection::SendVideoPacket(const VideoPacket& packet) {
  DCHECK_EQ(loop_, MessageLoop::current());

  // If we are disconnected then return.
  if (!connection_)
    return;

  video_writer_->SendPacket(packet);
}

int ClientConnection::GetPendingUpdateStreamMessages() {
  DCHECK_EQ(loop_, MessageLoop::current());
  return video_writer_->GetPendingPackets();
}

void ClientConnection::Disconnect() {
  DCHECK_EQ(loop_, MessageLoop::current());

  // If there is a channel then close it and release the reference.
  if (connection_) {
    connection_->Close(NewRunnableMethod(this, &ClientConnection::OnClosed));
    connection_ = NULL;
  }
}

ClientConnection::ClientConnection() {}

void ClientConnection::OnConnectionStateChange(
    ChromotocolConnection::State state) {
  if (state == ChromotocolConnection::CONNECTED) {
    control_writer_.Init(connection_->control_channel());
    event_reader_.Init<ChromotingClientMessage>(
        connection_->event_channel(),
        NewCallback(this, &ClientConnection::OnMessageReceived));
    video_writer_.reset(VideoWriter::Create(connection_->config()));
    video_writer_->Init(connection_);
  }

  loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &ClientConnection::StateChangeTask, state));
}

void ClientConnection::OnMessageReceived(ChromotingClientMessage* message) {
  loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &ClientConnection::MessageReceivedTask,
                        message));
}

void ClientConnection::StateChangeTask(ChromotocolConnection::State state) {
  DCHECK_EQ(loop_, MessageLoop::current());

  DCHECK(handler_);
  switch(state) {
    case ChromotocolConnection::CONNECTING:
      break;
    // Don't care about this message.
    case ChromotocolConnection::CONNECTED:
      handler_->OnConnectionOpened(this);
      break;
    case ChromotocolConnection::CLOSED:
      handler_->OnConnectionClosed(this);
      break;
    case ChromotocolConnection::FAILED:
      handler_->OnConnectionFailed(this);
      break;
    default:
      // We shouldn't receive other states.
      NOTREACHED();
  }
}

void ClientConnection::MessageReceivedTask(ChromotingClientMessage* message) {
  DCHECK_EQ(loop_, MessageLoop::current());
  DCHECK(handler_);
  handler_->HandleMessage(this, message);
}

// OnClosed() is used as a callback for ChromotocolConnection::Close().
void ClientConnection::OnClosed() {
}

}  // namespace remoting
