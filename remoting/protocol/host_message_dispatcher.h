// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_HOST_MESSAGE_DISPATCHER_H_
#define REMOTING_PROTOCOL_HOST_MESSAGE_DISPATCHER_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"

namespace remoting {

class ChromotocolConnection;
class ClientControlMessage;
class ClientEventMessage;
class HostControlMessageHandler;
class HostEventMessageHandler;
class MessageReader;

// A message dispatcher used to listen for messages received in
// ChromotocolConnection. It dispatches messages to the corresponding
// handler.
//
// Internally it contains an EventStreamReader that decodes data on
// communications channels into protocol buffer messages.
// EventStreamReader is registered with ChromotocolConnection given to it.
//
// Object of this class is owned by ChromotingHost to dispatch messages
// to itself.
class HostMessageDispatcher :
      public base::RefCountedThreadSafe<HostMessageDispatcher> {
 public:
  // Construct a message dispatcher.
  HostMessageDispatcher();
  virtual ~HostMessageDispatcher();

  // Initialize the message dispatcher with the given connection and
  // message handlers.
  // Return true if initalization was successful.
  bool Initialize(ChromotocolConnection* connection,
                  HostControlMessageHandler* control_message_handler,
                  HostEventMessageHandler* event_message_handler);

 private:
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

  // This method is called by |control_channel_reader_| when a control
  // message is received.
  void OnControlMessageReceived(ClientControlMessage* message);

  // This method is called by |event_channel_reader_| when a event
  // message is received.
  void OnEventMessageReceived(ClientEventMessage* message);

  // Dummy methods to destroy messages.
  template <class T>
  static void DeleteMessage(scoped_refptr<T> message) { }

  // MessageReader that runs on the control channel. It runs a loop
  // that parses data on the channel and then delegates the message to this
  // class.
  scoped_ptr<MessageReader> control_message_reader_;

  // MessageReader that runs on the event channel.
  scoped_ptr<MessageReader> event_message_reader_;

  // Event handlers for control channel and event channel respectively.
  // Method calls to these objects are made on the message loop given.
  scoped_ptr<HostControlMessageHandler> control_message_handler_;
  scoped_ptr<HostEventMessageHandler> event_message_handler_;
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_HOST_MESSAGE_DISPATCHER_H_
