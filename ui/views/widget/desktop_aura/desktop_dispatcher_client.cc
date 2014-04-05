// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_dispatcher_client.h"

#include "base/auto_reset.h"
#include "base/run_loop.h"

namespace views {

DesktopDispatcherClient::DesktopDispatcherClient()
    : weak_ptr_factory_(this) {}

DesktopDispatcherClient::~DesktopDispatcherClient() {
}

void DesktopDispatcherClient::RunWithDispatcher(
    base::MessagePumpDispatcher* nested_dispatcher) {
  // TODO(erg): This class has been copypastad from
  // ash/accelerators/nested_dispatcher_controller.cc. I have left my changes
  // commented out because I don't entirely understand the implications of the
  // change.
  base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
  base::MessageLoopForUI::ScopedNestableTaskAllower allow_nested(loop);

  base::Closure old_quit_closure = quit_closure_;
#if defined(OS_WIN)
  base::RunLoop run_loop(nested_dispatcher);
#else
  base::RunLoop run_loop;
#endif

  quit_closure_ = run_loop.QuitClosure();
  base::WeakPtr<DesktopDispatcherClient> alive(weak_ptr_factory_.GetWeakPtr());
  run_loop.Run();
  if (alive) {
    weak_ptr_factory_.InvalidateWeakPtrs();
    quit_closure_ = old_quit_closure;
  }
}

void DesktopDispatcherClient::QuitNestedMessageLoop() {
  CHECK(!quit_closure_.is_null());
  quit_closure_.Run();
}

}  // namespace views
