// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INPUT_METHOD_DELEGATE_H_
#define UI_BASE_IME_INPUT_METHOD_DELEGATE_H_

#include "base/event_types.h"
#include "ui/base/events/event_constants.h"
#include "ui/base/ui_export.h"

namespace ui {
namespace internal {

// An interface implemented by the object that handles events sent back from an
// ui::InputMethod implementation.
class UI_EXPORT InputMethodDelegate {
 public:
  virtual ~InputMethodDelegate() {}

  // Dispatch a key event already processed by the input method.
  virtual void DispatchKeyEventPostIME(
      const base::NativeEvent& native_key_event) = 0;
  virtual void DispatchFabricatedKeyEventPostIME(ui::EventType type,
                                                 ui::KeyboardCode key_code,
                                                 int flags) = 0;
};

}  // namespace internal
}  // namespace ui

#endif  // UI_BASE_IME_INPUT_METHOD_DELEGATE_H_
