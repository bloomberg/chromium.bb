// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/event_executor_mac.h"

namespace remoting {

EventExecutorMac::EventExecutorMac(Capturer* capturer)
  : EventExecutor(capturer) {
}

EventExecutorMac::~EventExecutorMac() {
}

void EventExecutorMac::HandleInputEvents(ClientMessageList* messages) {
}

}  // namespace remoting
