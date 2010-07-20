// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_INPUT_HANDLER_H_
#define REMOTING_CLIENT_INPUT_HANDLER_H_

#include "base/basictypes.h"
#include "base/task.h"

namespace remoting {

class InputHandler {
 public:
  InputHandler() {}
  virtual ~InputHandler() {}

  virtual void Initialize() = 0;

 protected:
  DISALLOW_COPY_AND_ASSIGN(InputHandler);
};

}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::InputHandler);

#endif  // REMOTING_CLIENT_INPUT_HANDLER_H_
