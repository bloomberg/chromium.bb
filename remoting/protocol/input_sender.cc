// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This stub is thread safe because of the use of BufferedSocketWriter.
// BufferedSocketWriter buffers messages and send them on the right thread.

#include "remoting/protocol/input_sender.h"

#include "base/task.h"
#include "remoting/proto/event.pb.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/buffered_socket_writer.h"
#include "remoting/protocol/util.h"

namespace remoting {
namespace protocol {

InputSender::InputSender(base::MessageLoopProxy* message_loop,
                         net::Socket* socket)
    : buffered_writer_(new BufferedSocketWriter(message_loop)) {
  // TODO(garykac) Set write failed callback.
  DCHECK(socket);
  buffered_writer_->Init(socket, NULL);
}

InputSender::~InputSender() {
}

void InputSender::InjectKeyEvent(const KeyEvent& event) {
  EventMessage message;
  message.set_sequence_number(base::Time::Now().ToInternalValue());
  message.mutable_key_event()->CopyFrom(event);
  buffered_writer_->Write(SerializeAndFrameMessage(message), NULL);
}

void InputSender::InjectMouseEvent(const MouseEvent& event) {
  EventMessage message;
  message.set_sequence_number(base::Time::Now().ToInternalValue());
  message.mutable_mouse_event()->CopyFrom(event);
  buffered_writer_->Write(SerializeAndFrameMessage(message), NULL);
}

void InputSender::Close() {
  buffered_writer_->Close();
}

}  // namespace protocol
}  // namespace remoting
