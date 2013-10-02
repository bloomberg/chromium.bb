// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/tooltip_manager.h"

#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"

namespace views {

const size_t kMaxTooltipLength = 1024;

// static
int TooltipManager::GetMaxWidth(int x, int y, gfx::NativeView context) {
  gfx::Rect monitor_bounds =
      gfx::Screen::GetScreenFor(context)->GetDisplayNearestPoint(
          gfx::Point(x, y)).bounds();
  return (monitor_bounds.width() + 1) / 2;
}

// static
void TooltipManager::TrimTooltipText(base::string16* text) {
  // Clamp the tooltip length to kMaxTooltipLength so that we don't
  // accidentally DOS the user with a mega tooltip.
  if (text->length() > kMaxTooltipLength)
    *text = text->substr(0, kMaxTooltipLength);
}

}  // namespace views
