// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_FOCUS_ACCELERATOR_HANDLER_H_
#define VIEWS_FOCUS_ACCELERATOR_HANDLER_H_

#if defined(OS_LINUX)
#include <gdk/gdk.h>
#endif

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
#if defined(OS_LINUX)
  // Last key pressed and consumed as an accelerator.
  guint last_key_pressed_;
#endif

  DISALLOW_COPY_AND_ASSIGN(AcceleratorHandler);
};

}  // namespace views

#endif  // VIEWS_FOCUS_ACCELERATOR_HANDLER_H_
