// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_EVENT_EXECUTOR_MAC_H_
#define REMOTING_HOST_EVENT_EXECUTOR_MAC_H_

#include <vector>

#include "base/basictypes.h"
#include "remoting/protocol/input_stub.h"

class MessageLoop;

namespace remoting {

class Capturer;
class ChromotingClientMessage;

// A class to generate events on Mac.
class EventExecutorMac : public protocol::InputStub {
 public:
  EventExecutorMac(MessageLoop* message_loop, Capturer* capturer);
  virtual ~EventExecutorMac();

  virtual void InjectKeyEvent(const KeyEvent* event, Task* done);
  virtual void InjectMouseEvent(const MouseEvent* event, Task* done);

 private:
  MessageLoop* message_loop_;
  Capturer* capturer_;

  DISALLOW_COPY_AND_ASSIGN(EventExecutorMac);
};

}  // namespace remoting

#endif  // REMOTING_HOST_EVENT_EXECUTOR_MAC_H_
