// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_TERMINAL_SYSTEM_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_TERMINAL_SYSTEM_WEB_APP_INFO_H_

#include <memory>

#include "chrome/browser/web_applications/system_web_apps/system_web_app_delegate.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/gfx/geometry/rect.h"

struct WebApplicationInfo;
class Browser;

class TerminalSystemAppDelegate : public web_app::SystemWebAppDelegate {
 public:
  explicit TerminalSystemAppDelegate(Profile* profile);

  // web_app::SystemWebAppDelegate overrides:
  std::unique_ptr<WebApplicationInfo> GetWebAppInfo() const override;
  bool ShouldReuseExistingWindow() const override;
  bool ShouldShowNewWindowMenuOption() const override;
  bool ShouldHaveTabStrip() const override;
  bool HasTitlebarTerminalSelectNewTabButton() const override;
  gfx::Rect GetDefaultBounds(Browser* browser) const override;
  bool HasCustomTabMenuModel() const override;
  std::unique_ptr<ui::SimpleMenuModel> GetTabMenuModel(
      ui::SimpleMenuModel::Delegate* delegate) const override;
  bool ShouldShowTabContextMenuShortcut(Profile* profile,
                                        int command_id) const override;
};

// Returns a WebApplicationInfo used to install the app.
std::unique_ptr<WebApplicationInfo> CreateWebAppInfoForTerminalSystemWebApp();

// Returns the default bounds.
gfx::Rect GetDefaultBoundsForTerminal(Browser* browser);

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_TERMINAL_SYSTEM_WEB_APP_INFO_H_
