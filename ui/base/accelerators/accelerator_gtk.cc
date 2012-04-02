// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/accelerator_gtk.h"

#include "ui/base/keycodes/keyboard_code_conversion_gtk.h"
#include "ui/base/keycodes/keyboard_codes_posix.h"

namespace ui {

AcceleratorGtk::AcceleratorGtk() : gdk_key_code_(0) {
}

AcceleratorGtk::AcceleratorGtk(guint key_code, GdkModifierType modifier_type)
    : Accelerator(WindowsKeyCodeForGdkKeyCode(key_code), modifier_type),
      gdk_key_code_(key_code) {
}

AcceleratorGtk::AcceleratorGtk(KeyboardCode key_code,
                               bool shift_pressed,
                               bool ctrl_pressed,
                               bool alt_pressed)
    : Accelerator(key_code, 0),
      gdk_key_code_(0) {
  if (shift_pressed)
    modifiers_ |= GDK_SHIFT_MASK;
  if (ctrl_pressed)
    modifiers_ |= GDK_CONTROL_MASK;
  if (alt_pressed)
    modifiers_ |= GDK_MOD1_MASK;
}

AcceleratorGtk::~AcceleratorGtk() {
}

guint AcceleratorGtk::GetGdkKeyCode() const {
  if (gdk_key_code_ > 0)
    return gdk_key_code_;

  // The second parameter is false because accelerator keys are expressed in
  // terms of the non-shift-modified key.
  return GdkKeyCodeForWindowsKeyCode(key_code_, false);
}

}  // namespace ui
