// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This stub is thread safe because of the use of BufferedSocketWriter.
// BufferedSocketWriter buffers messages and send them on them right thread.

#include "remoting/protocol/input_sender.h"

#include "base/task.h"
#include "remoting/proto/event.pb.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/buffered_socket_writer.h"
#include "remoting/protocol/util.h"

namespace remoting {
namespace protocol {

InputSender::InputSender(net::Socket* socket)
    : buffered_writer_(new BufferedSocketWriter()) {
  // TODO(garykac) Set write failed callback.
  DCHECK(socket);
  buffered_writer_->Init(socket, NULL);
}

InputSender::~InputSender() {
}

void InputSender::InjectKeyEvent(const KeyEvent* event, Task* done) {
  EventMessage message;
  // TODO(hclam): Provide timestamp.
  message.set_timestamp(0);
  message.mutable_key_event()->CopyFrom(*event);
  buffered_writer_->Write(SerializeAndFrameMessage(message), done);
}

void InputSender::InjectMouseEvent(const MouseEvent* event, Task* done) {
  EventMessage message;
  // TODO(hclam): Provide timestamp.
  message.set_timestamp(0);
  message.mutable_mouse_event()->CopyFrom(*event);
  buffered_writer_->Write(SerializeAndFrameMessage(message), done);
}

}  // namespace protocol
}  // namespace remoting
