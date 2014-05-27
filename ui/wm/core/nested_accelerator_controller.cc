// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/nested_accelerator_controller.h"

#include "base/auto_reset.h"
#include "base/run_loop.h"
#include "ui/wm/core/nested_accelerator_delegate.h"
#include "ui/wm/core/nested_accelerator_dispatcher.h"

namespace wm {

NestedAcceleratorController::NestedAcceleratorController(
    NestedAcceleratorDelegate* delegate)
    : dispatcher_delegate_(delegate) {
  DCHECK(delegate);
}

NestedAcceleratorController::~NestedAcceleratorController() {
}

void NestedAcceleratorController::RunWithDispatcher(
    base::MessagePumpDispatcher* nested_dispatcher) {
  base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
  base::MessageLoopForUI::ScopedNestableTaskAllower allow_nested(loop);

  scoped_ptr<NestedAcceleratorDispatcher> old_accelerator_dispatcher =
      accelerator_dispatcher_.Pass();
  accelerator_dispatcher_ = NestedAcceleratorDispatcher::Create(
      dispatcher_delegate_.get(), nested_dispatcher);

  // TODO(jbates) crbug.com/134753 Find quitters of this RunLoop and have them
  //              use run_loop.QuitClosure().
  scoped_ptr<base::RunLoop> run_loop = accelerator_dispatcher_->CreateRunLoop();
  base::AutoReset<base::Closure> reset_closure(&quit_closure_,
                                               run_loop->QuitClosure());
  run_loop->Run();
  accelerator_dispatcher_ = old_accelerator_dispatcher.Pass();
}

void NestedAcceleratorController::QuitNestedMessageLoop() {
  CHECK(!quit_closure_.is_null());
  quit_closure_.Run();
  accelerator_dispatcher_.reset();
}

}  // namespace wm
