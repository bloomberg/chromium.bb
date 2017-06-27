// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/expand_arrow_view.h"

#include "base/metrics/histogram_macros.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/vector_icons/vector_icons.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"
#include "ui/views/controls/image_view.h"

namespace app_list {

namespace {

constexpr int kExpandArrowTileSize = 36;
constexpr int kExpandArrowIconSize = 12;
constexpr int kInkDropRadius = 18;

constexpr SkColor kExpandArrowColor = SK_ColorWHITE;
constexpr SkColor kInkDropRippleColor =
    SkColorSetARGBMacro(0x14, 0xFF, 0xFF, 0xFF);
constexpr SkColor kInkDropHighlightColor =
    SkColorSetARGBMacro(0xF, 0xFF, 0xFF, 0xFF);

}  // namespace

ExpandArrowView::ExpandArrowView(ContentsView* contents_view,
                                 AppListView* app_list_view)
    : views::CustomButton(this),
      contents_view_(contents_view),
      app_list_view_(app_list_view) {
  icon_ = new views::ImageView;
  icon_->SetVerticalAlignment(views::ImageView::CENTER);
  icon_->SetImage(gfx::CreateVectorIcon(kIcArrowUpIcon, kExpandArrowIconSize,
                                        kExpandArrowColor));
  AddChildView(icon_);

  SetInkDropMode(InkDropHostView::InkDropMode::ON);
}

ExpandArrowView::~ExpandArrowView() {}

void ExpandArrowView::ButtonPressed(views::Button* sender,
                                    const ui::Event& event) {
  UMA_HISTOGRAM_ENUMERATION(kPageOpenedHistogram, AppListModel::STATE_APPS,
                            AppListModel::STATE_LAST);

  contents_view_->SetActiveState(AppListModel::STATE_APPS);
  app_list_view_->SetState(AppListView::FULLSCREEN_ALL_APPS);
  GetInkDrop()->AnimateToState(views::InkDropState::ACTION_TRIGGERED);
}

gfx::Size ExpandArrowView::CalculatePreferredSize() const {
  return gfx::Size(kExpandArrowTileSize, kExpandArrowTileSize);
}

void ExpandArrowView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  gfx::Point center = rect.CenterPoint();
  rect.SetRect(center.x() - kExpandArrowIconSize / 2,
               center.y() - kExpandArrowIconSize / 2, kExpandArrowIconSize,
               kExpandArrowIconSize);
  icon_->SetBoundsRect(rect);
}

std::unique_ptr<views::InkDrop> ExpandArrowView::CreateInkDrop() {
  std::unique_ptr<views::InkDropImpl> ink_drop =
      CustomButton::CreateDefaultInkDropImpl();
  ink_drop->SetShowHighlightOnHover(false);
  ink_drop->SetShowHighlightOnFocus(true);
  ink_drop->SetAutoHighlightMode(
      views::InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE);
  return std::move(ink_drop);
}

std::unique_ptr<views::InkDropMask> ExpandArrowView::CreateInkDropMask() const {
  return base::MakeUnique<views::CircleInkDropMask>(
      size(), GetLocalBounds().CenterPoint(), kInkDropRadius);
}

std::unique_ptr<views::InkDropRipple> ExpandArrowView::CreateInkDropRipple()
    const {
  gfx::Point center = GetLocalBounds().CenterPoint();
  gfx::Rect bounds(center.x() - kInkDropRadius, center.y() - kInkDropRadius,
                   2 * kInkDropRadius, 2 * kInkDropRadius);
  return base::MakeUnique<views::FloodFillInkDropRipple>(
      size(), GetLocalBounds().InsetsFrom(bounds),
      GetInkDropCenterBasedOnLastEvent(), kInkDropRippleColor, 1.0f);
}

std::unique_ptr<views::InkDropHighlight>
ExpandArrowView::CreateInkDropHighlight() const {
  return base::MakeUnique<views::InkDropHighlight>(
      gfx::PointF(GetLocalBounds().CenterPoint()),
      base::MakeUnique<views::CircleLayerDelegate>(kInkDropHighlightColor,
                                                   kInkDropRadius));
}

}  // namespace app_list
