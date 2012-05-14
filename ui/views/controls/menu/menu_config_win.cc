// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

#include <windows.h>
#include <uxtheme.h>
#include <Vssym32.h>

#include "base/logging.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/win_util.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/base/native_theme/native_theme_win.h"

using ui::NativeTheme;
using ui::NativeThemeWin;

namespace views {

// static
MenuConfig* MenuConfig::Create() {
  MenuConfig* config = new MenuConfig();

  config->text_color = NativeThemeWin::instance()->GetThemeColorWithDefault(
      NativeThemeWin::MENU, MENU_POPUPITEM, MPI_NORMAL, TMT_TEXTCOLOR,
      COLOR_MENUTEXT);

  NONCLIENTMETRICS metrics;
  base::win::GetNonClientMetrics(&metrics);
  l10n_util::AdjustUIFont(&(metrics.lfMenuFont));
  {
    base::win::ScopedHFONT font(CreateFontIndirect(&metrics.lfMenuFont));
    DLOG_ASSERT(font.Get());
    config->font = gfx::Font(font);
  }
  NativeTheme::ExtraParams extra;
  extra.menu_check.is_radio = false;
  extra.menu_check.is_selected = false;
  gfx::Size check_size = NativeTheme::instance()->GetPartSize(
      NativeTheme::kMenuCheck, NativeTheme::kNormal, extra);
  if (!check_size.IsEmpty()) {
    config->check_width = check_size.width();
    config->check_height = check_size.height();
  } else {
    config->check_width = GetSystemMetrics(SM_CXMENUCHECK);
    config->check_height = GetSystemMetrics(SM_CYMENUCHECK);
  }

  extra.menu_check.is_radio = true;
  gfx::Size radio_size = NativeTheme::instance()->GetPartSize(
      NativeTheme::kMenuCheck, NativeTheme::kNormal, extra);
  if (!radio_size.IsEmpty()) {
    config->radio_width = radio_size.width();
    config->radio_height = radio_size.height();
  } else {
    config->radio_width = GetSystemMetrics(SM_CXMENUCHECK);
    config->radio_height = GetSystemMetrics(SM_CYMENUCHECK);
  }

  gfx::Size arrow_size = NativeTheme::instance()->GetPartSize(
      NativeTheme::kMenuPopupArrow, NativeTheme::kNormal, extra);
  if (!arrow_size.IsEmpty()) {
    config->arrow_width = arrow_size.width();
    config->arrow_height = arrow_size.height();
  } else {
    // Sadly I didn't see a specify metrics for this.
    config->arrow_width = GetSystemMetrics(SM_CXMENUCHECK);
    config->arrow_height = GetSystemMetrics(SM_CYMENUCHECK);
  }

  gfx::Size gutter_size = NativeTheme::instance()->GetPartSize(
      NativeTheme::kMenuPopupGutter, NativeTheme::kNormal, extra);
  if (!gutter_size.IsEmpty()) {
    config->gutter_width = gutter_size.width();
    config->render_gutter = true;
  } else {
    config->gutter_width = 0;
    config->render_gutter = false;
  }

  gfx::Size separator_size = NativeTheme::instance()->GetPartSize(
      NativeTheme::kMenuPopupSeparator, NativeTheme::kNormal, extra);
  if (!separator_size.IsEmpty()) {
    config->separator_height = separator_size.height();
  } else {
    // -1 makes separator centered.
    config->separator_height = GetSystemMetrics(SM_CYMENU) / 2 - 1;
  }

  // On Windows, having some menus use wider spacing than others looks wrong.
  // See http://crbug.com/88875
  config->item_no_icon_bottom_margin = config->item_bottom_margin;
  config->item_no_icon_top_margin = config->item_top_margin;

  BOOL show_cues;
  config->show_mnemonics =
      (SystemParametersInfo(SPI_GETKEYBOARDCUES, 0, &show_cues, 0) &&
       show_cues == TRUE);
  return config;
}

}  // namespace views
