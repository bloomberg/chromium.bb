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

#include "base/message_loop.h"

namespace views {

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
#endif

 private:
#if defined(OS_WIN)
  // The keys currently pressed and consumed by the FocusManager.
  std::set<WPARAM> pressed_keys_;
#else  // OS_LINUX
  // The set of hardware keycodes that are currently pressed.  We use this
  // to keep track of whether or not a key is being pressed in isolation
  // or not. We must use hardware keycodes because higher-level keycodes
  // may change between press and release depending on what modifier keys
  // are pressed in the interim.
  std::set<int> pressed_hardware_keys_;
  // The set of vkey codes that were consumed by the FocusManager.
  std::set<int> consumed_vkeys_;
  // True if only the menu key (Alt key) was pressed, and no other keys.
  bool only_menu_pressed_;
#endif

  DISALLOW_COPY_AND_ASSIGN(AcceleratorHandler);
};

}  // namespace views

#endif  // VIEWS_FOCUS_ACCELERATOR_HANDLER_H_
