// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/omnibox_chip_button.h"

#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/layout_constants.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/painter.h"

namespace {

// Padding between chip's icon and label.
constexpr int kChipImagePadding = 4;
// An extra space between chip's label and right edge.
constexpr int kExtraRightPadding = 4;

}  // namespace

OmniboxChipButton::OmniboxChipButton(PressedCallback callback,
                                     const gfx::VectorIcon& icon_on,
                                     const gfx::VectorIcon& icon_off,
                                     std::u16string message,
                                     bool is_prominent)
    : MdTextButton(std::move(callback),
                   std::u16string(),
                   views::style::CONTEXT_BUTTON_MD),
      icon_on_(icon_on),
      icon_off_(icon_off) {
  views::InstallPillHighlightPathGenerator(this);
  SetText(message);
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
  SetElideBehavior(gfx::ElideBehavior::FADE_TAIL);
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  // Equalizing padding on the left, right and between icon and label.
  SetImageLabelSpacing(kChipImagePadding);
  SetCustomPadding(
      gfx::Insets(GetLayoutConstant(LOCATION_BAR_CHILD_INTERIOR_PADDING),
                  GetLayoutInsets(LOCATION_BAR_ICON_INTERIOR_PADDING).left()));

  constexpr auto kAnimationDuration = base::Milliseconds(350);
  animation_ = std::make_unique<gfx::SlideAnimation>(this);
  animation_->SetSlideDuration(kAnimationDuration);

  UpdateIconAndColors();
}

OmniboxChipButton::~OmniboxChipButton() = default;

void OmniboxChipButton::AnimateCollapse() {
  constexpr auto kAnimationDuration = base::Milliseconds(250);
  animation_->SetSlideDuration(kAnimationDuration);
  animation_->Hide();
}

void OmniboxChipButton::AnimateExpand() {
  constexpr auto kAnimationDuration = base::Milliseconds(350);
  animation_->SetSlideDuration(kAnimationDuration);
  animation_->Show();
}

void OmniboxChipButton::ResetAnimation(double value) {
  animation_->Reset(value);
}

void OmniboxChipButton::SetExpandAnimationEndedCallback(
    base::RepeatingCallback<void()> callback) {
  expand_animation_ended_callback_ = callback;
}

gfx::Size OmniboxChipButton::CalculatePreferredSize() const {
  const int fixed_width = GetIconSize() + GetInsets().width();
  const int collapsable_width = label()->GetPreferredSize().width() +
                                kChipImagePadding + kExtraRightPadding;
  const double animation_value =
      force_expanded_for_testing_ ? 1.0 : animation_->GetCurrentValue();
  const int width =
      std::round(collapsable_width * animation_value) + fixed_width;
  return gfx::Size(width, GetHeightForWidth(width));
}

void OmniboxChipButton::OnThemeChanged() {
  MdTextButton::OnThemeChanged();
  UpdateIconAndColors();
}

void OmniboxChipButton::UpdateBackgroundColor() {
  SetBackground(
      CreateBackgroundFromPainter(views::Painter::CreateSolidRoundRectPainter(
          GetBackgroundColor(), GetIconSize())));
}

void OmniboxChipButton::AnimationEnded(const gfx::Animation* animation) {
  if (animation != animation_.get())
    return;

  fully_collapsed_ = animation->GetCurrentValue() != 1.0;
  if (animation->GetCurrentValue() == 1.0)
    expand_animation_ended_callback_.Run();
}

void OmniboxChipButton::AnimationProgressed(const gfx::Animation* animation) {
  if (animation == animation_.get())
    PreferredSizeChanged();
}

void OmniboxChipButton::SetTheme(Theme theme) {
  theme_ = theme;
  UpdateIconAndColors();
}

int OmniboxChipButton::GetIconSize() const {
  return GetLayoutConstant(LOCATION_BAR_ICON_SIZE);
}

void OmniboxChipButton::UpdateIconAndColors() {
  if (!GetWidget())
    return;
  SetEnabledTextColors(GetTextAndIconColor());
  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(
                    show_blocked_icon_ ? icon_off_ : icon_on_,
                    GetTextAndIconColor(), GetIconSize(), nullptr));
}

SkColor OmniboxChipButton::GetTextAndIconColor() {
  switch (theme_) {
    case Theme::kNormalVisibility: {
      // TODO(crbug.com/1274118) Instead of using constants or toolbar colors,
      // add the chip's properties.
      return color_utils::IsDark(
                 GetThemeProvider()->GetColor(ThemeProperties::COLOR_TOOLBAR))
                 ? gfx::kGoogleBlue300
                 : gfx::kGoogleBlue600;
    }
    case Theme::kLowVisibility: {
      return GetThemeProvider()->GetColor(
          ThemeProperties::COLOR_TAB_FOREGROUND_ACTIVE_FRAME_ACTIVE);
    }
  }
}

SkColor OmniboxChipButton::GetBackgroundColor() {
  SkColor active_tab_color =
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_TOOLBAR);

  if (theme_ == Theme::kLowVisibility) {
    return active_tab_color;
  }

  // TODO(crbug.com/1274118) Instead of using constants or toolbar colors, add
  // the chip's properties.
  return ThemeProperties::GetDefaultColor(
      ThemeProperties::COLOR_TOOLBAR, false,
      /*dark_mode=*/color_utils::IsDark(active_tab_color));
}

void OmniboxChipButton::SetForceExpandedForTesting(
    bool force_expanded_for_testing) {
  force_expanded_for_testing_ = force_expanded_for_testing;
}

void OmniboxChipButton::SetShowBlockedIcon(bool show_blocked_icon) {
  if (show_blocked_icon_ != show_blocked_icon) {
    show_blocked_icon_ = show_blocked_icon;
    theme_ =
        show_blocked_icon ? Theme::kLowVisibility : Theme::kNormalVisibility;
    UpdateIconAndColors();
  }
}

BEGIN_METADATA(OmniboxChipButton, views::MdTextButton)
ADD_READONLY_PROPERTY_METADATA(int, IconSize)
END_METADATA
