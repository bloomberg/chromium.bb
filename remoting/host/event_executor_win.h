// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_EVENT_EXECUTOR_WIN_H_
#define REMOTING_HOST_EVENT_EXECUTOR_WIN_H_

#include <vector>

#include "remoting/host/event_executor.h"

namespace remoting {

// A class to generate events on Windows.
class EventExecutorWin : public EventExecutor {
 public:
  EventExecutorWin();
  virtual ~EventExecutorWin();

  virtual void HandleInputEvents(ClientMessageList* messages);

 private:
  DISALLOW_COPY_AND_ASSIGN(EventExecutorWin);
};

}  // namespace remoting

#endif  // REMOTING_HOST_EVENT_EXECUTOR_WIN_H_
