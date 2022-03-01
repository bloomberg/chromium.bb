// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/sharesheet/sharesheet_target_button.h"

#include <memory>

#include "ash/public/cpp/ash_typography.h"
#include "ash/public/cpp/style/scoped_light_mode_as_default.h"
#include "ash/style/ash_color_provider.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"

namespace {

// Sizes are in px.
// kButtonTextMaxWidth is button max width without padding.
constexpr int kButtonTextMaxWidth = 76;
constexpr int kButtonMaxLines = 2;
constexpr int kButtonPadding = 8;

std::unique_ptr<views::ImageView> CreateImageView(
    const absl::optional<gfx::ImageSkia> icon,
    const gfx::VectorIcon* vector_icon) {
  auto image = std::make_unique<views::ImageView>();
  if (icon.has_value()) {
    image->SetImage(icon.value());
    return image;
  }
  if (vector_icon != nullptr) {
    ash::ScopedLightModeAsDefault scoped_light_mode_as_default;
    auto* color_provider = ash::AshColorProvider::Get();
    const auto icon_color = color_provider->GetContentLayerColor(
        ash::AshColorProvider::ContentLayerType::kIconColorProminent);
    gfx::ImageSkia icon = gfx::CreateVectorIcon(
        *vector_icon, sharesheet::kIconSize / 2, icon_color);
    gfx::ImageSkia circle_icon =
        gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
            sharesheet::kIconSize / 2, color_provider->GetBackgroundColor(),
            icon);

    // TODO(crbug.com/1184414): Replace hard-coded values when shadow styles
    // are implemented.
    gfx::ShadowValues shadow_values;
    shadow_values.push_back(gfx::ShadowValue(gfx::Vector2d(0, 1), 0,
                                             SkColorSetARGB(0x33, 0, 0, 0)));
    shadow_values.push_back(gfx::ShadowValue(gfx::Vector2d(0, 1), 2,
                                             SkColorSetARGB(0x33, 0, 0, 0)));
    gfx::ImageSkia circle_icon_with_shadow =
        gfx::ImageSkiaOperations::CreateImageWithDropShadow(circle_icon,
                                                            shadow_values);
    image->SetImage(circle_icon_with_shadow);
  }
  // There must always be either an icon or a vector icon so image must be
  // set here.
  DCHECK(!image->GetImageModel().IsEmpty());
  return image;
}

}  // namespace

namespace ash {
namespace sharesheet {

// A button that represents a candidate share target.
// Only apps will have |icon| values, while share_actions will have a
// |vector_icon| which is used to generate a |ColorTrackingIconView|. If
// |icon| has a value |vector_icon| should be nullptr and vice versa. There
// should never be a case where both don't have values or both have values.
// It is safe to use |vector_icon| as a raw pointer because it has the same
// lifetime as the |SharesheetService|, which outlives|SharesheetTargetButton|
// as it is a transient UI invoked from the |SharesheetService|.
SharesheetTargetButton::SharesheetTargetButton(
    PressedCallback callback,
    const std::u16string& display_name,
    const std::u16string& secondary_display_name,
    const absl::optional<gfx::ImageSkia> icon,
    const gfx::VectorIcon* vector_icon)
    : Button(std::move(callback)) {
  // TODO(crbug.com/1097623) Margins shouldn't be within
  // SharesheetTargetButton as the margins are different in |expanded_view_|.
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(kButtonPadding),
      kButtonPadding, true));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  auto* image = AddChildView(CreateImageView(icon, vector_icon));
  image->SetCanProcessEventsWithinSubtree(false);

  auto label_view = std::make_unique<views::View>();
  label_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0, true));

  auto* label = label_view->AddChildView(std::make_unique<views::Label>(
      display_name, CONTEXT_SHARESHEET_BUBBLE_BODY, STYLE_SHARESHEET));
  label->SetEnabledColor(kPrimaryTextColor);
  SetLabelProperties(label);

  std::u16string accessible_name = display_name;
  if (secondary_display_name != std::u16string() &&
      secondary_display_name != display_name) {
    auto* secondary_label =
        label_view->AddChildView(std::make_unique<views::Label>(
            secondary_display_name, CONTEXT_SHARESHEET_BUBBLE_BODY_SECONDARY,
            STYLE_SHARESHEET));
    secondary_label->SetEnabledColor(kSecondaryTextColor);
    SetLabelProperties(secondary_label);
    accessible_name =
        base::StrCat({display_name, u" ", secondary_display_name});
    // As there is a secondary label, don't let the initial label stretch across
    // multiple lines.
    label->SetMultiLine(false);
    secondary_label->SetMultiLine(false);
  } else {
    label->SetMaxLines(kButtonMaxLines);
  }

  AddChildView(std::move(label_view));
  SetAccessibleName(accessible_name);
}

void SharesheetTargetButton::SetLabelProperties(views::Label* label) {
  label->SetMultiLine(true);
  label->SetMaximumWidth(kButtonTextMaxWidth);
  label->SetBackgroundColor(SK_ColorTRANSPARENT);
  label->SetHandlesTooltips(true);
  label->SetTooltipText(label->GetText());
  label->SetAutoColorReadabilityEnabled(false);
  label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
}

BEGIN_METADATA(SharesheetTargetButton, views::Button)
END_METADATA

}  // namespace sharesheet
}  // namespace ash
