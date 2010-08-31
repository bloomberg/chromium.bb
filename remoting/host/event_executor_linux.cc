// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/event_executor_linux.h"

namespace remoting {

EventExecutorLinux::EventExecutorLinux(Capturer* capturer)
  : EventExecutor(capturer) {
}

EventExecutorLinux::~EventExecutorLinux() {
}

void EventExecutorLinux::HandleInputEvents(ClientMessageList* messages) {
}

}  // namespace remoting
