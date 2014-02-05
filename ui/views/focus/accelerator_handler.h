// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_FOCUS_ACCELERATOR_HANDLER_H_
#define UI_VIEWS_FOCUS_ACCELERATOR_HANDLER_H_

#include "build/build_config.h"

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_pump_dispatcher.h"
#include "ui/views/views_export.h"

namespace views {

// This class delegates the key messages to the associated FocusManager class
// for the window that is receiving these messages for accelerator processing.
class VIEWS_EXPORT AcceleratorHandler : public base::MessagePumpDispatcher {
 public:
  AcceleratorHandler();

  // Dispatcher method. This returns true if an accelerator was processed by the
  // focus manager
  virtual bool Dispatch(const base::NativeEvent& event) OVERRIDE;

 private:
#if defined(OS_WIN)
  // The keys currently pressed and consumed by the FocusManager.
  std::set<WPARAM> pressed_keys_;
#endif

  DISALLOW_COPY_AND_ASSIGN(AcceleratorHandler);
};

}  // namespace views

#endif  // UI_VIEWS_FOCUS_ACCELERATOR_HANDLER_H_
