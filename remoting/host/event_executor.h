// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_EVENT_EXECUTOR_H_
#define REMOTING_HOST_EVENT_EXECUTOR_H_

#include "remoting/protocol/input_stub.h"

class MessageLoop;

namespace remoting {

class Capturer;

class EventExecutor : public protocol::InputStub {
 public:
  // Creates default event executor for the current platform.
  static EventExecutor* Create(MessageLoop* message_loop,
                               Capturer* capturer);
};

}  // namespace remoting

#endif  // REMOTING_HOST_EVENT_EXECUTOR_H_
