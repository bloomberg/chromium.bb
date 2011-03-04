// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ref_counted.h"
#include "net/base/io_buffer.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/host_message_dispatcher.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/message_reader.h"
#include "remoting/protocol/session.h"

namespace remoting {
namespace protocol {

HostMessageDispatcher::HostMessageDispatcher() :
    host_stub_(NULL),
    input_stub_(NULL) {
}

HostMessageDispatcher::~HostMessageDispatcher() {
}

void HostMessageDispatcher::Initialize(
    protocol::Session* session,
    HostStub* host_stub, InputStub* input_stub) {
  if (!session || !host_stub || !input_stub ||
      !session->event_channel() || !session->control_channel()) {
    return;
  }

  control_message_reader_.reset(new ProtobufMessageReader<ControlMessage>());
  event_message_reader_.reset(new ProtobufMessageReader<EventMessage>());
  host_stub_ = host_stub;
  input_stub_ = input_stub;

  // Initialize the readers on the sockets provided by channels.
  event_message_reader_->Init(
      session->event_channel(),
      NewCallback(this, &HostMessageDispatcher::OnEventMessageReceived));
  control_message_reader_->Init(
      session->control_channel(),
      NewCallback(this, &HostMessageDispatcher::OnControlMessageReceived));
}

void HostMessageDispatcher::OnControlMessageReceived(
    ControlMessage* message, Task* done_task) {
  if (!host_stub_->authenticated()) {
    // When the client has not authenticated with the host, we restrict the
    // control messages that we support.
    if (message->has_begin_session_request()) {
      host_stub_->BeginSessionRequest(
          &message->begin_session_request().credentials(), done_task);
      return;
    } else {
      LOG(WARNING) << "Invalid control message received "
                   << "(client not authenticated).";
    }
  } else {
    // TODO(sergeyu): Add message validation.
    if (message->has_suggest_resolution()) {
      host_stub_->SuggestResolution(&message->suggest_resolution(), done_task);
      return;
    } else if (message->has_begin_session_request()) {
      LOG(WARNING) << "BeginSessionRequest sent after client already "
                   << "authorized.";
    } else {
      LOG(WARNING) << "Invalid control message received.";
    }
  }
  done_task->Run();
  delete done_task;
}

void HostMessageDispatcher::OnEventMessageReceived(
    EventMessage* message, Task* done_task) {
  if (input_stub_->authenticated()) {
    // TODO(sergeyu): Add message validation.
    if (message->has_key_event()) {
      input_stub_->InjectKeyEvent(&message->key_event(), done_task);
    } else if (message->has_mouse_event()) {
      input_stub_->InjectMouseEvent(&message->mouse_event(), done_task);
    } else {
      LOG(WARNING) << "Invalid event message received.";
      done_task->Run();
      delete done_task;
    }
  }
}

}  // namespace protocol
}  // namespace remoting
