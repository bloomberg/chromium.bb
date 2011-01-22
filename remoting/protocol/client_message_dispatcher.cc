// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ref_counted.h"
#include "net/base/io_buffer.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/client_message_dispatcher.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/message_reader.h"
#include "remoting/protocol/session.h"

namespace remoting {
namespace protocol {

ClientMessageDispatcher::ClientMessageDispatcher() : client_stub_(NULL) {
}

ClientMessageDispatcher::~ClientMessageDispatcher() {
}

void ClientMessageDispatcher::Initialize(
    protocol::Session* session, ClientStub* client_stub) {
  if (!session || !client_stub || !session->control_channel()) {
    return;
  }

  control_message_reader_.reset(new ProtobufMessageReader<ControlMessage>());
  client_stub_ = client_stub;

  control_message_reader_->Init(
      session->control_channel(),
      NewCallback(this, &ClientMessageDispatcher::OnControlMessageReceived));
  return;
}

void ClientMessageDispatcher::OnControlMessageReceived(
    ControlMessage* message, Task* done_task) {
  // TODO(sergeyu): Add message validation.
  if (message->has_notify_resolution()) {
    client_stub_->NotifyResolution(
        &message->notify_resolution(), done_task);
  } else if (message->has_begin_session_response()) {
    client_stub_->BeginSessionResponse(
        &message->begin_session_response().login_status(), done_task);
  } else {
    LOG(WARNING) << "Invalid control message received.";
    done_task->Run();
    delete done_task;
  }
}

}  // namespace protocol
}  // namespace remoting
