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

SuggestionChipView::SuggestionChipView(const base::string16& text,
                                       SuggestionChipListener* listener)
    : text_view_(new views::Label(text)), listener_(listener) {
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

void SuggestionChipView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    if (listener_)
      listener_->OnSuggestionChipPressed(this);
    event->SetHandled();
  }
}

bool SuggestionChipView::OnMousePressed(const ui::MouseEvent& event) {
  if (listener_)
    listener_->OnSuggestionChipPressed(this);
  return true;
}

const base::string16& SuggestionChipView::GetText() const {
  return text_view_->text();
}

}  // namespace app_list
