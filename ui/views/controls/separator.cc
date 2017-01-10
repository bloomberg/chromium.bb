// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/separator.h"

#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/canvas.h"
#include "ui/native_theme/native_theme.h"

namespace views {

// static
const char Separator::kViewClassName[] = "Separator";

// The separator size in pixels.
const int kSeparatorSize = 1;

Separator::Separator(Orientation orientation)
    : orientation_(orientation),
      color_overridden_(false),
      size_(kSeparatorSize) {
  SetColorFromNativeTheme();
}

Separator::~Separator() {
}

void Separator::SetColor(SkColor color) {
  color_ = color;
  color_overridden_ = true;
  SchedulePaint();
}

void Separator::SetPreferredSize(int size) {
  if (size != size_) {
    size_ = size;
    PreferredSizeChanged();
  }
}

void Separator::SetColorFromNativeTheme() {
  color_ = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_SeparatorColor);
}

////////////////////////////////////////////////////////////////////////////////
// Separator, View overrides:

gfx::Size Separator::GetPreferredSize() const {
  gfx::Size size =
      orientation_ == HORIZONTAL ? gfx::Size(1, size_) : gfx::Size(size_, 1);
  gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

void Separator::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ui::AX_ROLE_SPLITTER;
}

void Separator::OnPaint(gfx::Canvas* canvas) {
  canvas->FillRect(GetContentsBounds(), color_);
}

void Separator::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  if (!color_overridden_)
    SetColorFromNativeTheme();
}

const char* Separator::GetClassName() const {
  return kViewClassName;
}

}  // namespace views
