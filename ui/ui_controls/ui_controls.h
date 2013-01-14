// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_UI_CONTROLS_UI_CONTROLS_H_
#define UI_UI_CONTROLS_UI_CONTROLS_H_

#include "base/callback_forward.h"
#include "build/build_config.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/gfx/native_widget_types.h"

namespace ui_controls {

// Many of the functions in this class include a variant that takes a Closure.
// The version that takes a Closure waits until the generated event is
// processed. Once the generated event is processed the Closure is Run (and
// deleted). Note that this is a somewhat fragile process in that any event of
// the correct type (key down, mouse click, etc.) will trigger the Closure to be
// run. Hence a usage such as
//
//   SendKeyPress(...);
//   SendKeyPressNotifyWhenDone(..., task);
//
// might trigger |task| early.
//
// Note: Windows does not currently do anything with the |window| argument for
// these functions, so passing NULL is ok.

// Send a key press with/without modifier keys.
//
// If you're writing a test chances are you want the variant in ui_test_utils.
// See it for details.
bool SendKeyPress(gfx::NativeWindow window,
                  ui::KeyboardCode key,
                  bool control,
                  bool shift,
                  bool alt,
                  bool command);
bool SendKeyPressNotifyWhenDone(gfx::NativeWindow window,
                                ui::KeyboardCode key,
                                bool control,
                                bool shift,
                                bool alt,
                                bool command,
                                const base::Closure& task);

// Simulate a mouse move. (x,y) are absolute screen coordinates.
bool SendMouseMove(long x, long y);
bool SendMouseMoveNotifyWhenDone(long x,
                                 long y,
                                 const base::Closure& task);

enum MouseButton {
  LEFT = 0,
  MIDDLE,
  RIGHT,
};

// Used to indicate the state of the button when generating events.
enum MouseButtonState {
  UP = 1,
  DOWN = 2
};

// Sends a mouse down and/or up message. The click will be sent to wherever
// the cursor currently is, so be sure to move the cursor before calling this
// (and be sure the cursor has arrived!).
bool SendMouseEvents(MouseButton type, int state);
bool SendMouseEventsNotifyWhenDone(MouseButton type,
                                   int state,
                                   const base::Closure& task);

// Same as SendMouseEvents with UP | DOWN.
bool SendMouseClick(MouseButton type);

#if defined(TOOLKIT_VIEWS)
// Runs |closure| after processing all pending ui events.
void RunClosureAfterAllPendingUIEvents(const base::Closure& closure);
#endif

#if defined(USE_AURA)
class UIControlsAura;
void InstallUIControlsAura(UIControlsAura* instance);
#endif

#if defined(USE_ASH)
ui_controls::UIControlsAura* CreateAshUIControls();
#endif

}  // namespace ui_controls

#endif  // UI_UI_CONTROLS_UI_CONTROLS_H_
