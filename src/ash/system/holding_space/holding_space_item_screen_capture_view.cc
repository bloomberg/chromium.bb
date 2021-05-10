// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_screen_capture_view.h"

#include "ash/public/cpp/holding_space/holding_space_color_provider.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/rounded_image_view.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/holding_space/holding_space_util.h"
#include "ash/system/tray/tray_constants.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

// Appearance.
constexpr gfx::Insets kCheckmarkAndPinButtonContainerPadding(4);
constexpr gfx::Size kPinButtonSize(24, 24);
constexpr gfx::Size kPlayIconSize(32, 32);

HoldingSpaceItemScreenCaptureView::HoldingSpaceItemScreenCaptureView(
    HoldingSpaceItemViewDelegate* delegate,
    const HoldingSpaceItem* item)
    : HoldingSpaceItemView(delegate, item) {
  SetPreferredSize(kHoldingSpaceScreenCaptureSize);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  image_ = AddChildView(std::make_unique<RoundedImageView>(
      kHoldingSpaceCornerRadius, RoundedImageView::Alignment::kLeading));
  image_->SetID(kHoldingSpaceItemImageId);

  // Subscribe to be notified of changes to `item_`'s image.
  image_subscription_ = item->image().AddImageSkiaChangedCallback(
      base::BindRepeating(&HoldingSpaceItemScreenCaptureView::UpdateImage,
                          base::Unretained(this)));

  UpdateImage();

  if (item->type() == HoldingSpaceItem::Type::kScreenRecording)
    AddPlayIcon();

  views::View* checkmark_and_pin_button_container =
      AddChildView(std::make_unique<views::View>());
  auto* layout = checkmark_and_pin_button_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          kCheckmarkAndPinButtonContainerPadding));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  // Checkmark.
  AddCheckmark(/*parent=*/checkmark_and_pin_button_container);

  // Spacer.
  views::View* spacer = checkmark_and_pin_button_container->AddChildView(
      std::make_unique<views::View>());
  layout->SetFlexForView(spacer, 1);

  // Pin.
  auto* pin = AddPin(/*parent=*/checkmark_and_pin_button_container);
  pin->SetPreferredSize(kPinButtonSize);
}

HoldingSpaceItemScreenCaptureView::~HoldingSpaceItemScreenCaptureView() =
    default;

views::View* HoldingSpaceItemScreenCaptureView::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  // Tooltip events should be handled top level, not by descendents.
  return HitTestPoint(point) ? this : nullptr;
}

base::string16 HoldingSpaceItemScreenCaptureView::GetTooltipText(
    const gfx::Point& point) const {
  return item()->text();
}

void HoldingSpaceItemScreenCaptureView::OnHoldingSpaceItemUpdated(
    const HoldingSpaceItem* item) {
  HoldingSpaceItemView::OnHoldingSpaceItemUpdated(item);
  if (this->item() == item)
    TooltipTextChanged();
}

void HoldingSpaceItemScreenCaptureView::OnThemeChanged() {
  HoldingSpaceItemView::OnThemeChanged();
  pin()->SetBackground(holding_space_util::CreateCircleBackground(
      HoldingSpaceColorProvider::Get()->GetBackgroundColor()));
}

void HoldingSpaceItemScreenCaptureView::UpdateImage() {
  image_->SetImage(
      item()->image().GetImageSkia(kHoldingSpaceScreenCaptureSize));
  SchedulePaint();
}

void HoldingSpaceItemScreenCaptureView::AddPlayIcon() {
  auto* play_icon_container = AddChildView(std::make_unique<views::View>());
  play_icon_container->SetFocusBehavior(views::View::FocusBehavior::NEVER);

  auto* layout =
      play_icon_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  auto* play_icon =
      play_icon_container->AddChildView(std::make_unique<views::ImageView>());
  play_icon->SetID(kHoldingSpaceScreenCapturePlayIconId);
  play_icon->SetBackground(holding_space_util::CreateCircleBackground(
      HoldingSpaceColorProvider::Get()->GetBackgroundColor()));
  play_icon->SetImage(gfx::CreateVectorIcon(
      vector_icons::kPlayArrowIcon, kHoldingSpaceIconSize,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kButtonIconColor)));
  play_icon->SetPreferredSize(kPlayIconSize);
}

BEGIN_METADATA(HoldingSpaceItemScreenCaptureView, HoldingSpaceItemView)
END_METADATA

}  // namespace ash
