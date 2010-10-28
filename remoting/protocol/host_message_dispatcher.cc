// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/io_buffer.h"
#include "remoting/base/multiple_array_input_stream.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/chromotocol_connection.h"
#include "remoting/protocol/host_message_dispatcher.h"
#include "remoting/protocol/host_control_message_handler.h"
#include "remoting/protocol/host_event_message_handler.h"
#include "remoting/protocol/message_reader.h"

namespace remoting {

HostMessageDispatcher::HostMessageDispatcher() {
}

HostMessageDispatcher::~HostMessageDispatcher() {
}

bool HostMessageDispatcher::Initialize(
    ChromotocolConnection* connection,
    HostControlMessageHandler* control_message_handler,
    HostEventMessageHandler* event_message_handler) {
  if (!connection || !control_message_handler || !event_message_handler ||
      !connection->event_channel() || !connection->control_channel()) {
    return false;
  }

  control_message_reader_.reset(new MessageReader());
  event_message_reader_.reset(new MessageReader());
  control_message_handler_.reset(control_message_handler);
  event_message_handler_.reset(event_message_handler);

  // Initialize the readers on the sockets provided by channels.
  event_message_reader_->Init<ClientEventMessage>(
      connection->event_channel(),
      NewCallback(this, &HostMessageDispatcher::OnEventMessageReceived));
  control_message_reader_->Init<ClientControlMessage>(
      connection->control_channel(),
      NewCallback(this, &HostMessageDispatcher::OnControlMessageReceived));
  return true;
}

void HostMessageDispatcher::OnControlMessageReceived(
    ClientControlMessage* message) {
  scoped_refptr<RefCountedMessage<ClientControlMessage> > ref_msg =
      new RefCountedMessage<ClientControlMessage>(message);
  if (message->has_suggest_screen_resolution_request()) {
    control_message_handler_->OnSuggestScreenResolutionRequest(
        message->suggest_screen_resolution_request(),
        NewRunnableFunction(
            &DeleteMessage<RefCountedMessage<ClientControlMessage> >,
            ref_msg));
  }
}

void HostMessageDispatcher::OnEventMessageReceived(
    ClientEventMessage* message) {
  // TODO(hclam): Implement.
}

}  // namespace remoting
