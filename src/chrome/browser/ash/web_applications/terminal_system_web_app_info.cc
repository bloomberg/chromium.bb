// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/terminal_system_web_app_info.h"

#include <memory>

#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"
#include "url/gurl.h"

namespace {
constexpr gfx::Rect TERMINAL_DEFAULT_BOUNDS(gfx::Point(64, 64),
                                            gfx::Size(652, 484));
constexpr gfx::Size TERMINAL_SETTINGS_DEFAULT_SIZE(768, 512);
}  // namespace

std::unique_ptr<WebApplicationInfo> CreateWebAppInfoForTerminalSystemWebApp() {
  auto info = std::make_unique<WebApplicationInfo>();
  // URL used for crostini::kCrostiniTerminalSystemAppId.
  info->start_url = GURL("chrome-untrusted://terminal/html/terminal.html");
  info->scope = GURL(chrome::kChromeUIUntrustedTerminalURL);
  info->title = l10n_util::GetStringUTF16(IDS_CROSTINI_TERMINAL_APP_NAME);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url, {{"app_icon_256.png", 256, IDR_LOGO_CROSTINI_TERMINAL}},
      *info);
  info->background_color = 0xFF202124;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  return info;
}

gfx::Rect GetDefaultBoundsForTerminal(Browser* browser) {
  if (browser->is_type_app_popup()) {
    gfx::Rect bounds =
        display::Screen::GetScreen()->GetDisplayForNewWindows().work_area();
    bounds.ClampToCenteredSize(TERMINAL_SETTINGS_DEFAULT_SIZE);
    return bounds;
  }
  return TERMINAL_DEFAULT_BOUNDS;
}
