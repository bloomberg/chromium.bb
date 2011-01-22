// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CLIENT_MESSAGE_DISPATCHER_H_
#define REMOTING_PROTOCOL_CLIENT_MESSAGE_DISPATCHER_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "remoting/protocol/message_reader.h"

namespace remoting {

class EventMessage;

namespace protocol {

class ClientStub;
class ControlMessage;
class Session;

// A message dispatcher used to listen for messages received in
// protocol::Session. It dispatches messages to the corresponding
// handler.
//
// Internally it contains an EventStreamReader that decodes data on
// communications channels into protocol buffer messages.
// EventStreamReader is registered with protocol::Session given to it.
//
// Object of this class is owned by ConnectionToHost.
class ClientMessageDispatcher {
 public:
  // Construct a message dispatcher.
  ClientMessageDispatcher();
  virtual ~ClientMessageDispatcher();

  // Initialize the message dispatcher with the given connection and
  // message handlers.
  void Initialize(protocol::Session* session, ClientStub* client_stub);

 private:
  void OnControlMessageReceived(ControlMessage* message, Task* done_task);

  // MessageReader that runs on the control channel. It runs a loop
  // that parses data on the channel and then calls the corresponding handler
  // in this class.
  scoped_ptr<ProtobufMessageReader<ControlMessage> > control_message_reader_;

  // Stubs for client and input. These objects are not owned.
  // They are called on the thread there data is received, i.e. jingle thread.
  ClientStub* client_stub_;

  DISALLOW_COPY_AND_ASSIGN(ClientMessageDispatcher);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CLIENT_MESSAGE_DISPATCHER_H_
