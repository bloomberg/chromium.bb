// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_UI_CONTROLS_H_
#define UI_BASE_TEST_UI_CONTROLS_H_

#include "base/callback_forward.h"
#include "build/build_config.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/native_widget_types.h"

namespace ui_controls {

// A set of utility functions to generate native events in platform
// independent way. Note that since the implementations depend on a window being
// top level, these can only be called from test suites that are not sharded.
// For aura tests, please look into |aura::test:EventGenerator| first. This
// class provides a way to emulate events in synchronous way and it is often
// easier to write tests with this class than using |ui_controls|.
//
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

// Per the above comment, these methods can only be called from non-sharded test
// suites. This method ensures that they're not accidently called by sharded
// tests.
void EnableUIControls();

#if defined(OS_MACOSX)
bool IsUIControlsEnabled();
#endif

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

// Simulate a mouse move.
bool SendMouseMove(long screen_x, long screen_y);

// Returns false on Windows if the desired position is not over a window
// belonging to the current process.
bool SendMouseMoveNotifyWhenDone(long screen_x,
                                 long screen_y,
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

enum TouchType { PRESS = 1 << 0, RELEASE = 1 << 1, MOVE = 1 << 2 };

// Sends a mouse down and/or up message. The click will be sent to wherever
// the cursor currently is, so be sure to move the cursor before calling this
// (and be sure the cursor has arrived!).
bool SendMouseEvents(MouseButton type, int state);
bool SendMouseEventsNotifyWhenDone(MouseButton type,
                                   int state,
                                   const base::Closure& task);

// Same as SendMouseEvents with UP | DOWN.
bool SendMouseClick(MouseButton type);

#if defined(OS_WIN)
// Send WM_POINTER messages to generate touch events. There is no way to detect
// when events are received by chrome, it's up to users of this API to detect
// when events arrive. |action| is a bitmask of the TouchType constants that
// indicate what events are generated, |num| is the number of the touch
// pointers, |screen_x| and |screen_y| are the screen coordinates of a touch
// pointer.
bool SendTouchEvents(int action, int num, int screen_x, int screen_y);
#endif

#if defined(TOOLKIT_VIEWS)
// Runs |closure| after processing all pending ui events.
void RunClosureAfterAllPendingUIEvents(const base::Closure& closure);
#endif

#if defined(USE_AURA)
class UIControlsAura;
void InstallUIControlsAura(UIControlsAura* instance);
#endif

#if defined(OS_MACOSX)
// Returns true when tests need to use extra Tab and Shift-Tab key events
// to traverse to the desired item; because the application is configured to
// traverse more elements for accessibility reasons.
bool IsFullKeyboardAccessEnabled();
#endif

}  // namespace ui_controls

#endif  // UI_BASE_TEST_UI_CONTROLS_H_
