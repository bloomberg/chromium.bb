// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/suggestion_chip_view.h"

#include "ui/gfx/canvas.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace app_list {

namespace {

// Appearance.
constexpr SkColor kBackgroundColor = SkColorSetA(SK_ColorBLACK, 0x1F);
constexpr int kCornerRadiusDip = 12;
constexpr int kPaddingHorizontalDip = 8;
constexpr int kPaddingVerticalDip = 4;

// Typography.
constexpr SkColor kTextColor = SkColorSetA(SK_ColorBLACK, 0xDE);

}  // namespace

SuggestionChipView::SuggestionChipView(const base::string16& text)
    : views::View(), text_view_(new views::Label(text)) {
  InitLayout();
}

void SuggestionChipView::InitLayout() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(kPaddingVerticalDip, kPaddingHorizontalDip)));

  // TODO(dmblack): Add optional icon.

  // Text view.
  text_view_->SetAutoColorReadabilityEnabled(false);
  text_view_->SetEnabledColor(kTextColor);
  text_view_->SetFontList(text_view_->font_list().DeriveWithSizeDelta(2));
  AddChildView(text_view_);
}

void SuggestionChipView::OnPaintBackground(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(kBackgroundColor);
  canvas->DrawRoundRect(GetContentsBounds(), kCornerRadiusDip, flags);
}

}  // namespace app_list
