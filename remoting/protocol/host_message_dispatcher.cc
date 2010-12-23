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
#include "remoting/protocol/ref_counted_message.h"
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

  control_message_reader_.reset(new MessageReader());
  event_message_reader_.reset(new MessageReader());
  host_stub_ = host_stub;
  input_stub_ = input_stub;

  // Initialize the readers on the sockets provided by channels.
  event_message_reader_->Init<EventMessage>(
      session->event_channel(),
      NewCallback(this, &HostMessageDispatcher::OnEventMessageReceived));
  control_message_reader_->Init<ControlMessage>(
      session->control_channel(),
      NewCallback(this, &HostMessageDispatcher::OnControlMessageReceived));
}

void HostMessageDispatcher::OnControlMessageReceived(ControlMessage* message) {
  scoped_refptr<RefCountedMessage<ControlMessage> > ref_msg =
      new RefCountedMessage<ControlMessage>(message);
  if (message->has_suggest_resolution()) {
    host_stub_->SuggestResolution(
        &message->suggest_resolution(), NewDeleteTask(ref_msg));
  } else if (message->has_begin_session_request()) {
    host_stub_->BeginSessionRequest(
        &message->begin_session_request().credentials(),
        NewDeleteTask(ref_msg));
  } else {
    NOTREACHED() << "Invalid control message received";
  }
}

void HostMessageDispatcher::OnEventMessageReceived(
    EventMessage* message) {
  scoped_refptr<RefCountedMessage<EventMessage> > ref_msg =
      new RefCountedMessage<EventMessage>(message);
  for (int i = 0; i < message->event_size(); ++i) {
    if (message->event(i).has_key()) {
      input_stub_->InjectKeyEvent(
          &message->event(i).key(), NewDeleteTask(ref_msg));
    }
    if (message->event(i).has_mouse()) {
      input_stub_->InjectMouseEvent(
          &message->event(i).mouse(), NewDeleteTask(ref_msg));
    }
  }
}

}  // namespace protocol
}  // namespace remoting
