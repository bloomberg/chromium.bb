// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_separator.h"

#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/native_theme/native_theme.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/menu/menu_config.h"

namespace {

const int kSeparatorHeight = 1;

}  // namespace

namespace views {

void MenuSeparator::OnPaint(gfx::Canvas* canvas) {
  int pos = 0;
  if (type_ == ui::LOWER_SEPARATOR)
    pos = height() - kSeparatorHeight;
  else if (type_ != ui::SPACING_SEPARATOR)
    pos = height() / 2;
  else if (type_ != ui::UPPER_SEPARATOR)
    return;
  canvas->FillRect(gfx::Rect(0, pos, width(), kSeparatorHeight),
      ui::NativeTheme::instance()->GetSystemColor(
            ui::NativeTheme::kColorId_MenuSeparatorColor));
}

gfx::Size MenuSeparator::GetPreferredSize() {
  int height = MenuConfig::instance().separator_height;
  switch(type_) {
    case ui::SPACING_SEPARATOR:
      height = MenuConfig::instance().separator_spacing_height;
      break;
    case ui::LOWER_SEPARATOR:
      height = MenuConfig::instance().separator_lower_height;
      break;
    case ui::UPPER_SEPARATOR:
      height = MenuConfig::instance().separator_upper_height;
      break;
    default:
      height = MenuConfig::instance().separator_height;
      break;
  }
  return gfx::Size(10,  // Just in case we're the only item in a menu.
                   height);
}

}  // namespace views
