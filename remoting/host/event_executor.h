// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_EVENT_EXECUTOR_H_
#define REMOTING_HOST_EVENT_EXECUTOR_H_

#include "base/memory/scoped_ptr.h"
#include "remoting/protocol/host_event_stub.h"

class MessageLoop;

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace remoting {

class Capturer;

class EventExecutor : public protocol::HostEventStub {
 public:
  // Creates a default event executor for the current platform.
  // This object should do as much work as possible on |message_loop|, using
  // |ui_loop| only when necessary.
  static scoped_ptr<EventExecutor> Create(MessageLoop* message_loop,
                                          base::MessageLoopProxy* ui_loop,
                                          Capturer* capturer);

  // Initialises any objects needed to execute events.
  virtual void OnSessionStarted() = 0;

  // Destroys any objects constructed by Start().
  virtual void OnSessionFinished() = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_EVENT_EXECUTOR_H_
