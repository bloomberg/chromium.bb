// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/sample_system_web_app_info.h"

#include <memory>

#include "ash/grit/ash_sample_system_web_app_resources.h"
#include "ash/webui/sample_system_web_app_ui/url_constants.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"

std::unique_ptr<WebApplicationInfo> CreateWebAppInfoForSampleSystemWebApp() {
  std::unique_ptr<WebApplicationInfo> info =
      std::make_unique<WebApplicationInfo>();
  info->start_url = GURL(ash::kChromeUISampleSystemWebAppURL);
  info->scope = GURL(ash::kChromeUISampleSystemWebAppURL);
  // |title| should come from a resource string, but this is the sample app, and
  // doesn't have one.
  info->title = u"Sample System Web App";
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {{"app_icon_192.png", 192,
        IDR_ASH_SAMPLE_SYSTEM_WEB_APP_APP_ICON_192_PNG}},
      *info);
  info->theme_color = 0xFF4285F4;
  info->background_color = 0xFFFFFFFF;
  // Bright green in dark mode to be able to see it flicker.
  // This should match up with the dark theme background color to prevent
  // flickering. But for the sample app, we use different, garish colors
  // to make sure we can see it working.
  info->dark_mode_theme_color = 0xFF11ff00;
  info->dark_mode_background_color = 0xFFff8888;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = blink::mojom::DisplayMode::kStandalone;

  {
    WebAppShortcutsMenuItemInfo shortcut;
    shortcut.name = u"Inter Frame Communication Demo";
    shortcut.url =
        GURL("chrome://sample-system-web-app/inter_frame_communication.html");
    info->shortcuts_menu_item_infos.push_back(std::move(shortcut));
  }
  {
    WebAppShortcutsMenuItemInfo shortcut;
    shortcut.name = u"Component Playground";
    shortcut.url =
        GURL("chrome://sample-system-web-app/component_playground.html");
    info->shortcuts_menu_item_infos.push_back(std::move(shortcut));
  }

  return info;
}

SampleSystemAppDelegate::SampleSystemAppDelegate(Profile* profile)
    : web_app::SystemWebAppDelegate(
          web_app::SystemAppType::SAMPLE,
          "Sample",
          GURL("chrome://sample-system-web-app/pwa.html"),
          profile,
          web_app::OriginTrialsMap(
              {{web_app::GetOrigin("chrome://sample-system-web-app"),
                {"Frobulate"}},
               {web_app::GetOrigin("chrome-untrusted://sample-system-web-app"),
                {"Frobulate"}}})) {}

std::unique_ptr<WebApplicationInfo> SampleSystemAppDelegate::GetWebAppInfo()
    const {
  return CreateWebAppInfoForSampleSystemWebApp();
}

bool SampleSystemAppDelegate::ShouldCaptureNavigations() const {
  return true;
}

bool SampleSystemAppDelegate::ShouldShowNewWindowMenuOption() const {
  return true;
}

bool SampleSystemAppDelegate::ShouldReuseExistingWindow() const {
  return false;
}

absl::optional<web_app::SystemAppBackgroundTaskInfo>
SampleSystemAppDelegate::GetTimerInfo() const {
  return web_app::SystemAppBackgroundTaskInfo(
      base::Seconds(30), GURL("chrome://sample-system-web-app/timer.html"));
}
