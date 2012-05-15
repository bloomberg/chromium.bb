// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_separator.h"

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/base/native_theme/native_theme.h"
#include "ui/views/controls/menu/menu_config.h"

namespace {

const int kSeparatorHeight = 1;

}  // namespace

namespace views {

void MenuSeparator::OnPaint(gfx::Canvas* canvas) {
  canvas->FillRect(gfx::Rect(0, height() / 2, width(), kSeparatorHeight),
      ui::NativeTheme::instance()->GetSystemColor(
          ui::NativeTheme::kColorId_MenuSeparatorColor));
}

gfx::Size MenuSeparator::GetPreferredSize() {
  return gfx::Size(10,  // Just in case we're the only item in a menu.
                   MenuConfig::instance().separator_height);
}

}  // namespace views
