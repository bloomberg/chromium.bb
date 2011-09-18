// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "views/views_export.h"

namespace views {

#if (defined(TOUCH_UI) && !defined(USE_WAYLAND)) \
    || (!defined(WIN_OS) && defined(USE_AURA))
// Dispatch an XEvent to the RootView. Return true if the event was dispatched
// and handled, false otherwise.
bool VIEWS_EXPORT DispatchXEvent(XEvent* xevent);

// Keep a list of touch devices so that it is possible to determine if a pointer
// event is a touch-event or a mouse-event.
void VIEWS_EXPORT SetTouchDeviceList(std::vector<unsigned int>& devices);
#endif  // TOUCH_UI

// This class delegates the key messages to the associated FocusManager class
// for the window that is receiving these messages for accelerator processing.
class VIEWS_EXPORT AcceleratorHandler : public MessageLoop::Dispatcher {
 public:
  AcceleratorHandler();

  // Dispatcher method. This returns true if an accelerator was processed by the
  // focus manager
#if defined(OS_WIN)
  virtual bool Dispatch(const MSG& msg);
#elif defined(USE_WAYLAND)
  virtual base::MessagePumpDispatcher::DispatchStatus Dispatch(
      ui::WaylandEvent* ev);
#elif defined(TOUCH_UI) || defined(USE_AURA)
  virtual base::MessagePumpDispatcher::DispatchStatus Dispatch(XEvent* xev);
#else
  virtual bool Dispatch(GdkEvent* event);
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
