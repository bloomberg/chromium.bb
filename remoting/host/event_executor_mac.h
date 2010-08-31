// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_EVENT_EXECUTOR_MAC_H_
#define REMOTING_HOST_EVENT_EXECUTOR_MAC_H_

#include <vector>

#include "remoting/host/event_executor.h"

namespace remoting {

// A class to generate events on Mac.
class EventExecutorMac : public EventExecutor {
 public:
  EventExecutorMac(Capturer* capturer);
  virtual ~EventExecutorMac();

  virtual void HandleInputEvents(ClientMessageList* messages);

 private:
  DISALLOW_COPY_AND_ASSIGN(EventExecutorMac);
};

}  // namespace remoting

#endif  // REMOTING_HOST_EVENT_EXECUTOR_MAC_H_
