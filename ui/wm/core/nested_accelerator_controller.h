// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_NESTED_ACCELERATOR_CONTROLLER_H_
#define UI_WM_CORE_NESTED_ACCELERATOR_CONTROLLER_H_

#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "ui/wm/public/dispatcher_client.h"
#include "ui/wm/wm_export.h"

namespace wm {

class NestedAcceleratorDelegate;
class NestedAcceleratorDispatcher;

// Creates a dispatcher which wraps another dispatcher.
// The outer dispatcher runs first and performs ash specific handling.
// If it does not consume the event it forwards the event to the nested
// dispatcher.
class WM_EXPORT NestedAcceleratorController
    : public aura::client::DispatcherClient {
 public:
  explicit NestedAcceleratorController(NestedAcceleratorDelegate* delegate);
  virtual ~NestedAcceleratorController();

  // aura::client::DispatcherClient:
  virtual void RunWithDispatcher(
      base::MessagePumpDispatcher* dispatcher) OVERRIDE;
  virtual void QuitNestedMessageLoop() OVERRIDE;

 private:
  base::Closure quit_closure_;
  scoped_ptr<NestedAcceleratorDispatcher> accelerator_dispatcher_;
  scoped_ptr<NestedAcceleratorDelegate> dispatcher_delegate_;

  DISALLOW_COPY_AND_ASSIGN(NestedAcceleratorController);
};

}  // namespace wm

#endif  // UI_WM_CORE_NESTED_ACCELERATOR_CONTROLLER_H_
