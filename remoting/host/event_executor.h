// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_EVENT_EXECUTOR_H_
#define REMOTING_HOST_EVENT_EXECUTOR_H_

#include "base/memory/scoped_ptr.h"
#include "remoting/protocol/host_event_stub.h"

class MessageLoop;

namespace remoting {

class Capturer;

class EventExecutor : public protocol::HostEventStub {
 public:
  // Creates default event executor for the current platform.
  // Does not take ownership of |message_loop| or |capturer|.
  static scoped_ptr<protocol::HostEventStub> Create(MessageLoop* message_loop,
                                                    Capturer* capturer);
};

}  // namespace remoting

#endif  // REMOTING_HOST_EVENT_EXECUTOR_H_
