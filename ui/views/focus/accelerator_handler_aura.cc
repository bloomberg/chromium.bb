// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/focus/accelerator_handler.h"

namespace views {

AcceleratorHandler::AcceleratorHandler() {
}

#if defined(OS_WIN)
bool AcceleratorHandler::Dispatch(const MSG& msg) {
  TranslateMessage(&msg);
  DispatchMessage(&msg);
  return true;
}
#else
base::MessagePumpDispatcher::DispatchStatus AcceleratorHandler::Dispatch(
    XEvent*) {
  return base::MessagePumpDispatcher::EVENT_IGNORED;
}

bool DispatchXEvent(XEvent* xev) {
  return false;
}
#endif  // defined(OS_WIN)

}  // namespace views
