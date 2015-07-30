// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_KEYBOARD_SWITCHES_H_
#define UI_KEYBOARD_KEYBOARD_SWITCHES_H_

#include "ui/keyboard/keyboard_export.h"

namespace keyboard {
namespace switches {

// Disables IME extension APIs from overriding the URL for specifying the
// contents of the virtual keyboard container.
KEYBOARD_EXPORT extern const char kDisableInputView[];

// Disables voice input.
KEYBOARD_EXPORT extern const char kDisableVoiceInput[];

// Enables an IME extension API to set a URL for specifying the contents
// of the virtual keyboard container.
KEYBOARD_EXPORT extern const char kEnableInputView[];

// Enables experimental features for IME extensions.
KEYBOARD_EXPORT extern const char kEnableExperimentalInputViewFeatures[];

// Gesture typing flag for the virtual keyboard.
KEYBOARD_EXPORT extern const char kGestureTyping[];

// Enables gesture typing for the virtual keyboard.
KEYBOARD_EXPORT extern const char kGestureTypingEnabled[];

// Disables gesture typing for the virtual keyboard.
KEYBOARD_EXPORT extern const char kGestureTypingDisabled[];

// Controls the appearance of the settings option to enable gesture editing
// for the virtual keyboard.
KEYBOARD_EXPORT extern const char kGestureEditing[];

// Enables gesture editing for the virtual keyboard.
KEYBOARD_EXPORT extern const char kGestureEditingEnabled[];

// Disables gesture editing for the virtual keyboard.
KEYBOARD_EXPORT extern const char kGestureEditingDisabled[];

// Enables the virtual keyboard.
KEYBOARD_EXPORT extern const char kEnableVirtualKeyboard[];

// Floating virtual keyboard flag.
KEYBOARD_EXPORT extern const char kFloatingVirtualKeyboard[];

// Disable floating virtual keyboard.
KEYBOARD_EXPORT extern const char kFloatingVirtualKeyboardDisabled[];

// Enable floating virtual keyboard.
KEYBOARD_EXPORT extern const char kFloatingVirtualKeyboardEnabled[];

// Disabled overscrolling of web content when the virtual keyboard is displayed.
// If disabled, the work area is resized to restrict windows from overlapping
// with the keybaord area.
KEYBOARD_EXPORT extern const char kDisableVirtualKeyboardOverscroll[];

// Enable overscrolling of web content when the virtual keyboard is displayed
// to provide access to content that would otherwise be occluded.
KEYBOARD_EXPORT extern const char kEnableVirtualKeyboardOverscroll[];

// Controls automatic showing/hiding of the keyboard based on the devices
// plugged in.
KEYBOARD_EXPORT extern const char kSmartVirtualKeyboard[];

// Enables smart deploy for the virtual keyboard.
KEYBOARD_EXPORT extern const char kSmartVirtualKeyboardEnabled[];

// Disables smart deploy for the virtual keyboard.
KEYBOARD_EXPORT extern const char kSmartVirtualKeyboardDisabled[];

}  // namespace switches
}  // namespace keyboard

#endif  //  UI_KEYBOARD_KEYBOARD_SWITCHES_H_
