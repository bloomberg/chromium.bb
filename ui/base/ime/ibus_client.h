// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_IBUS_CLIENT_H_
#define UI_BASE_IME_IBUS_CLIENT_H_
#pragma once

#include "base/basictypes.h"
#include "ui/base/ui_export.h"

namespace gfx {
class Rect;
}  // namespace gfx
namespace ui {

namespace internal {

// A class for sending and receiveing an event to and from ibus-daemon.
class UI_EXPORT IBusClient {
 public:
  IBusClient();
  virtual ~IBusClient();

  // The type of IME which is currently selected. Implementations should return
  // the former when no IME is selected or the type of the current IME is
  // unknown.
  enum InputMethodType {
    INPUT_METHOD_NORMAL = 0,
    INPUT_METHOD_XKB_LAYOUT,
  };

  // Returns the current input method type.
  virtual InputMethodType GetInputMethodType();

  // Resets the cursor location asynchronously.
  virtual void SetCursorLocation(const gfx::Rect& cursor_location,
                                 const gfx::Rect& composition_head);

 private:
  DISALLOW_COPY_AND_ASSIGN(IBusClient);
};

}  // namespace internal
}  // namespace ui

#endif  // UI_BASE_IME_IBUS_CLIENT_H_
