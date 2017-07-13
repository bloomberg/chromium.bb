// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/padded_button.h"

#include "base/memory/ptr_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/message_center/message_center_style.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/painter.h"

namespace message_center {

PaddedButton::PaddedButton(views::ButtonListener* listener)
    : views::ImageButton(listener) {
  SetFocusForPlatform();
  SetFocusPainter(views::Painter::CreateSolidFocusPainter(
      kFocusBorderColor,
      gfx::Insets(1, 2, 2, 2)));
  SetBackground(views::CreateSolidBackground(kControlButtonBackgroundColor));
  SetBorder(views::CreateEmptyBorder(gfx::Insets(kControlButtonBorderSize)));
  set_animate_on_state_change(false);

  SetInkDropMode(InkDropMode::ON);
  set_ink_drop_base_color(SkColorSetA(SK_ColorBLACK, 0.6 * 0xff));
  set_has_ink_drop_action_on_click(true);
}

std::unique_ptr<views::InkDrop> PaddedButton::CreateInkDrop() {
  auto ink_drop = CreateDefaultInkDropImpl();
  ink_drop->SetShowHighlightOnHover(false);
  ink_drop->SetShowHighlightOnFocus(false);
  return std::move(ink_drop);
}

std::unique_ptr<views::InkDropRipple> PaddedButton::CreateInkDropRipple()
    const {
  return base::MakeUnique<views::FloodFillInkDropRipple>(
      size(), GetInkDropCenterBasedOnLastEvent(),
      SkColorSetA(SK_ColorBLACK, 0.6 * 255), ink_drop_visible_opacity());
}

}  // namespace message_center
