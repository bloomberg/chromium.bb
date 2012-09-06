// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_EVENT_EXECUTOR_FAKE_H_
#define REMOTING_HOST_EVENT_EXECUTOR_FAKE_H_

#include "remoting/host/event_executor.h"

namespace remoting {

// A dummy implementation of |EventExecutor| that does nothing.
class EventExecutorFake : public EventExecutor {
 public:
  EventExecutorFake();
  virtual ~EventExecutorFake();

  // ClipboardStub interface.
  virtual void InjectClipboardEvent(
      const protocol::ClipboardEvent& event) OVERRIDE;

  // InputStub interface.
  virtual void InjectKeyEvent(const protocol::KeyEvent& event) OVERRIDE;
  virtual void InjectMouseEvent(const protocol::MouseEvent& event) OVERRIDE;

  virtual void Start(
      scoped_ptr<protocol::ClipboardStub> client_clipboard) OVERRIDE;
  virtual void StopAndDelete() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(EventExecutorFake);
};

}  // namespace remoting

#endif  // REMOTING_HOST_EVENT_EXECUTOR_FAKE_H_
