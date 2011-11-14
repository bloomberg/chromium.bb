// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/focus/accelerator_handler.h"

#include "views/focus/focus_manager.h"

namespace views {

AcceleratorHandler::AcceleratorHandler() {}

base::MessagePumpDispatcher::DispatchStatus
    AcceleratorHandler::Dispatch(base::wayland::WaylandEvent* ev) {
  return base::MessagePumpDispatcher::EVENT_IGNORED;
}

}  // namespace views
