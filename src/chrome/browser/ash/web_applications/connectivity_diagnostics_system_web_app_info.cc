// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/connectivity_diagnostics_system_web_app_info.h"

#include <memory>

#include "ash/grit/connectivity_diagnostics_resources.h"
#include "ash/webui/connectivity_diagnostics/url_constants.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

std::unique_ptr<WebApplicationInfo>
CreateWebAppInfoForConnectivityDiagnosticsSystemWebApp() {
  std::unique_ptr<WebApplicationInfo> info =
      std::make_unique<WebApplicationInfo>();
  info->start_url = GURL(ash::kChromeUIConnectivityDiagnosticsUrl);
  info->scope = GURL(ash::kChromeUIConnectivityDiagnosticsUrl);
  info->title = l10n_util::GetStringUTF16(IDS_CONNECTIVITY_DIAGNOSTICS_TITLE);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {{"app_icon_192.png", 192, IDR_CONNECTIVITY_DIAGNOSTICS_APP_ICON_192_PNG},
       {"app_icon_256.png", 256,
        IDR_CONNECTIVITY_DIAGNOSTICS_APP_ICON_256_PNG}},
      *info);
  info->theme_color = 0xFFFFFFFF;
  info->background_color = 0xFFFFFFFF;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = blink::mojom::DisplayMode::kStandalone;

  return info;
}

ConnectivityDiagnosticsSystemAppDelegate::
    ConnectivityDiagnosticsSystemAppDelegate(Profile* profile)
    : web_app::SystemWebAppDelegate(
          web_app::SystemAppType::CONNECTIVITY_DIAGNOSTICS,
          "ConnectivityDiagnostics",
          GURL(ash::kChromeUIConnectivityDiagnosticsUrl),
          profile) {}

std::unique_ptr<WebApplicationInfo>
ConnectivityDiagnosticsSystemAppDelegate::GetWebAppInfo() const {
  return CreateWebAppInfoForConnectivityDiagnosticsSystemWebApp();
}

bool ConnectivityDiagnosticsSystemAppDelegate::ShouldShowInLauncher() const {
  return false;
}

bool ConnectivityDiagnosticsSystemAppDelegate::ShouldShowInSearch() const {
  return false;
}
