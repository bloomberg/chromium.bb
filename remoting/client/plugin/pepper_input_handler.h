// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_INPUT_HANDLER_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_INPUT_HANDLER_H_

#include "remoting/client/input_handler.h"

namespace remoting {

class PepperInputHandler : public InputHandler {
 public:
  PepperInputHandler();
  virtual ~PepperInputHandler();

  void Initialize();

 private:
  DISALLOW_COPY_AND_ASSIGN(PepperInputHandler);
};

}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::PepperInputHandler);

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_INPUT_HANDLER_H_
