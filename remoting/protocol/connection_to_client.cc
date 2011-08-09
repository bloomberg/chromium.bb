// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/connection_to_client.h"

#include "base/bind.h"
#include "google/protobuf/message.h"
#include "net/base/io_buffer.h"
#include "remoting/protocol/client_control_sender.h"
#include "remoting/protocol/host_message_dispatcher.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/input_stub.h"

// TODO(hclam): Remove this header once MessageDispatcher is used.
#include "remoting/base/compound_buffer.h"

namespace remoting {
namespace protocol {

// Determine how many update streams we should count to find the size of
// average update stream.
static const size_t kAverageUpdateStream = 10;

ConnectionToClient::ConnectionToClient(MessageLoop* message_loop,
                                       EventHandler* handler)
    : loop_(message_loop),
      handler_(handler),
      host_stub_(NULL),
      input_stub_(NULL) {
  DCHECK(loop_);
  DCHECK(handler_);
}

ConnectionToClient::~ConnectionToClient() {
  // TODO(hclam): When we shut down the viewer we may have to close the
  // connection.
}

void ConnectionToClient::Init(protocol::Session* session) {
  DCHECK_EQ(loop_, MessageLoop::current());
  session_.reset(session);
  session_->SetStateChangeCallback(
      NewCallback(this, &ConnectionToClient::OnSessionStateChange));
}

protocol::Session* ConnectionToClient::session() {
  return session_.get();
}

void ConnectionToClient::Disconnect() {
  // This method can be called from main thread so perform threading switching.
  if (MessageLoop::current() != loop_) {
    loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &ConnectionToClient::Disconnect));
    return;
  }

  CloseChannels();

  // If there is a session then release it, causing it to close.
  if (session_.get())
    session_.reset();
}

void ConnectionToClient::UpdateSequenceNumber(int64 sequence_number) {
  handler_->OnSequenceNumberUpdated(this, sequence_number);
}

VideoStub* ConnectionToClient::video_stub() {
  return video_writer_.get();
}

// Return pointer to ClientStub.
ClientStub* ConnectionToClient::client_stub() {
  return client_control_sender_.get();
}

void ConnectionToClient::set_host_stub(protocol::HostStub* host_stub) {
  host_stub_ = host_stub;
}

void ConnectionToClient::set_input_stub(protocol::InputStub* input_stub) {
  input_stub_ = input_stub;
}

void ConnectionToClient::OnSessionStateChange(protocol::Session::State state) {
  DCHECK_EQ(loop_, MessageLoop::current());

  DCHECK(handler_);
  switch(state) {
    case protocol::Session::CONNECTING:
      break;
    // Don't care about this message.
    case protocol::Session::CONNECTED:
      client_control_sender_.reset(
          new ClientControlSender(session_->control_channel()));
      video_writer_.reset(VideoWriter::Create(session_->config()));
      video_writer_->Init(
          session_.get(), base::Bind(&ConnectionToClient::OnVideoInitialized,
                                     base::Unretained(this)));
      dispatcher_.reset(new HostMessageDispatcher());
      dispatcher_->Initialize(this, host_stub_, input_stub_);
      break;
    case protocol::Session::CLOSED:
      CloseChannels();
      handler_->OnConnectionClosed(this);
      break;
    case protocol::Session::FAILED:
      CloseOnError();
      break;
    default:
      // We shouldn't receive other states.
      NOTREACHED();
  }
}

void ConnectionToClient::OnVideoInitialized(bool successful) {
  if (!successful) {
    CloseOnError();
    return;
  }

  handler_->OnConnectionOpened(this);
}

void ConnectionToClient::CloseOnError() {
  CloseChannels();
  handler_->OnConnectionFailed(this);
}

void ConnectionToClient::CloseChannels() {
  if (video_writer_.get())
    video_writer_->Close();
  if (client_control_sender_.get())
    client_control_sender_->Close();
}

}  // namespace protocol
}  // namespace remoting
