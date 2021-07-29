// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_screen_capture_view.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/rounded_image_view.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/holding_space/holding_space_util.h"
#include "ash/system/tray/tray_constants.h"
#include "base/bind.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout_view.h"

namespace ash {

// Appearance.
constexpr gfx::Insets kCheckmarkAndPrimaryActionContainerPadding(4);
constexpr gfx::Size kPlayIconSize(32, 32);
constexpr gfx::Size kPrimaryActionSize(24, 24);

HoldingSpaceItemScreenCaptureView::HoldingSpaceItemScreenCaptureView(
    HoldingSpaceViewDelegate* delegate,
    const HoldingSpaceItem* item)
    : HoldingSpaceItemView(delegate, item) {
  using CrossAxisAlignment = views::BoxLayout::CrossAxisAlignment;
  using MainAxisAlignment = views::BoxLayout::MainAxisAlignment;

  views::Builder<HoldingSpaceItemScreenCaptureView> builder(this);
  builder.SetPreferredSize(kHoldingSpaceScreenCaptureSize)
      .SetLayoutManager(std::make_unique<views::FillLayout>())
      .AddChild(views::Builder<RoundedImageView>()
                    .CopyAddressTo(&image_)
                    .SetID(kHoldingSpaceItemImageId)
                    .SetCornerRadius(kHoldingSpaceCornerRadius));

  if (item->type() == HoldingSpaceItem::Type::kScreenRecording) {
    builder.AddChild(
        views::Builder<views::BoxLayoutView>()
            .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
            .SetMainAxisAlignment(MainAxisAlignment::kCenter)
            .SetCrossAxisAlignment(CrossAxisAlignment::kCenter)
            .SetFocusBehavior(views::View::FocusBehavior::NEVER)
            .AddChild(views::Builder<views::ImageView>()
                          .CopyAddressTo(&play_icon_)
                          .SetID(kHoldingSpaceScreenCapturePlayIconId)
                          .SetPreferredSize(kPlayIconSize)
                          .SetImageSize(gfx::Size(kHoldingSpaceIconSize,
                                                  kHoldingSpaceIconSize))));
  }

  builder
      .AddChild(
          views::Builder<views::FlexLayoutView>()
              .SetOrientation(views::LayoutOrientation::kHorizontal)
              .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
              .SetInteriorMargin(kCheckmarkAndPrimaryActionContainerPadding)
              .AddChild(CreateCheckmarkBuilder())
              .AddChild(views::Builder<views::View>().SetProperty(
                  views::kFlexBehaviorKey,
                  views::FlexSpecification(
                      views::MinimumFlexSizeRule::kScaleToZero,
                      views::MaximumFlexSizeRule::kUnbounded)))
              .AddChild(CreatePrimaryActionBuilder(kPrimaryActionSize)))
      .BuildChildren();

  // Subscribe to be notified of changes to `item`'s image.
  image_subscription_ = item->image().AddImageSkiaChangedCallback(
      base::BindRepeating(&HoldingSpaceItemScreenCaptureView::UpdateImage,
                          base::Unretained(this)));

  UpdateImage();
}

HoldingSpaceItemScreenCaptureView::~HoldingSpaceItemScreenCaptureView() =
    default;

views::View* HoldingSpaceItemScreenCaptureView::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  // Tooltip events should be handled top level, not by descendents.
  return HitTestPoint(point) ? this : nullptr;
}

std::u16string HoldingSpaceItemScreenCaptureView::GetTooltipText(
    const gfx::Point& point) const {
  return item() ? item()->GetText() : base::EmptyString16();
}

void HoldingSpaceItemScreenCaptureView::OnHoldingSpaceItemUpdated(
    const HoldingSpaceItem* item) {
  HoldingSpaceItemView::OnHoldingSpaceItemUpdated(item);
  if (this->item() == item)
    TooltipTextChanged();
}

void HoldingSpaceItemScreenCaptureView::OnThemeChanged() {
  HoldingSpaceItemView::OnThemeChanged();

  // Image.
  UpdateImage();

  // Primary action.
  primary_action_container()->SetBackground(
      holding_space_util::CreateCircleBackground(
          AshColorProvider::Get()->GetBaseLayerColor(
              AshColorProvider::BaseLayerType::kTransparent80)));

  if (!play_icon_)
    return;

  // Play icon.
  play_icon_->SetBackground(holding_space_util::CreateCircleBackground(
      AshColorProvider::Get()->GetBaseLayerColor(
          AshColorProvider::BaseLayerType::kTransparent80)));
  play_icon_->SetImage(gfx::CreateVectorIcon(
      vector_icons::kPlayArrowIcon, kHoldingSpaceIconSize,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kButtonIconColor)));
}

void HoldingSpaceItemScreenCaptureView::UpdateImage() {
  // If the associated `item()` has been deleted then `this` is in the process
  // of being destroyed and no action needs to be taken.
  if (!item())
    return;

  image_->SetImage(item()->image().GetImageSkia(
      kHoldingSpaceScreenCaptureSize,
      /*dark_background=*/AshColorProvider::Get()->IsDarkModeEnabled()));
  SchedulePaint();
}

BEGIN_METADATA(HoldingSpaceItemScreenCaptureView, HoldingSpaceItemView)
END_METADATA

}  // namespace ash
