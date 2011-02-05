// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_separator.h"

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "views/controls/menu/menu_config.h"

namespace views {

static const SkColor kSeparatorColor = SkColorSetARGB(50, 00, 00, 00);

void MenuSeparator::Paint(gfx::Canvas* canvas) {
  canvas->DrawLineInt(kSeparatorColor, 0, height() / 2, width(), height() / 2);
}

gfx::Size MenuSeparator::GetPreferredSize() {
  return gfx::Size(10,  // Just in case we're the only item in a menu.
                   MenuConfig::instance().separator_height);
}

}  // namespace views
