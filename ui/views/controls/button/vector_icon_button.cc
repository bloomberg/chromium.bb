// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/vector_icon_button.h"

#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/vector_icon_button_delegate.h"
#include "ui/views/painter.h"

namespace views {

namespace {

// Extra space around the buttons to increase their event target size.
const int kButtonExtraTouchSize = 4;

}  // namespace

VectorIconButton::VectorIconButton(VectorIconButtonDelegate* delegate)
    : ImageButton(delegate),
      delegate_(delegate),
      id_(gfx::VectorIconId::VECTOR_ICON_NONE) {
  SetInkDropMode(InkDropMode::ON);
  set_has_ink_drop_action_on_click(true);
  SetImageAlignment(ImageButton::ALIGN_CENTER, ImageButton::ALIGN_MIDDLE);
  SetFocusPainter(nullptr);
}

VectorIconButton::~VectorIconButton() {}

void VectorIconButton::SetIcon(gfx::VectorIconId id) {
  id_ = id;
  icon_ = nullptr;

  OnSetIcon();
}

void VectorIconButton::SetIcon(const gfx::VectorIcon& icon) {
  id_ = gfx::VectorIconId::VECTOR_ICON_NONE;
  icon_ = &icon;

  OnSetIcon();
}

void VectorIconButton::OnThemeChanged() {
  UpdateImagesAndColors();
}

void VectorIconButton::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  UpdateImagesAndColors();
}

void VectorIconButton::OnSetIcon() {
  if (!border())
    SetBorder(CreateEmptyBorder(gfx::Insets(kButtonExtraTouchSize)));

  UpdateImagesAndColors();
}

void VectorIconButton::UpdateImagesAndColors() {
  SkColor icon_color =
      color_utils::DeriveDefaultIconColor(delegate_->GetVectorIconBaseColor());
  SkColor disabled_color = SkColorSetA(icon_color, 0xff / 2);
  if (icon_) {
    SetImage(CustomButton::STATE_NORMAL,
             gfx::CreateVectorIcon(*icon_, icon_color));
    SetImage(CustomButton::STATE_DISABLED,
             gfx::CreateVectorIcon(*icon_, disabled_color));
  } else {
    SetImage(CustomButton::STATE_NORMAL,
             gfx::CreateVectorIcon(id_, icon_color));
    SetImage(CustomButton::STATE_DISABLED,
             gfx::CreateVectorIcon(id_, disabled_color));
  }
  set_ink_drop_base_color(icon_color);
}

}  // namespace views
