// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_shutdown_confirmation_bubble.h"

#include "ash/constants/ash_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace ash {
namespace {

// The preferred width of the shutdown confirmation bubble's content.
constexpr int kBubblePreferredWidth = 175;

// The insets of the shutdown confirmation bubble. The value of
// Insets-leading-side is taken from the system.
constexpr int kShutdownConfirmationBubbleInsetsBottom = 12;
constexpr int kShutdownConfirmationBubbleInsetsTop = 8;

const gfx::Insets GetShutdownConfirmationBubbleInsets() {
  gfx::Insets insets = GetTrayBubbleInsets();
  insets.set_top(kShutdownConfirmationBubbleInsetsTop);
  insets.set_bottom(kShutdownConfirmationBubbleInsetsBottom);
  return insets;
}

}  // namespace

ShelfShutdownConfirmationBubble::ShelfShutdownConfirmationBubble(
    views::View* anchor,
    ShelfAlignment alignment,
    SkColor background_color,
    base::OnceClosure on_confirm_callback,
    base::OnceClosure on_cancel_callback)
    : ShelfBubble(anchor, alignment, background_color) {
  DCHECK(on_confirm_callback);
  DCHECK(on_cancel_callback);
  confirm_callback_ = std::move(on_confirm_callback);
  cancel_callback_ = std::move(on_cancel_callback);

  auto* layout_provider = views::LayoutProvider::Get();
  const gfx::Insets kShutdownConfirmationBubbleInsets =
      GetShutdownConfirmationBubbleInsets();
  const gfx::Insets dialog_insets =
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG);
  set_margins(kShutdownConfirmationBubbleInsets + dialog_insets);
  set_close_on_deactivate(true);

  views::FlexLayout* layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  // Set up the icon.
  icon_ = AddChildView(std::make_unique<views::ImageView>());
  icon_->SetProperty(
      views::kMarginsKey,
      gfx::Insets(/*top=*/0, /*left=*/0,
                  layout_provider->GetDistanceMetric(
                      views::DISTANCE_UNRELATED_CONTROL_VERTICAL),
                  /*right=*/0));

  // Set up the title view.
  title_ = AddChildView(std::make_unique<views::Label>());
  title_->SetMultiLine(true);
  title_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  TrayPopupUtils::SetLabelFontList(title_,
                                   TrayPopupUtils::FontStyle::kSubHeader);
  title_->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_SHUTDOWN_CONFIRMATION_TITLE));
  title_->SetProperty(
      views::kMarginsKey,
      gfx::Insets(/*top=*/0, /*left=*/0,
                  layout_provider->GetDistanceMetric(
                      views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_TEXT),
                  /*right=*/0));

  // Set up layout row for the buttons of cancellation and confirmation.
  views::View* button_container = AddChildView(std::make_unique<views::View>());

  button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      layout_provider->GetDistanceMetric(
          views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));

  auto cancel_button = std::make_unique<PillButton>(
      base::BindRepeating(&ShelfShutdownConfirmationBubble::OnCancelled,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_CANCEL), PillButton::Type::kIconless,
      /*icon=*/nullptr);
  cancel_button->SetID(kCancel);
  cancel_ = button_container->AddChildView(std::move(cancel_button));

  auto confirm_button = std::make_unique<PillButton>(
      base::BindRepeating(&ShelfShutdownConfirmationBubble::OnConfirmed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_ASH_SHUTDOWN_CONFIRM_BUTTON),
      PillButton::Type::kIconless, /*icon=*/nullptr);
  confirm_button->SetID(kShutdown);
  confirm_ = button_container->AddChildView(std::move(confirm_button));

  CreateBubble();

  auto bubble_border =
      std::make_unique<views::BubbleBorder>(arrow(), GetShadow(), color());
  bubble_border->set_insets(kShutdownConfirmationBubbleInsets);
  bubble_border->set_use_theme_background_color(true);
  bubble_border->SetCornerRadius(
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::Emphasis::kHigh));
  GetBubbleFrameView()->SetBubbleBorder(std::move(bubble_border));
  GetWidget()->Show();
}

ShelfShutdownConfirmationBubble::~ShelfShutdownConfirmationBubble() {
  // In case shutdown confirmation bubble was dismissed, the pointer of the
  // ShelfShutdownConfirmationBubble in LoginShelfView shall be cleaned up.
  if (cancel_callback_) {
    std::move(cancel_callback_).Run();
  }
}

void ShelfShutdownConfirmationBubble::OnThemeChanged() {
  views::View::OnThemeChanged();
  auto* color_provider = AshColorProvider::Get();

  SkColor icon_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor);
  icon_->SetImage(
      gfx::CreateVectorIcon(vector_icons::kWarningOutlineIcon, icon_color));

  SkColor label_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
  title_->SetEnabledColor(label_color);

  SkColor button_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonLabelColor);
  cancel_->SetEnabledTextColors(button_color);
  confirm_->SetEnabledTextColors(button_color);
}

void ShelfShutdownConfirmationBubble::OnCancelled() {
  GetWidget()->Close();
  std::move(cancel_callback_).Run();
}

void ShelfShutdownConfirmationBubble::OnConfirmed() {
  GetWidget()->Close();
  std::move(confirm_callback_).Run();
}

gfx::Size ShelfShutdownConfirmationBubble::CalculatePreferredSize() const {
  return gfx::Size(kBubblePreferredWidth,
                   GetHeightForWidth(kBubblePreferredWidth));
}

bool ShelfShutdownConfirmationBubble::ShouldCloseOnPressDown() {
  return true;
}

bool ShelfShutdownConfirmationBubble::ShouldCloseOnMouseExit() {
  return false;
}

}  // namespace ash