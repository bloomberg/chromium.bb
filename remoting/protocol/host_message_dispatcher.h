// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_HOST_MESSAGE_DISPATCHER_H_
#define REMOTING_PROTOCOL_HOST_MESSAGE_DISPATCHER_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace remoting {

class ChromotingClientMessage;
class ChromotingConnection;
class EventStreamReader;
class HostControlMessageHandler;
class HostEventMessageHandler;

// A message dispatcher used to listen for messages received in
// ChromotingConnection. It dispatches messages to the corresponding
// handler.
//
// Internally it contains an EventStreamReader that decodes data on
// communications channels into protocol buffer messages.
// EventStreamReader is registered with ChromotingConnection given to it.
//
// Object of this class is owned by ChromotingHost to dispatch messages
// to itself.
class HostMessageDispatcher {
 public:
  // Construct a message dispatcher that dispatches messages received
  // in ChromotingConnection.
  HostMessageDispatcher(base::MessageLoopProxy* message_loop_proxy,
                        ChromotingConnection* connection,
                        HostControlMessageHandler* control_message_handler,
                        HostEventMessageHandler* event_message_handler);

  virtual ~HostMessageDispatcher();

 private:
  // This method is called by |control_channel_reader_| when a control
  // message is received.
  void OnControlMessageReceived(ChromotingClientMessage* message);

  // This method is called by |event_channel_reader_| when a event
  // message is received.
  void OnEventMessageReceived(ChromotingClientMessage* message);

  // Message loop to dispatch the messages.
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;

  // EventStreamReader that runs on the control channel. It runs a loop
  // that parses data on the channel and then delegate the message to this
  // class.
  scoped_ptr<EventStreamReader> control_channel_reader_;

  // EventStreamReader that runs on the event channel.
  scoped_ptr<EventStreamReader> event_channel_reader_;

  // Event handlers for control channel and event channel respectively.
  // Method calls to these objects are made on the message loop given.
  scoped_ptr<HostControlMessageHandler> control_message_handler_;
  scoped_ptr<HostEventMessageHandler> event_message_handler_;
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_HOST_MESSAGE_DISPATCHER_H_
