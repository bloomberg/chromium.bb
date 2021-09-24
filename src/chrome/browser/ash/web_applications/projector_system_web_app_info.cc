// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/projector_system_web_app_info.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/projector_app/projector_app_constants.h"
#include "chromeos/grit/chromeos_projector_app_trusted_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"

ProjectorSystemWebAppDelegate::ProjectorSystemWebAppDelegate(Profile* profile)
    : web_app::SystemWebAppDelegate(
          web_app::SystemAppType::PROJECTOR,
          "Projector",
          GURL(chromeos::kChromeUITrustedProjectorAppUrl),
          profile) {}

ProjectorSystemWebAppDelegate::~ProjectorSystemWebAppDelegate() = default;

std::unique_ptr<WebApplicationInfo>
ProjectorSystemWebAppDelegate::GetWebAppInfo() const {
  auto info = std::make_unique<WebApplicationInfo>();
  info->start_url = GURL(chromeos::kChromeUITrustedProjectorAppUrl);
  info->scope = GURL(chromeos::kChromeUITrustedProjectorAppUrl);

  info->title = l10n_util::GetStringUTF16(IDS_PROJECTOR_APP_NAME);

  // TODO(b/195127670): Add 48, 128, and 192 size icons through
  // CreateIconInfoForSystemWebApp().

  // TODO(b/195127670): Figure out the theme color.
  info->theme_color = SK_ColorBLACK;
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

bool ProjectorSystemWebAppDelegate::IsAppEnabled() const {
  return ash::features::IsProjectorEnabled();
}
