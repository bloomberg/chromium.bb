// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_center_focus_border.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"
#include "ui/message_center/message_center_style.h"
#include "ui/views/view.h"

namespace message_center {

MessageCenterFocusBorder::MessageCenterFocusBorder() {}

MessageCenterFocusBorder::~MessageCenterFocusBorder() {}

void MessageCenterFocusBorder::Paint(const views::View& view,
                                     gfx::Canvas* canvas) const {
  gfx::Rect rect(view.GetLocalBounds());
  rect.Inset(2, 1, 2, 3);
  canvas->DrawRect(rect, kFocusBorderColor);
}

}  // namespace message_center
