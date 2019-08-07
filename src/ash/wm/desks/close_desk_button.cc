// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/close_desk_button.h"

#include <utility>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/background.h"
#include "ui/views/style/platform_style.h"

namespace ash {

namespace {

constexpr float kInkDropOpacity = 0.2f;

constexpr float kInkDropHighlightOpacity = 0.1f;

constexpr int kCornerRadius = 12;

constexpr SkColor kBackgroundColor = SkColorSetARGB(181, 55, 71, 79);

}  // namespace

CloseDeskButton::CloseDeskButton(views::ButtonListener* listener)
    : ImageButton(listener) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(kDesksCloseDeskButtonIcon, SK_ColorWHITE));
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  SetBackgroundImageAlignment(views::ImageButton::ALIGN_CENTER,
                              views::ImageButton::ALIGN_MIDDLE);
  SetBackground(
      CreateBackgroundFromPainter(views::Painter::CreateSolidRoundRectPainter(
          kBackgroundColor, kCornerRadius)));

  SetInkDropMode(InkDropMode::ON);
  set_has_ink_drop_action_on_click(true);
  set_ink_drop_visible_opacity(kInkDropOpacity);
  SetFocusPainter(nullptr);
}

CloseDeskButton::~CloseDeskButton() = default;

const char* CloseDeskButton::GetClassName() const {
  return "CloseDeskButton";
}

std::unique_ptr<views::InkDrop> CloseDeskButton::CreateInkDrop() {
  auto ink_drop = CreateDefaultFloodFillInkDropImpl();
  ink_drop->SetShowHighlightOnHover(true);
  ink_drop->SetShowHighlightOnFocus(!views::PlatformStyle::kPreferFocusRings);
  return std::move(ink_drop);
}

std::unique_ptr<views::InkDropRipple> CloseDeskButton::CreateInkDropRipple()
    const {
  return std::make_unique<views::FloodFillInkDropRipple>(
      size(), GetInkDropCenterBasedOnLastEvent(), GetInkDropBaseColor(),
      ink_drop_visible_opacity());
}

std::unique_ptr<views::InkDropHighlight>
CloseDeskButton::CreateInkDropHighlight() const {
  auto highlight = ImageButton::CreateInkDropHighlight();
  highlight->set_visible_opacity(kInkDropHighlightOpacity);
  return highlight;
}

SkColor CloseDeskButton::GetInkDropBaseColor() const {
  return SK_ColorWHITE;
}

std::unique_ptr<views::InkDropMask> CloseDeskButton::CreateInkDropMask() const {
  return std::make_unique<views::RoundRectInkDropMask>(size(), gfx::Insets(),
                                                       kCornerRadius);
}

}  // namespace ash
