// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/event_executor_fake.h"

namespace remoting {

EventExecutorFake::EventExecutorFake() {
}

EventExecutorFake::~EventExecutorFake() {
}

void EventExecutorFake::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
}

void EventExecutorFake::InjectKeyEvent(const protocol::KeyEvent& event) {
}

void EventExecutorFake::InjectMouseEvent(const protocol::MouseEvent& event) {
}

void EventExecutorFake::Start(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
}

void EventExecutorFake::StopAndDelete() {
  delete this;
}

}  // namespace remoting
