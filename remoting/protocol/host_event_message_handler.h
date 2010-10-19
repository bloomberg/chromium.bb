// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_HOST_EVENT_MESSAGE_HANDLER_H_
#define REMOTING_PROTOCOL_HOST_EVENT_MESSAGE_HANDLER_H_

#include "base/basictypes.h"
#include "remoting/proto/event.pb.h"

class Task;

namespace remoting {

// The interface for handling event messages sent to the host.
// For all methods of this interface. |task| needs to be called when
// receiver is done processing the event.
class HostEventMessageHandler {
 public:
  virtual ~HostEventMessageHandler() {}

  virtual void OnKeyEvent(int32 timestamp,
                          const KeyEvent& event, Task* task) = 0;
  virtual void OnMouseEvent(int32 timestamp, const MouseEvent& event,
                            Task* task) = 0;
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_HOST_EVENT_MESSAGE_HANDLER_H_
