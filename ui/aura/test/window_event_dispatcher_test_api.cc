// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/window_event_dispatcher_test_api.h"

#include "ui/aura/window_event_dispatcher.h"

namespace aura {
namespace test {

WindowEventDispatcherTestApi::WindowEventDispatcherTestApi(
    WindowEventDispatcher* dispatcher) : dispatcher_(dispatcher) {
}

bool WindowEventDispatcherTestApi::HoldingPointerMoves() const {
  return dispatcher_->move_hold_count_ > 0;
}

}  // namespace test
}  // namespace aura

