// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_FOCUS_ACCELERATOR_HANDLER_H_
#define UI_VIEWS_FOCUS_ACCELERATOR_HANDLER_H_
#pragma once

#include "build/build_config.h"

#if defined(OS_LINUX)
#include <gdk/gdk.h>
#endif

#include <set>
#include <vector>

#include "base/message_loop.h"

namespace ui {

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

////////////////////////////////////////////////////////////////////////////////
// AcceleratorHandler class
//
//  An object that pre-screens all UI messages for potential accelerators.
//  Registered accelerators are processed regardless of focus within a given
//  Widget or Window.
//
//  This processing is done at the Dispatcher level rather than on the Widget
//  because of the global nature of this processing, and the fact that not all
//  controls within a window need to be Widgets - some are native controls from
//  the underlying toolkit wrapped by NativeViewHost.
//
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
  virtual MessagePumpGlibXDispatcher::DispatchStatus Dispatch(XEvent* xev);
#endif
#endif

 private:
#if defined(OS_WIN)
  // The keys currently pressed and consumed by the FocusManager.
  std::set<WPARAM> pressed_keys_;
#endif

  DISALLOW_COPY_AND_ASSIGN(AcceleratorHandler);
};

}  // namespace ui

#endif  // UI_VIEWS_FOCUS_ACCELERATOR_HANDLER_H_
