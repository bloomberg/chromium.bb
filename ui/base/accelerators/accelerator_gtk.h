// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_ACCELERATOR_GTK_H_
#define UI_BASE_ACCELERATORS_ACCELERATOR_GTK_H_
#pragma once

#include <gdk/gdk.h>

#include "ui/base/accelerators/accelerator.h"
#include "ui/base/keycodes/keyboard_code_conversion_gtk.h"
#include "ui/base/keycodes/keyboard_codes_posix.h"

namespace ui {

class AcceleratorGtk : public Accelerator {
 public:
  AcceleratorGtk(ui::KeyboardCode key_code,
                 bool shift_pressed, bool ctrl_pressed, bool alt_pressed)
      : gdk_keyval_(0) {
    key_code_ = key_code;
    modifiers_ = 0;
    if (shift_pressed)
      modifiers_ |= GDK_SHIFT_MASK;
    if (ctrl_pressed)
      modifiers_ |= GDK_CONTROL_MASK;
    if (alt_pressed)
      modifiers_ |= GDK_MOD1_MASK;
  }

  AcceleratorGtk(guint keyval, GdkModifierType modifier_type) {
    key_code_ = ui::WindowsKeyCodeForGdkKeyCode(keyval);
    gdk_keyval_ = keyval;
    modifiers_ = modifier_type;
  }

  AcceleratorGtk() : gdk_keyval_(0) { }
  virtual ~AcceleratorGtk() { }

  guint GetGdkKeyCode() const {
    return gdk_keyval_ > 0 ?
           // The second parameter is false because accelerator keys are
           // expressed in terms of the non-shift-modified key.
           gdk_keyval_ : ui::GdkKeyCodeForWindowsKeyCode(key_code_, false);
  }

  GdkModifierType gdk_modifier_type() const {
    return static_cast<GdkModifierType>(modifiers());
  }

 private:
  // The GDK keycode.
  guint gdk_keyval_;
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_ACCELERATOR_GTK_H_
