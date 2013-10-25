// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/ui_controls.h"

#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "ui/base/test/ui_controls_internal_win.h"
#include "ui/gfx/point.h"

namespace ui_controls {
bool g_ui_controls_enabled = false;

void EnableUIControls() {
  g_ui_controls_enabled = true;
}

bool SendKeyPress(gfx::NativeWindow window,
                  ui::KeyboardCode key,
                  bool control,
                  bool shift,
                  bool alt,
                  bool command) {
  CHECK(g_ui_controls_enabled);
  DCHECK(!command);  // No command key on Windows
  return internal::SendKeyPressImpl(window, key, control, shift, alt,
                                    base::Closure());
}

bool SendKeyPressNotifyWhenDone(gfx::NativeWindow window,
                                ui::KeyboardCode key,
                                bool control,
                                bool shift,
                                bool alt,
                                bool command,
                                const base::Closure& task) {
  CHECK(g_ui_controls_enabled);
  DCHECK(!command);  // No command key on Windows
  return internal::SendKeyPressImpl(window, key, control, shift, alt, task);
}

bool SendMouseMove(long x, long y) {
  CHECK(g_ui_controls_enabled);
  return internal::SendMouseMoveImpl(x, y, base::Closure());
}

bool SendMouseMoveNotifyWhenDone(long x, long y, const base::Closure& task) {
  CHECK(g_ui_controls_enabled);
  return internal::SendMouseMoveImpl(x, y, task);
}

bool SendMouseEvents(MouseButton type, int state) {
  CHECK(g_ui_controls_enabled);
  return internal::SendMouseEventsImpl(type, state, base::Closure());
}

bool SendMouseEventsNotifyWhenDone(MouseButton type, int state,
                                   const base::Closure& task) {
  CHECK(g_ui_controls_enabled);
  return internal::SendMouseEventsImpl(type, state, task);
}

bool SendMouseClick(MouseButton type) {
  CHECK(g_ui_controls_enabled);
  return internal::SendMouseEventsImpl(type, UP | DOWN, base::Closure());
}

void RunClosureAfterAllPendingUIEvents(const base::Closure& closure) {
  // On windows, posting UI events is synchronous so just post the closure.
  base::MessageLoopForUI::current()->PostTask(FROM_HERE, closure);
}

}  // namespace ui_controls
