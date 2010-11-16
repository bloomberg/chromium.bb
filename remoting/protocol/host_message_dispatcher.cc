// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/io_buffer.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/host_message_dispatcher.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/message_reader.h"
#include "remoting/protocol/session.h"

namespace {

// A single protobuf can contain multiple messages that will be handled by
// different message handlers.  We use this wrapper to ensure that the
// protobuf is only deleted after all the handlers have finished executing.
template <typename T>
class RefCountedMessage : public base::RefCounted<RefCountedMessage<T> > {
 public:
  RefCountedMessage(T* message) : message_(message) { }

  T* message() { return message_.get(); }

 private:
  scoped_ptr<T> message_;
};

// Dummy methods to destroy messages.
template <class T>
static void DeleteMessage(scoped_refptr<T> message) { }

template <class T>
static Task* NewDeleteTask(scoped_refptr<T> message) {
  return NewRunnableFunction(&DeleteMessage<T>, message);
}

}  // namespace

namespace remoting {
namespace protocol {

HostMessageDispatcher::HostMessageDispatcher() :
    host_stub_(NULL),
    input_stub_(NULL) {
}

HostMessageDispatcher::~HostMessageDispatcher() {
}

bool HostMessageDispatcher::Initialize(
    protocol::Session* session,
    HostStub* host_stub, InputStub* input_stub) {
  if (!session || !host_stub || !input_stub ||
      !session->event_channel() || !session->control_channel()) {
    return false;
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
  return true;
}

void HostMessageDispatcher::OnControlMessageReceived(ControlMessage* message) {
  scoped_refptr<RefCountedMessage<ControlMessage> > ref_msg =
      new RefCountedMessage<ControlMessage>(message);
  if (message->has_suggest_resolution()) {
    host_stub_->SuggestResolution(
        message->suggest_resolution(), NewDeleteTask(ref_msg));
  }
}

void HostMessageDispatcher::OnEventMessageReceived(
    EventMessage* message) {
  // TODO(hclam): Implement.
}

}  // namespace protocol
}  // namespace remoting
