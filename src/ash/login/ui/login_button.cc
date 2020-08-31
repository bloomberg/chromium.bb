// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_button.h"

#include "ash/login/ui/views_utils.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"

namespace ash {

namespace {

// Color of the ink drop ripple.
constexpr SkColor kInkDropRippleColor = SkColorSetARGB(0x0F, 0xFF, 0xFF, 0xFF);

// Color of the ink drop highlight.
constexpr SkColor kInkDropHighlightColor =
    SkColorSetARGB(0x14, 0xFF, 0xFF, 0xFF);

}  // namespace

LoginButton::LoginButton(views::ButtonListener* listener)
    : views::ImageButton(listener) {
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  SetInkDropMode(InkDropMode::ON);
  set_has_ink_drop_action_on_click(true);

  SetInstallFocusRingOnFocus(true);
  login_views_utils::ConfigureRectFocusRingCircleInkDrop(this, focus_ring(),
                                                         base::nullopt);
}

LoginButton::~LoginButton() = default;

std::unique_ptr<views::InkDropRipple> LoginButton::CreateInkDropRipple() const {
  gfx::Point center = GetLocalBounds().CenterPoint();
  const int radius = GetInkDropRadius();
  gfx::Rect bounds(center.x() - radius, center.y() - radius, radius * 2,
                   radius * 2);

  return std::make_unique<views::FloodFillInkDropRipple>(
      size(), GetLocalBounds().InsetsFrom(bounds),
      GetInkDropCenterBasedOnLastEvent(), kInkDropRippleColor,
      1.f /*visible_opacity*/);
}

std::unique_ptr<views::InkDropHighlight> LoginButton::CreateInkDropHighlight()
    const {
  return std::make_unique<views::InkDropHighlight>(gfx::SizeF(size()),
                                                   kInkDropHighlightColor);
}

int LoginButton::GetInkDropRadius() const {
  return std::min(GetLocalBounds().width(), GetLocalBounds().height()) / 2;
}

}  // namespace ash
