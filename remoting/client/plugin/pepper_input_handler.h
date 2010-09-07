// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_INPUT_HANDLER_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_INPUT_HANDLER_H_

#include "remoting/client/input_handler.h"

#include "third_party/ppapi/c/pp_event.h"

namespace remoting {

class PepperInputHandler : public InputHandler {
 public:
  PepperInputHandler(ClientContext* context,
                     HostConnection* connection,
                     ChromotingView* view);
  virtual ~PepperInputHandler();

  void Initialize();

  void HandleKeyEvent(bool keydown, const PP_Event_Key& event);
  void HandleCharacterEvent(const PP_Event_Character& event);

  void HandleMouseMoveEvent(const PP_Event_Mouse& event);
  void HandleMouseButtonEvent(bool button_down,
                              const PP_Event_Mouse& event);

 private:
  DISALLOW_COPY_AND_ASSIGN(PepperInputHandler);
};

}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::PepperInputHandler);

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_INPUT_HANDLER_H_
