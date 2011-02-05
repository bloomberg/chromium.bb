// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_config.h"

#include <windows.h>
#include <uxtheme.h>
#include <Vssym32.h>

#include "base/logging.h"
#include "base/win/win_util.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/gfx/native_theme_win.h"

using gfx::NativeTheme;

namespace views {

// static
MenuConfig* MenuConfig::Create() {
  MenuConfig* config = new MenuConfig();

  config->text_color = NativeTheme::instance()->GetThemeColorWithDefault(
      NativeTheme::MENU, MENU_POPUPITEM, MPI_NORMAL, TMT_TEXTCOLOR,
      COLOR_MENUTEXT);

  NONCLIENTMETRICS metrics;
  base::win::GetNonClientMetrics(&metrics);
  l10n_util::AdjustUIFont(&(metrics.lfMenuFont));
  HFONT font = CreateFontIndirect(&metrics.lfMenuFont);
  DLOG_ASSERT(font);
  config->font = gfx::Font(font);

  HDC dc = GetDC(NULL);
  RECT bounds = { 0, 0, 200, 200 };
  SIZE check_size;
  if (NativeTheme::instance()->GetThemePartSize(
          NativeTheme::MENU, dc, MENU_POPUPCHECK, MC_CHECKMARKNORMAL, &bounds,
          TS_TRUE, &check_size) == S_OK) {
    config->check_width = check_size.cx;
    config->check_height = check_size.cy;
  } else {
    config->check_width = GetSystemMetrics(SM_CXMENUCHECK);
    config->check_height = GetSystemMetrics(SM_CYMENUCHECK);
  }

  SIZE radio_size;
  if (NativeTheme::instance()->GetThemePartSize(
          NativeTheme::MENU, dc, MENU_POPUPCHECK, MC_BULLETNORMAL, &bounds,
          TS_TRUE, &radio_size) == S_OK) {
    config->radio_width = radio_size.cx;
    config->radio_height = radio_size.cy;
  } else {
    config->radio_width = GetSystemMetrics(SM_CXMENUCHECK);
    config->radio_height = GetSystemMetrics(SM_CYMENUCHECK);
  }

  SIZE arrow_size;
  if (NativeTheme::instance()->GetThemePartSize(
          NativeTheme::MENU, dc, MENU_POPUPSUBMENU, MSM_NORMAL, &bounds,
          TS_TRUE, &arrow_size) == S_OK) {
    config->arrow_width = arrow_size.cx;
    config->arrow_height = arrow_size.cy;
  } else {
    // Sadly I didn't see a specify metrics for this.
    config->arrow_width = GetSystemMetrics(SM_CXMENUCHECK);
    config->arrow_height = GetSystemMetrics(SM_CYMENUCHECK);
  }

  SIZE gutter_size;
  if (NativeTheme::instance()->GetThemePartSize(
          NativeTheme::MENU, dc, MENU_POPUPGUTTER, MSM_NORMAL, &bounds,
          TS_TRUE, &gutter_size) == S_OK) {
    config->gutter_width = gutter_size.cx;
    config->render_gutter = true;
  } else {
    config->gutter_width = 0;
    config->render_gutter = false;
  }

  SIZE separator_size;
  if (NativeTheme::instance()->GetThemePartSize(
          NativeTheme::MENU, dc, MENU_POPUPSEPARATOR, MSM_NORMAL, &bounds,
          TS_TRUE, &separator_size) == S_OK) {
    config->separator_height = separator_size.cy;
  } else {
    // -1 makes separator centered.
    config->separator_height = GetSystemMetrics(SM_CYMENU) / 2 - 1;
  }

  ReleaseDC(NULL, dc);

  BOOL show_cues;
  config->show_mnemonics =
      (SystemParametersInfo(SPI_GETKEYBOARDCUES, 0, &show_cues, 0) &&
       show_cues == TRUE);
  return config;
}

}  // namespace views
