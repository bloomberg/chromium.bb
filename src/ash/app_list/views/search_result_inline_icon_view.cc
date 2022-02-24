// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_inline_icon_view.h"

#include <algorithm>
#include <memory>

#include "ash/app_list/app_list_util.h"
#include "ash/public/cpp/ash_typography.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/style/ash_color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"

namespace ash {

namespace {

constexpr int kBorderThickness = 1;
constexpr float kButtonCornerRadius = 6.0f;
constexpr int kLeftRightMargin = 6;
constexpr gfx::Insets kBubbleBorder(0, kLeftRightMargin, 0, kLeftRightMargin);
constexpr int kIconSize = 14;
constexpr int kLabelMinEdgeLength = 20;

}  // namespace

class SearchResultInlineIconView::SizedLabel : public views::Label {
 public:
  SizedLabel() = default;
  ~SizedLabel() override = default;

  // views::Label:
  gfx::Size GetMinimumSize() const override {
    return gfx::Size(kLabelMinEdgeLength, kLabelMinEdgeLength);
  }
};

SearchResultInlineIconView::SearchResultInlineIconView() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

SearchResultInlineIconView::~SearchResultInlineIconView() = default;

void SearchResultInlineIconView::SetIcon(const gfx::VectorIcon& icon) {
  DCHECK(!label_);
  if (!icon_image_) {
    icon_image_ = AddChildView(std::make_unique<views::ImageView>());
    icon_image_->SetCanProcessEventsWithinSubtree(false);
    icon_image_->SetVisible(true);
  }

  icon_ = &icon;
  icon_image_->SetImage(gfx::CreateVectorIcon(
      *icon_, AshColorProvider::Get()->GetContentLayerColor(
                  AshColorProvider::ContentLayerType::kTextColorURL)));
  icon_image_->SetImageSize(gfx::Size(kIconSize, kIconSize));
  icon_image_->SetVisible(true);

  icon_image_->SetBorder(views::CreateEmptyBorder(kBubbleBorder));
  SetVisible(true);
}

void SearchResultInlineIconView::SetText(const std::u16string& text) {
  DCHECK(!icon_image_);
  if (!label_) {
    label_ = AddChildView(std::make_unique<SizedLabel>());
    label_->SetBackgroundColor(SK_ColorTRANSPARENT);
    label_->SetVisible(true);
    label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    label_->SetTextContext(CONTEXT_SEARCH_RESULT_VIEW);
    label_->SetTextStyle(STYLE_PRODUCTIVITY_LAUNCHER);
  }

  label_->SetText(text);
  label_->SetVisible(true);
  label_->SetBorder(views::CreateEmptyBorder(kBubbleBorder));

  label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorURL));
  SetVisible(true);
}

void SearchResultInlineIconView::OnPaint(gfx::Canvas* canvas) {
  cc::PaintFlags paint_flags;
  paint_flags.setAntiAlias(true);
  paint_flags.setColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorURL));
  paint_flags.setStyle(cc::PaintFlags::kStroke_Style);
  paint_flags.setStrokeWidth(kBorderThickness);
  canvas->DrawRoundRect(GetContentsBounds(), kButtonCornerRadius, paint_flags);
}

void SearchResultInlineIconView::OnThemeChanged() {
  views::View::OnThemeChanged();
  if (icon_image_) {
    DCHECK(icon_);
    icon_image_->SetImage(gfx::CreateVectorIcon(
        *icon_, AshColorProvider::Get()->GetContentLayerColor(
                    AshColorProvider::ContentLayerType::kTextColorURL)));
  }
  if (label_) {
    label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorURL));
  }
}

}  // namespace ash
