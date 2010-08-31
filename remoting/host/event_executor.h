// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_EVENT_EXECUTOR_H_
#define REMOTING_HOST_EVENT_EXECUTOR_H_

#include <vector>

#include "remoting/base/protocol_decoder.h"

namespace remoting {

class Capturer;

// An interface that defines the behavior of an event executor object.
// An event executor is to perform actions on the host machine. For example
// moving the mouse cursor, generating keyboard events and manipulating
// clipboards.
class EventExecutor {
 public:
  EventExecutor(Capturer* capturer)
    : capturer_(capturer) {
  }
  virtual ~EventExecutor() {}

  // Handles input events from ClientMessageList and removes them from the
  // list.
  virtual void HandleInputEvents(ClientMessageList* messages) = 0;
  // TODO(hclam): Define actions for clipboards.

 protected:
  Capturer* capturer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EventExecutor);
};

}  // namespace remoting

#endif  // REMOTING_HOST_EVENT_EXECUTOR_H_
