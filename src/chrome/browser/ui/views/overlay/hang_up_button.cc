// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/hang_up_button.h"

#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {

constexpr SkColor kHangUpButtonColor = gfx::kGoogleRed300;

}  // namespace

HangUpButton::HangUpButton(PressedCallback callback)
    : OverlayWindowImageButton(std::move(callback)) {
  SetTooltipText(
      l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_HANG_UP_TEXT));
  UpdateImage();
}

void HangUpButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (size() == previous_bounds.size())
    return;

  UpdateImage();
}

void HangUpButton::UpdateImage() {
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(vector_icons::kCallEndIcon, width(),
                                 kHangUpButtonColor));
}

BEGIN_METADATA(HangUpButton, OverlayWindowImageButton)
END_METADATA
