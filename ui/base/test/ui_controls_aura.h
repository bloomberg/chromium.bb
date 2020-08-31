// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_TEST_BASE_UI_CONTROLS_AURA_H_
#define UI_TEST_BASE_UI_CONTROLS_AURA_H_

#include "base/callback_forward.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/native_widget_types.h"

namespace ui_controls {

// An interface to provide Aura implementation of UI control.
class UIControlsAura {
 public:
  UIControlsAura();
  virtual ~UIControlsAura();

  virtual bool SendKeyPress(gfx::NativeWindow window,
                            ui::KeyboardCode key,
                            bool control,
                            bool shift,
                            bool alt,
                            bool command) = 0;
  virtual bool SendKeyPressNotifyWhenDone(gfx::NativeWindow window,
                                          ui::KeyboardCode key,
                                          bool control,
                                          bool shift,
                                          bool alt,
                                          bool command,
                                          base::OnceClosure task) = 0;

  // Simulate a mouse move. (x,y) are absolute screen coordinates.
  virtual bool SendMouseMove(int x, int y) = 0;
  virtual bool SendMouseMoveNotifyWhenDone(int x,
                                           int y,
                                           base::OnceClosure task) = 0;

  // Sends a mouse down and/or up message. The click will be sent to wherever
  // the cursor currently is, so be sure to move the cursor before calling this
  // (and be sure the cursor has arrived!).
  virtual bool SendMouseEvents(MouseButton type,
                               int button_state,
                               int accelerator_state) = 0;
  virtual bool SendMouseEventsNotifyWhenDone(MouseButton type,
                                             int button_state,
                                             base::OnceClosure task,
                                             int accelerator_state) = 0;
  // Same as SendMouseEvents with BUTTON_UP | BUTTON_DOWN.
  virtual bool SendMouseClick(MouseButton type) = 0;

#if defined(OS_WIN)
  virtual bool SendTouchEvents(int action, int num, int x, int y) = 0;
#elif defined(OS_CHROMEOS)
  virtual bool SendTouchEvents(int action, int id, int x, int y) = 0;
  virtual bool SendTouchEventsNotifyWhenDone(int action,
                                             int id,
                                             int x,
                                             int y,
                                             base::OnceClosure task) = 0;
#endif
};

}  // namespace ui_controls

#endif  // UI_TEST_BASE_UI_CONTROLS_AURA_H_
