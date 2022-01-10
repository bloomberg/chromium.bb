// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/shortcut_customization_system_web_app_info.h"

#include <memory>

#include "ash/grit/ash_shortcut_customization_app_resources.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/webui/shortcut_customization_ui/url_constants.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"

// TODO(jimmyxgong): Update to correct icon and app sizes.
std::unique_ptr<WebApplicationInfo>
CreateWebAppInfoForShortcutCustomizationSystemWebApp() {
  auto info = std::make_unique<WebApplicationInfo>();
  info->start_url = GURL(ash::kChromeUIShortcutCustomizationAppURL);
  info->scope = GURL(ash::kChromeUIShortcutCustomizationAppURL);
  info->title =
      l10n_util::GetStringUTF16(IDS_ASH_SHORTCUT_CUSTOMIZATION_APP_TITLE);
  info->theme_color =
      web_app::GetDefaultBackgroundColor(/*use_dark_mode=*/false);
  info->dark_mode_theme_color =
      web_app::GetDefaultBackgroundColor(/*use_dark_mode=*/true);
  info->background_color = info->theme_color;
  info->dark_mode_background_color = info->dark_mode_theme_color;
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {{"app_icon_192.png", 192,
        IDR_ASH_SHORTCUT_CUSTOMIZATION_APP_APP_ICON_192_PNG}},
      *info);
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = blink::mojom::DisplayMode::kStandalone;

  return info;
}

ShortcutCustomizationSystemAppDelegate::ShortcutCustomizationSystemAppDelegate(
    Profile* profile)
    : web_app::SystemWebAppDelegate(
          web_app::SystemAppType::SHORTCUT_CUSTOMIZATION,
          "ShortcutCustomization",
          GURL(ash::kChromeUIShortcutCustomizationAppURL),
          profile) {}

std::unique_ptr<WebApplicationInfo>
ShortcutCustomizationSystemAppDelegate::GetWebAppInfo() const {
  return CreateWebAppInfoForShortcutCustomizationSystemWebApp();
}

bool ShortcutCustomizationSystemAppDelegate::IsAppEnabled() const {
  return features::IsShortcutCustomizationAppEnabled();
}

gfx::Size ShortcutCustomizationSystemAppDelegate::GetMinimumWindowSize() const {
  return {600, 600};
}
