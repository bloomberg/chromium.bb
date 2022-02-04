// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/close_image_button.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/views/overlay/constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/vector_icons.h"

namespace {

constexpr int kCloseButtonMargin = 8;
constexpr int kCloseButtonSize = 16;

}  // namespace

CloseImageButton::CloseImageButton(PressedCallback callback)
    : OverlayWindowImageButton(std::move(callback)) {
  SetSize(gfx::Size(kCloseButtonSize, kCloseButtonSize));
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(views::kIcCloseIcon, kCloseButtonSize,
                                 kPipWindowIconColor));

  // Accessibility.
  const std::u16string close_button_label(
      l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_CLOSE_CONTROL_TEXT));
  SetAccessibleName(close_button_label);
  SetTooltipText(close_button_label);
}

void CloseImageButton::SetPosition(
    const gfx::Size& size,
    OverlayWindowViews::WindowQuadrant quadrant) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (quadrant == OverlayWindowViews::WindowQuadrant::kBottomLeft) {
    views::ImageButton::SetPosition(
        gfx::Point(kCloseButtonMargin, kCloseButtonMargin));
    return;
  }
#endif

  views::ImageButton::SetPosition(
      gfx::Point(size.width() - kCloseButtonSize - kCloseButtonMargin,
                 kCloseButtonMargin));
}

BEGIN_METADATA(CloseImageButton, OverlayWindowImageButton)
END_METADATA
