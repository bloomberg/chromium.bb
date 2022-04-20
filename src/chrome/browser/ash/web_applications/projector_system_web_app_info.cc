// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/projector_system_web_app_info.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/grit/ash_projector_app_trusted_resources.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/ui/ash/projector/projector_utils.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_styles.h"

namespace {

SkColor GetBgColor(bool use_dark_mode) {
  return cros_styles::ResolveColor(
      cros_styles::ColorName::kBgColor, use_dark_mode,
      base::FeatureList::IsEnabled(
          ash::features::kSemanticColorsDebugOverride));
}

}  // namespace

ProjectorSystemWebAppDelegate::ProjectorSystemWebAppDelegate(Profile* profile)
    : web_app::SystemWebAppDelegate(web_app::SystemAppType::PROJECTOR,
                                    "Projector",
                                    GURL(ash::kChromeUITrustedProjectorAppUrl),
                                    profile) {}

ProjectorSystemWebAppDelegate::~ProjectorSystemWebAppDelegate() = default;

std::unique_ptr<WebAppInstallInfo>
ProjectorSystemWebAppDelegate::GetWebAppInfo() const {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL(ash::kChromeUITrustedProjectorAppUrl);
  info->scope = GURL(ash::kChromeUITrustedProjectorAppUrl);

  info->title = l10n_util::GetStringUTF16(IDS_PROJECTOR_APP_NAME);

  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {
          {"app_icon_16.png", 16,
           IDR_ASH_PROJECTOR_APP_TRUSTED_ASSETS_ICON_16_PNG},
          {"app_icon_32.png", 32,
           IDR_ASH_PROJECTOR_APP_TRUSTED_ASSETS_ICON_32_PNG},
          {"app_icon_48.png", 48,
           IDR_ASH_PROJECTOR_APP_TRUSTED_ASSETS_ICON_48_PNG},
          {"app_icon_64.png", 64,
           IDR_ASH_PROJECTOR_APP_TRUSTED_ASSETS_ICON_64_PNG},
          {"app_icon_96.png", 96,
           IDR_ASH_PROJECTOR_APP_TRUSTED_ASSETS_ICON_96_PNG},
          {"app_icon_128.png", 128,
           IDR_ASH_PROJECTOR_APP_TRUSTED_ASSETS_ICON_128_PNG},
          {"app_icon_192.png", 192,
           IDR_ASH_PROJECTOR_APP_TRUSTED_ASSETS_ICON_192_PNG},
          {"app_icon_256.png", 256,
           IDR_ASH_PROJECTOR_APP_TRUSTED_ASSETS_ICON_256_PNG},
      },
      *info);

  info->theme_color = GetBgColor(/*use_dark_mode=*/false);
  info->dark_mode_theme_color = GetBgColor(/*use_dark_mode=*/true);
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = blink::mojom::DisplayMode::kStandalone;

  // TODO(b/195127670): Add info.url_handlers for https://projector.apps.chrome
  // domain. Requires web-app-origin-association file at the new domain to prove
  // cross-ownership. See
  // https://web.dev/pwa-url-handler/#the-web-app-origin-association-file.

  return info;
}

bool ProjectorSystemWebAppDelegate::ShouldCaptureNavigations() const {
  return true;
}

gfx::Size ProjectorSystemWebAppDelegate::GetMinimumWindowSize() const {
  // The minimum width matches the minimum width of the Projector viewer left
  // panel defined in the web component.
  return {222, 550};
}

bool ProjectorSystemWebAppDelegate::IsAppEnabled() const {
  return IsProjectorAppEnabled(profile_);
}
