// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_control_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/widget/widget.h"

namespace ash {

ShelfControlButton::ShelfControlButton() : views::ImageButton(nullptr) {
  SetInkDropMode(InkDropMode::ON_NO_GESTURE_HANDLER);
  set_has_ink_drop_action_on_click(true);
  set_ink_drop_base_color(kShelfInkDropBaseColor);
  set_ink_drop_visible_opacity(kShelfInkDropVisibleOpacity);

  SetSize(gfx::Size(kShelfControlSize, kShelfControlSize));
  SetFocusPainter(TrayPopupUtils::CreateFocusPainter());
}

ShelfControlButton::~ShelfControlButton() = default;

gfx::Point ShelfControlButton::GetCenterPoint() const {
  return gfx::Point(width() / 2.f, width() / 2.f);
}

std::unique_ptr<views::InkDropRipple> ShelfControlButton::CreateInkDropRipple()
    const {
  return std::make_unique<views::FloodFillInkDropRipple>(
      size(),
      gfx::Insets(ShelfConstants::button_size() / 2 -
                  ShelfConstants::control_border_radius()),
      GetInkDropCenterBasedOnLastEvent(), GetInkDropBaseColor(),
      ink_drop_visible_opacity());
}

std::unique_ptr<views::InkDrop> ShelfControlButton::CreateInkDrop() {
  std::unique_ptr<views::InkDropImpl> ink_drop =
      Button::CreateDefaultInkDropImpl();
  ink_drop->SetShowHighlightOnHover(false);
  return std::move(ink_drop);
}

std::unique_ptr<views::InkDropMask> ShelfControlButton::CreateInkDropMask()
    const {
  return std::make_unique<views::CircleInkDropMask>(
      size(), GetCenterPoint(), ShelfConstants::control_border_radius());
}

void ShelfControlButton::PaintBackground(gfx::Canvas* canvas,
                                         const gfx::Rect& bounds) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(kShelfControlPermanentHighlightBackground);
  canvas->DrawRoundRect(bounds, ShelfConstants::control_border_radius(), flags);
}

}  // namespace ash
