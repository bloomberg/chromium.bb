// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/events/event_utils_win.h"

#include "ui/base/events.h"
#include "views/events/event.h"

namespace views {

int GetRepeatCount(const KeyEvent& event) {
  return LOWORD(event.native_event().lParam);
}

bool IsExtendedKey(const KeyEvent& event) {
  return (HIWORD(event.native_event().lParam) & KF_EXTENDED) == KF_EXTENDED;
}

int GetWindowsFlags(const Event& event) {
  // TODO(beng): need support for x1/x2.
  int result = 0;
  result |= (event.flags() & ui::EF_SHIFT_DOWN) ? MK_SHIFT : 0;
  result |= (event.flags() & ui::EF_CONTROL_DOWN) ? MK_CONTROL : 0;
  result |= (event.flags() & ui::EF_LEFT_BUTTON_DOWN) ? MK_LBUTTON : 0;
  result |= (event.flags() & ui::EF_MIDDLE_BUTTON_DOWN) ? MK_MBUTTON : 0;
  result |= (event.flags() & ui::EF_RIGHT_BUTTON_DOWN) ? MK_RBUTTON : 0;
  return result;
}

}  // namespace views
