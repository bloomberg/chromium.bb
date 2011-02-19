// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_EVENT_EXECUTOR_LINUX_H_
#define REMOTING_HOST_EVENT_EXECUTOR_LINUX_H_

#include <vector>

#include "base/task.h"
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "remoting/host/event_executor.h"
#include "remoting/protocol/input_stub.h"

namespace remoting {

class EventExecutorLinuxPimpl;

// A class to generate events on Linux.
class EventExecutorLinux : public protocol::InputStub {
 public:
  EventExecutorLinux(MessageLoopForUI* message_loop,
                     Capturer* capturer);
  virtual ~EventExecutorLinux();

  virtual void InjectKeyEvent(const protocol::KeyEvent* event, Task* done);
  virtual void InjectMouseEvent(const protocol::MouseEvent* event, Task* done);

 private:
  MessageLoopForUI* message_loop_;
  Capturer* capturer_;
  scoped_ptr<EventExecutorLinuxPimpl> pimpl_;

  DISALLOW_COPY_AND_ASSIGN(EventExecutorLinux);
};

}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::EventExecutorLinux);

#endif  // REMOTING_HOST_EVENT_EXECUTOR_LINUX_H_
