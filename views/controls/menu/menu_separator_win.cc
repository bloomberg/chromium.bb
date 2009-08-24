// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_separator.h"

#include <windows.h>
#include <uxtheme.h>
#include <Vssym32.h>

#include "app/gfx/canvas.h"
#include "base/gfx/native_theme.h"
#include "views/controls/menu/menu_config.h"
#include "views/controls/menu/menu_item_view.h"

namespace views {

void MenuSeparator::Paint(gfx::Canvas* canvas) {
  const MenuConfig& config = MenuConfig::instance();
  // The gutter is rendered before the background.
  int start_x = 0;
  int start_y = height() / 3;
  HDC dc = canvas->beginPlatformPaint();
  if (config.render_gutter) {
    // If render_gutter is true, we're on Vista and need to render the
    // gutter, then indent the separator from the gutter.
    RECT gutter_bounds = { MenuItemView::label_start() -
                           config.gutter_to_label - config.gutter_width, 0, 0,
                           height() };
    gutter_bounds.right = gutter_bounds.left + config.gutter_width;
    gfx::NativeTheme::instance()->PaintMenuGutter(dc, MENU_POPUPGUTTER,
                                                  MPI_NORMAL, &gutter_bounds);
    start_x = gutter_bounds.left + config.gutter_width;
    start_y = 0;
  }
  RECT separator_bounds = { start_x, start_y, width(), height() };
  gfx::NativeTheme::instance()->PaintMenuSeparator(
      dc, MENU_POPUPSEPARATOR, MPI_NORMAL, &separator_bounds);
  canvas->endPlatformPaint();
}

gfx::Size MenuSeparator::GetPreferredSize() {
  return gfx::Size(10,  // Just in case we're the only item in a menu.
                   MenuConfig::instance().separator_height);
}

}  // namespace views
