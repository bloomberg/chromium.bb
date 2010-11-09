// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This stub is thread safe because of the use of BufferedSocketWriter.
// BufferedSocketWriter buffers messages and send them on them right thread.

#include "remoting/protocol/input_sender.h"

#include "base/task.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/buffered_socket_writer.h"
#include "remoting/protocol/util.h"

namespace remoting {
namespace protocol {

InputSender::InputSender(net::Socket* socket)
    : buffered_writer_(new BufferedSocketWriter()) {
  buffered_writer_->Init(socket, NULL);
}

InputSender::~InputSender() {
}

void InputSender::InjectKeyEvent(const KeyEvent* event, Task* done) {
  EventMessage message;
  Event* evt = message.add_event();
  // TODO(hclam): Provide timestamp.
  evt->set_timestamp(0);
  evt->mutable_key()->CopyFrom(*event);
  buffered_writer_->Write(SerializeAndFrameMessage(message));
  done->Run();
  delete done;
}

void InputSender::InjectMouseEvent(const MouseEvent* event, Task* done) {
  EventMessage message;
  Event* evt = message.add_event();
  // TODO(hclam): Provide timestamp.
  evt->set_timestamp(0);
  evt->mutable_mouse()->CopyFrom(*event);
  buffered_writer_->Write(SerializeAndFrameMessage(message));
  done->Run();
  delete done;
}

}  // namespace protocol
}  // namespace remoting
