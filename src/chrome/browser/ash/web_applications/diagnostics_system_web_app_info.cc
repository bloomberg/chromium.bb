// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/diagnostics_system_web_app_info.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/grit/ash_diagnostics_app_resources.h"
#include "ash/webui/diagnostics_ui/url_constants.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "url/gurl.h"

std::unique_ptr<WebApplicationInfo>
CreateWebAppInfoForDiagnosticsSystemWebApp() {
  std::unique_ptr<WebApplicationInfo> info =
      std::make_unique<WebApplicationInfo>();
  info->start_url = GURL(ash::kChromeUIDiagnosticsAppUrl);
  info->scope = GURL(ash::kChromeUIDiagnosticsAppUrl);

  // TODO(jimmyxgong): Update the title with finalized i18n copy.
  info->title = u"Diagnostics";
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url, {{"app_icon_192.png", 192, IDR_DIAGNOSTICS_APP_ICON}},
      *info);
  info->theme_color =
      web_app::GetDefaultBackgroundColor(/*use_dark_mode=*/false);
  info->dark_mode_theme_color =
      web_app::GetDefaultBackgroundColor(/*use_dark_mode=*/true);
  info->background_color = info->theme_color;
  info->dark_mode_background_color = info->dark_mode_theme_color;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = blink::mojom::DisplayMode::kStandalone;

  return info;
}

DiagnosticsSystemAppDelegate::DiagnosticsSystemAppDelegate(Profile* profile)
    : web_app::SystemWebAppDelegate(web_app::SystemAppType::DIAGNOSTICS,
                                    "Diagnostics",
                                    GURL("chrome://diagnostics"),
                                    profile) {}

std::unique_ptr<WebApplicationInfo>
DiagnosticsSystemAppDelegate::GetWebAppInfo() const {
  return CreateWebAppInfoForDiagnosticsSystemWebApp();
}

bool DiagnosticsSystemAppDelegate::ShouldShowInLauncher() const {
  return false;
}

gfx::Size DiagnosticsSystemAppDelegate::GetMinimumWindowSize() const {
  return {600, 390};
}

bool DiagnosticsSystemAppDelegate::IsAppEnabled() const {
  return base::FeatureList::IsEnabled(chromeos::features::kDiagnosticsApp);
}
