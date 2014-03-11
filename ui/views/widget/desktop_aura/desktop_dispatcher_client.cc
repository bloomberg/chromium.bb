// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_dispatcher_client.h"

#include "base/auto_reset.h"
#include "base/run_loop.h"

namespace views {

DesktopDispatcherClient::DesktopDispatcherClient() {}

DesktopDispatcherClient::~DesktopDispatcherClient() {}

void DesktopDispatcherClient::RunWithDispatcher(
    base::MessagePumpDispatcher* nested_dispatcher,
    aura::Window* associated_window) {
  // TODO(erg): This class has been copypastad from
  // ash/accelerators/nested_dispatcher_controller.cc. I have left my changes
  // commented out because I don't entirely understand the implications of the
  // change.
  base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
  base::MessageLoopForUI::ScopedNestableTaskAllower allow_nested(loop);

  base::RunLoop run_loop(nested_dispatcher);
  base::AutoReset<base::Closure> reset_closure(&quit_closure_,
                                               run_loop.QuitClosure());
  run_loop.Run();
}

void DesktopDispatcherClient::QuitNestedMessageLoop() {
  CHECK(!quit_closure_.is_null());
  quit_closure_.Run();
}

}  // namespace views
