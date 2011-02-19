// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/event_executor_mac.h"

#include "base/message_loop.h"
#include "base/task.h"
#include "remoting/protocol/message_decoder.h"

namespace remoting {

using protocol::MouseEvent;
using protocol::KeyEvent;

EventExecutorMac::EventExecutorMac(
    MessageLoopForUI* message_loop, Capturer* capturer)
    : message_loop_(message_loop),
      capturer_(capturer) {
}

EventExecutorMac::~EventExecutorMac() {
}

void EventExecutorMac::InjectKeyEvent(const KeyEvent* event, Task* done) {
  done->Run();
  delete done;
}

void EventExecutorMac::InjectMouseEvent(const MouseEvent* event, Task* done) {
  done->Run();
  delete done;
}

protocol::InputStub* CreateEventExecutor(MessageLoopForUI* message_loop,
                                         Capturer* capturer) {
  return new EventExecutorMac(message_loop, capturer);
}

}  // namespace remoting
