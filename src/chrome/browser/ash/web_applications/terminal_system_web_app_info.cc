// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/terminal_system_web_app_info.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "chrome/browser/ash/crostini/crostini_terminal.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "extensions/common/constants.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/display/screen.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {
constexpr gfx::Rect TERMINAL_DEFAULT_BOUNDS(gfx::Point(64, 64),
                                            gfx::Size(652, 484));
constexpr gfx::Size TERMINAL_SETTINGS_DEFAULT_SIZE(768, 512);
}  // namespace

std::unique_ptr<WebApplicationInfo> CreateWebAppInfoForTerminalSystemWebApp() {
  auto info = std::make_unique<WebApplicationInfo>();
  // URL used for crostini::kCrostiniTerminalSystemAppId.
  const GURL terminal_url("chrome-untrusted://terminal/html/terminal.html");
  info->start_url = terminal_url;
  info->scope = GURL(chrome::kChromeUIUntrustedTerminalURL);
  info->title = l10n_util::GetStringUTF16(IDS_CROSTINI_TERMINAL_APP_NAME);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url, {{"app_icon_256.png", 256, IDR_LOGO_CROSTINI_TERMINAL}},
      *info);
  info->background_color = 0xFF202124;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  {
    apps::FileHandler handler;
    handler.accept.emplace_back();
    handler.accept.back().mime_type =
        extensions::app_file_handler_util::kMimeTypeInodeDirectory;
    info->file_handlers.push_back(std::move(handler));
  }
  info->additional_search_terms = {
      "linux", "terminal", "crostini", "ssh",
      l10n_util::GetStringUTF8(IDS_CROSTINI_TERMINAL_APP_SEARCH_TERMS)};
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

TerminalSystemAppDelegate::TerminalSystemAppDelegate(Profile* profile)
    : web_app::SystemWebAppDelegate(web_app::SystemAppType::TERMINAL,
                                    "Terminal",
                                    GURL(chrome::kChromeUIUntrustedTerminalURL),
                                    profile) {}

std::unique_ptr<WebApplicationInfo> TerminalSystemAppDelegate::GetWebAppInfo()
    const {
  return CreateWebAppInfoForTerminalSystemWebApp();
}

bool TerminalSystemAppDelegate::ShouldReuseExistingWindow() const {
  return false;
}

bool TerminalSystemAppDelegate::ShouldShowNewWindowMenuOption() const {
  return true;
}

bool TerminalSystemAppDelegate::ShouldHaveTabStrip() const {
  return true;
}

bool TerminalSystemAppDelegate::HasTitlebarTerminalSelectNewTabButton() const {
  return base::FeatureList::IsEnabled(chromeos::features::kTerminalSSH);
}

gfx::Rect TerminalSystemAppDelegate::GetDefaultBounds(Browser* browser) const {
  return GetDefaultBoundsForTerminal(browser);
}

bool TerminalSystemAppDelegate::HasCustomTabMenuModel() const {
  return true;
}

std::unique_ptr<ui::SimpleMenuModel> TerminalSystemAppDelegate::GetTabMenuModel(
    ui::SimpleMenuModel::Delegate* delegate) const {
  auto result = std::make_unique<ui::SimpleMenuModel>(delegate);
  result->AddItemWithStringId(TabStripModel::CommandNewTabToRight,
                              IDS_TAB_CXMENU_NEWTABTORIGHT);
  result->AddSeparator(ui::NORMAL_SEPARATOR);
  result->AddItemWithStringId(TabStripModel::CommandCloseTab,
                              IDS_TAB_CXMENU_CLOSETAB);
  result->AddItemWithStringId(TabStripModel::CommandCloseOtherTabs,
                              IDS_TAB_CXMENU_CLOSEOTHERTABS);
  result->AddItemWithStringId(TabStripModel::CommandCloseTabsToRight,
                              IDS_TAB_CXMENU_CLOSETABSTORIGHT);
  return result;
}

bool TerminalSystemAppDelegate::ShouldShowTabContextMenuShortcut(
    Profile* profile,
    int command_id) const {
  if (command_id == TabStripModel::CommandCloseTab) {
    return crostini::GetTerminalSettingPassCtrlW(profile);
  }
  return true;
}
