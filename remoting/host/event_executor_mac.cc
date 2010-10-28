// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/event_executor_mac.h"

#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/message_decoder.h"

namespace remoting {

EventExecutorMac::EventExecutorMac(Capturer* capturer)
  : EventExecutor(capturer) {
}

EventExecutorMac::~EventExecutorMac() {
}

void EventExecutorMac::HandleInputEvent(ChromotingClientMessage* message) {
  delete message;
}

}  // namespace remoting
