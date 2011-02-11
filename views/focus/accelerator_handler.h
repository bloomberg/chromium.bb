// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_FOCUS_ACCELERATOR_HANDLER_H_
#define VIEWS_FOCUS_ACCELERATOR_HANDLER_H_
#pragma once

#include "build/build_config.h"

#if defined(OS_LINUX)
#include <gdk/gdk.h>
#endif

#include <set>
#include <vector>

#include "base/message_loop.h"

namespace views {

#if defined(TOUCH_UI)
// Dispatch an XEvent to the RootView. Return true if the event was dispatched
// and handled, false otherwise.
bool DispatchXEvent(XEvent* xevent);

#if defined(HAVE_XINPUT2)
// Keep a list of touch devices so that it is possible to determine if a pointer
// event is a touch-event or a mouse-event.
void SetTouchDeviceList(std::vector<unsigned int>& devices);
#endif  // HAVE_XINPUT2
#endif  // TOUCH_UI

// This class delegates the key messages to the associated FocusManager class
// for the window that is receiving these messages for accelerator processing.
class AcceleratorHandler : public MessageLoopForUI::Dispatcher {
 public:
  AcceleratorHandler();

  // Dispatcher method. This returns true if an accelerator was processed by the
  // focus manager
#if defined(OS_WIN)
  virtual bool Dispatch(const MSG& msg);
#else
  virtual bool Dispatch(GdkEvent* event);
#if defined(TOUCH_UI)
  virtual MessagePumpGlibXDispatcher::DispatchStatus DispatchX(XEvent* xev);
#endif
#endif

 private:
#if defined(OS_WIN)
  // The keys currently pressed and consumed by the FocusManager.
  std::set<WPARAM> pressed_keys_;
#endif

  DISALLOW_COPY_AND_ASSIGN(AcceleratorHandler);
};

}  // namespace views

#endif  // VIEWS_FOCUS_ACCELERATOR_HANDLER_H_
