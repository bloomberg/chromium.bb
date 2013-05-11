// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/separator.h"

#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/gfx/canvas.h"

namespace views {

// static
const char Separator::kViewClassName[] = "Separator";

// The separator height in pixels.
const int kSeparatorHeight = 1;

// Default color of the separator.
const SkColor kDefaultColor = SkColorSetARGB(255, 233, 233, 233);

Separator::Separator(Orientation orientation) : orientation_(orientation) {
  set_focusable(false);
}

Separator::~Separator() {
}

////////////////////////////////////////////////////////////////////////////////
// Separator, View overrides:

gfx::Size Separator::GetPreferredSize() {
  if (orientation_ == HORIZONTAL)
    return gfx::Size(width(), kSeparatorHeight);
  return gfx::Size(kSeparatorHeight, height());
}

void Separator::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_SEPARATOR;
}

void Separator::Paint(gfx::Canvas* canvas) {
  canvas->FillRect(bounds(), kDefaultColor);
}

const char* Separator::GetClassName() const {
  return kViewClassName;
}

}  // namespace views
