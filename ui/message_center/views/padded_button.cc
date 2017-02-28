// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/padded_button.h"

#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/message_center/message_center_style.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/painter.h"

namespace message_center {

PaddedButton::PaddedButton(views::ButtonListener* listener)
    : views::ImageButton(listener) {
  SetFocusForPlatform();
  SetFocusPainter(views::Painter::CreateSolidFocusPainter(
      kFocusBorderColor,
      gfx::Insets(1, 2, 2, 2)));
  SetBorder(
      views::CreateEmptyBorder(gfx::Insets(kControlButtonPaddingFromBorder)));
  set_animate_on_state_change(false);
}

PaddedButton::~PaddedButton() {
}

}  // namespace message_center
