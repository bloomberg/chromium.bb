// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/back_to_tab_image_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/vector_icons.h"

namespace {

const int kBackToTabButtonMargin = 8;
const int kBackToTabButtonSize = 24;

constexpr SkColor kBackToTabBgColor = gfx::kGoogleGrey700;
constexpr SkColor kBackToTabIconColor = SK_ColorWHITE;

}  // namespace

namespace views {

BackToTabImageButton::BackToTabImageButton(ButtonListener* listener)
    : ImageButton(listener),
      back_to_tab_background_(
          gfx::CreateVectorIcon(kPictureInPictureControlBackgroundIcon,
                                kBackToTabButtonSize,
                                kBackToTabBgColor)) {
  SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                    views::ImageButton::ALIGN_MIDDLE);
  SetBackgroundImageAlignment(views::ImageButton::ALIGN_LEFT,
                              views::ImageButton::ALIGN_TOP);
  SetSize(gfx::Size(kBackToTabButtonSize, kBackToTabButtonSize));
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(views::kLaunchIcon,
                                 std::round(kBackToTabButtonSize * 2.0 / 3.0),
                                 kBackToTabIconColor));

  // Accessibility.
  SetFocusForPlatform();
  const base::string16 back_to_tab_button_label(l10n_util::GetStringUTF16(
      IDS_PICTURE_IN_PICTURE_BACK_TO_TAB_CONTROL_TEXT));
  SetAccessibleName(back_to_tab_button_label);
  SetTooltipText(back_to_tab_button_label);
  SetInstallFocusRingOnFocus(true);
}

void BackToTabImageButton::StateChanged(ButtonState old_state) {
  ImageButton::StateChanged(old_state);

  if (state() == STATE_HOVERED || state() == STATE_PRESSED) {
    SetBackgroundImage(kBackToTabBgColor, &back_to_tab_background_,
                       &back_to_tab_background_);
  } else {
    SetBackgroundImage(kBackToTabBgColor, nullptr, nullptr);
  }
}

void BackToTabImageButton::OnFocus() {
  ImageButton::OnFocus();
  SetBackgroundImage(kBackToTabBgColor, &back_to_tab_background_,
                     &back_to_tab_background_);
}

void BackToTabImageButton::OnBlur() {
  ImageButton::OnBlur();
  SetBackgroundImage(kBackToTabBgColor, nullptr, nullptr);
}

void BackToTabImageButton::SetPosition(
    const gfx::Size& size,
    OverlayWindowViews::WindowQuadrant quadrant) {
#if defined(OS_CHROMEOS)
  if (quadrant == OverlayWindowViews::WindowQuadrant::kTopLeft) {
    ImageButton::SetPosition(gfx::Point(
        kBackToTabButtonMargin,
        size.height() - kBackToTabButtonSize - kBackToTabButtonMargin));
    return;
  }
#endif
  ImageButton::SetPosition(gfx::Point(
      size.width() - kBackToTabButtonSize - kBackToTabButtonMargin,
      size.height() - kBackToTabButtonSize - kBackToTabButtonMargin));
}

}  // namespace views
