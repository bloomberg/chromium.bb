// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_HOST_MESSAGE_DISPATCHER_H_
#define REMOTING_PROTOCOL_HOST_MESSAGE_DISPATCHER_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "remoting/protocol/message_reader.h"

namespace remoting {
namespace protocol {

class ControlMessage;
class EventMessage;
class HostStub;
class InputStub;
class Session;

// A message dispatcher used to listen for messages received in
// protocol::Session. It dispatches messages to the corresponding
// handler.
//
// Internally it contains an EventStreamReader that decodes data on
// communications channels into protocol buffer messages.
// EventStreamReader is registered with protocol::Session given to it.
//
// Object of this class is owned by ConnectionToClient to dispatch messages
// to itself.
class HostMessageDispatcher {
 public:
  // Construct a message dispatcher.
  HostMessageDispatcher();
  virtual ~HostMessageDispatcher();

  // Initialize the message dispatcher with the given connection and
  // message handlers.
  void Initialize(protocol::Session* session,
                  HostStub* host_stub, InputStub* input_stub);

 private:
  // This method is called by |control_channel_reader_| when a control
  // message is received.
  void OnControlMessageReceived(ControlMessage* message, Task* done_task);

  // This method is called by |event_channel_reader_| when a event
  // message is received.
  void OnEventMessageReceived(EventMessage* message, Task* done_task);

  // MessageReader that runs on the control channel. It runs a loop
  // that parses data on the channel and then delegates the message to this
  // class.
  scoped_ptr<ProtobufMessageReader<ControlMessage> > control_message_reader_;

  // MessageReader that runs on the event channel.
  scoped_ptr<ProtobufMessageReader<EventMessage> > event_message_reader_;

  // Stubs for host and input. These objects are not owned.
  // They are called on the thread there data is received, i.e. jingle thread.
  HostStub* host_stub_;
  InputStub* input_stub_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_HOST_MESSAGE_DISPATCHER_H_
