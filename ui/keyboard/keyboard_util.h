// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_KEYBOARD_UTIL_H_
#define UI_KEYBOARD_KEYBOARD_UTIL_H_

#include <string>

#include "base/strings/string16.h"
// TODO(beng): replace with forward decl once RootWindow is renamed.
#include "ui/aura/window.h"
#include "ui/keyboard/keyboard_export.h"

struct GritResourceMap;

namespace keyboard {

// Enumeration of swipe directions.
enum CursorMoveDirection {
  kCursorMoveRight = 0x01,
  kCursorMoveLeft = 0x02,
  kCursorMoveUp = 0x04,
  kCursorMoveDown = 0x08
};

// An enumeration of different keyboard control events that should be logged.
enum KeyboardControlEvent {
  KEYBOARD_CONTROL_SHOW = 0,
  KEYBOARD_CONTROL_HIDE_AUTO,
  KEYBOARD_CONTROL_HIDE_USER,
  KEYBOARD_CONTROL_MAX,
};

// Returns true if the virtual keyboard is enabled.
KEYBOARD_EXPORT bool IsKeyboardEnabled();

// Returns true if the keyboard usability test is enabled.
KEYBOARD_EXPORT bool IsKeyboardUsabilityExperimentEnabled();

// Insert |text| into the active TextInputClient associated with |root_window|,
// if there is one. Returns true if |text| was successfully inserted. Note
// that this may convert |text| into ui::KeyEvents for injection in some
// special circumstances (i.e. VKEY_RETURN, VKEY_BACK).
KEYBOARD_EXPORT bool InsertText(const base::string16& text,
                                aura::Window* root_window);

// Move cursor when swipe on the virtualkeyboard. Returns true if cursor was
// successfully moved according to |swipe_direction|.
KEYBOARD_EXPORT bool MoveCursor(int swipe_direction,
                                int modifier_flags,
                                aura::WindowEventDispatcher* dispatcher);

// Sends a fabricated key event, where |type| is the event type, |key_value|
// is the unicode value of the character, |key_code| is the legacy key code
// value, |key_name| is the name of the key as defined in the DOM3 key event
// specification, and |modifier| indicates if any modifier keys are being
// virtually pressed. The event is dispatched to the active TextInputClient
// associated with |root_window|. The type may be "keydown" or "keyup".
KEYBOARD_EXPORT bool SendKeyEvent(std::string type,
                                   int key_value,
                                   int key_code,
                                   std::string key_name,
                                   int modifiers,
                                   aura::WindowEventDispatcher* dispatcher);

// Marks that the keyboard load has started. This is used to measure the time it
// takes to fully load the keyboard. This should be called before
// MarkKeyboardLoadFinished.
KEYBOARD_EXPORT const void MarkKeyboardLoadStarted();

// Marks that the keyboard load has ended. This finishes measuring that the
// keyboard is loaded.
KEYBOARD_EXPORT const void MarkKeyboardLoadFinished();

// Get the list of keyboard resources. |size| is populated with the number of
// resources in the returned array.
KEYBOARD_EXPORT const GritResourceMap* GetKeyboardExtensionResources(
    size_t* size);

// Logs the keyboard control event as a UMA stat.
void LogKeyboardControlEvent(KeyboardControlEvent event);

}  // namespace keyboard

#endif  // UI_KEYBOARD_KEYBOARD_UTIL_H_
