// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "net/base/io_buffer.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/host_message_dispatcher.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/message_reader.h"
#include "remoting/protocol/session.h"

namespace remoting {
namespace protocol {

HostMessageDispatcher::HostMessageDispatcher()
    : connection_(NULL),
      host_stub_(NULL),
      input_stub_(NULL) {
}

HostMessageDispatcher::~HostMessageDispatcher() {
}

void HostMessageDispatcher::Initialize(
    ConnectionToClient* connection,
    HostStub* host_stub, InputStub* input_stub) {
  if (!connection || !host_stub || !input_stub ||
      !connection->session()->event_channel() ||
      !connection->session()->control_channel()) {
    return;
  }

  control_message_reader_.reset(new ProtobufMessageReader<ControlMessage>());
  event_message_reader_.reset(new ProtobufMessageReader<EventMessage>());
  connection_ = connection;
  host_stub_ = host_stub;
  input_stub_ = input_stub;

  // Initialize the readers on the sockets provided by channels.
  event_message_reader_->Init(
      connection->session()->event_channel(),
      NewCallback(this, &HostMessageDispatcher::OnEventMessageReceived));
  control_message_reader_->Init(
      connection->session()->control_channel(),
      NewCallback(this, &HostMessageDispatcher::OnControlMessageReceived));
}

void HostMessageDispatcher::OnControlMessageReceived(
    ControlMessage* message, Task* done_task) {
  if (message->has_begin_session_request()) {
    const BeginSessionRequest& request = message->begin_session_request();
    if (request.has_credentials() && request.credentials().has_type()) {
      host_stub_->BeginSessionRequest(&request.credentials(), done_task);
      return;
    }
  }

  LOG(WARNING) << "Invalid control message received.";
  done_task->Run();
  delete done_task;
}

void HostMessageDispatcher::OnEventMessageReceived(
    EventMessage* message, Task* done_task) {
  base::ScopedTaskRunner done_runner(done_task);

  connection_->UpdateSequenceNumber(message->sequence_number());

  if (message->has_key_event()) {
    const KeyEvent& event = message->key_event();
    if (event.has_keycode() && event.has_pressed()) {
      input_stub_->InjectKeyEvent(event);
      return;
    }
  } else if (message->has_mouse_event()) {
    input_stub_->InjectMouseEvent(message->mouse_event());
    return;
  }

  LOG(WARNING) << "Unknown event message received.";
}

}  // namespace protocol
}  // namespace remoting
