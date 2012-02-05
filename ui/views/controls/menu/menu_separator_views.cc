// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_separator.h"

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/menu/menu_config.h"

namespace views {

static const SkColor kSeparatorColor = SkColorSetARGB(50, 00, 00, 00);

void MenuSeparator::OnPaint(gfx::Canvas* canvas) {
  const int y = height() / 2;
  canvas->DrawLine(gfx::Point(0, y), gfx::Point(width(), y), kSeparatorColor);
}

gfx::Size MenuSeparator::GetPreferredSize() {
  return gfx::Size(10,  // Just in case we're the only item in a menu.
                   MenuConfig::instance().separator_height);
}

}  // namespace views
