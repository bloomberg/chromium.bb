// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_delegate.h"

#include "ui/events/event.h"

namespace ui {
namespace internal {

// static
void InputMethodDelegate::RunDispatchKeyEventPostIMECallback(
    KeyEvent* key_event,
    DispatchKeyEventPostIMECallback callback) {
  if (!callback)
    return;
  std::move(callback).Run(key_event->handled(),
                          key_event->stopped_propagation());
}

}  // namespace internal
}  // namespace ui
