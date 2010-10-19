// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_HOST_CONTROL_MESSAGE_HANDLER_H_
#define REMOTING_PROTOCOL_HOST_CONTROL_MESSAGE_HANDLER_H_

#include "remoting/proto/control.pb.h"

class Task;

namespace remoting {

// The interface for handling control messages sent to the host.
// For all methods of this interface. |task| needs to be called when
// receiver is done processing the event.
class HostControlMessageHandler {
 public:
  virtual ~HostControlMessageHandler() {}

  virtual void OnSuggestScreenResolutionRequest(
      const SuggestScreenResolutionRequest& request, Task* task) = 0;
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_HOST_CONTROL_MESSAGE_HANDLER_H_
