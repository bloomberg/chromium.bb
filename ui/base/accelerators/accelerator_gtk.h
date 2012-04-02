// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_ACCELERATOR_GTK_H_
#define UI_BASE_ACCELERATORS_ACCELERATOR_GTK_H_
#pragma once

#include <gdk/gdk.h>

#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ui_export.h"

namespace ui {

class UI_EXPORT AcceleratorGtk : public Accelerator {
 public:
  AcceleratorGtk();

  AcceleratorGtk(guint key_code, GdkModifierType modifier_type);

  AcceleratorGtk(KeyboardCode key_code,
                 bool shift_pressed,
                 bool ctrl_pressed,
                 bool alt_pressed);

  virtual ~AcceleratorGtk();

  guint GetGdkKeyCode() const;

  GdkModifierType gdk_modifier_type() const {
    return static_cast<GdkModifierType>(modifiers_);
  }

 private:
  guint gdk_key_code_;

  // Copy and assign is allowed.
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_ACCELERATOR_GTK_H_
