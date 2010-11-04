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
namespace protocol {

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

void ClientConnection::Init(protocol::Session* session) {
  DCHECK_EQ(session->message_loop(), MessageLoop::current());

  session_ = session;
  session_->SetStateChangeCallback(
      NewCallback(this, &ClientConnection::OnSessionStateChange));
}

protocol::Session* ClientConnection::session() {
  return session_;
}

void ClientConnection::SendInitClientMessage(int width, int height) {
  DCHECK_EQ(loop_, MessageLoop::current());

  // If we are disconnected then return.
  if (!session_)
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
  if (!session_)
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
  if (session_) {
    session_->Close(NewRunnableMethod(this, &ClientConnection::OnClosed));
    session_ = NULL;
  }
}

ClientConnection::ClientConnection() {}

void ClientConnection::OnSessionStateChange(
    protocol::Session::State state) {
  if (state == protocol::Session::CONNECTED) {
    control_writer_.Init(session_->control_channel());
    event_reader_.Init<ChromotingClientMessage>(
        session_->event_channel(),
        NewCallback(this, &ClientConnection::OnMessageReceived));
    video_writer_.reset(VideoWriter::Create(session_->config()));
    video_writer_->Init(session_);
  }

  loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &ClientConnection::StateChangeTask, state));
}

void ClientConnection::OnMessageReceived(ChromotingClientMessage* message) {
  loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &ClientConnection::MessageReceivedTask,
                        message));
}

void ClientConnection::StateChangeTask(protocol::Session::State state) {
  DCHECK_EQ(loop_, MessageLoop::current());

  DCHECK(handler_);
  switch(state) {
    case protocol::Session::CONNECTING:
      break;
    // Don't care about this message.
    case protocol::Session::CONNECTED:
      handler_->OnConnectionOpened(this);
      break;
    case protocol::Session::CLOSED:
      handler_->OnConnectionClosed(this);
      break;
    case protocol::Session::FAILED:
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

// OnClosed() is used as a callback for protocol::Session::Close().
void ClientConnection::OnClosed() {
}

}  // namespace protocol
}  // namespace remoting
