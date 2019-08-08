// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_manager_desktop.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#endif  // defined(OS_CHROMEOS)

namespace {

// Platform values defined in:
// https://github.com/w3c/manifest/wiki/Platforms
const char kPlatformChromeWebStore[] = "chrome_web_store";

#if defined(OS_CHROMEOS)
const char kPlatformPlay[] = "play";
#endif  // defined(OS_CHROMEOS)

bool gDisableTriggeringForTesting = false;

}  // namespace

namespace banners {

// static
AppBannerManager* AppBannerManager::FromWebContents(
    content::WebContents* web_contents) {
  return AppBannerManagerDesktop::FromWebContents(web_contents);
}

void AppBannerManagerDesktop::DisableTriggeringForTesting() {
  gDisableTriggeringForTesting = true;
}

AppBannerManagerDesktop::AppBannerManagerDesktop(
    content::WebContents* web_contents)
    : AppBannerManager(web_contents),
      extension_registry_(extensions::ExtensionRegistry::Get(
          web_contents->GetBrowserContext())) {}

AppBannerManagerDesktop::~AppBannerManagerDesktop() { }

base::WeakPtr<AppBannerManager> AppBannerManagerDesktop::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AppBannerManagerDesktop::InvalidateWeakPtrs() {
  weak_factory_.InvalidateWeakPtrs();
}

bool AppBannerManagerDesktop::IsSupportedAppPlatform(
    const base::string16& platform) const {
  if (base::EqualsASCII(platform, kPlatformChromeWebStore))
    return true;

#if defined(OS_CHROMEOS)
  if (base::EqualsASCII(platform, kPlatformPlay) &&
      arc::IsArcAllowedForProfile(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()))) {
    return true;
  }
#endif  // defined(OS_CHROMEOS)

  return false;
}

bool AppBannerManagerDesktop::IsRelatedAppInstalled(
    const blink::Manifest::RelatedApplication& related_app) const {
  std::string id = base::UTF16ToUTF8(related_app.id.string());
  if (id.empty())
    return false;

  const base::string16& platform = related_app.platform.string();

  if (base::EqualsASCII(platform, kPlatformChromeWebStore)) {
    return extension_registry_->GetExtensionById(
               id, extensions::ExtensionRegistry::ENABLED) != nullptr;
  }

#if defined(OS_CHROMEOS)
  if (base::EqualsASCII(platform, kPlatformPlay)) {
    ArcAppListPrefs* arc_app_list_prefs =
        ArcAppListPrefs::Get(web_contents()->GetBrowserContext());
    return arc_app_list_prefs && arc_app_list_prefs->GetPackage(id) != nullptr;
  }
#endif  // defined(OS_CHROMEOS)

  return false;
}

bool AppBannerManagerDesktop::IsWebAppConsideredInstalled(
    content::WebContents* web_contents,
    const GURL& validated_url,
    const GURL& start_url,
    const GURL& manifest_url) {
  return web_app::WebAppProvider::Get(
             Profile::FromBrowserContext(web_contents->GetBrowserContext()))
      ->registrar()
      .IsInstalled(start_url);
}

void AppBannerManagerDesktop::ShowBannerUi(WebappInstallSource install_source) {
  RecordDidShowBanner("AppBanner.WebApp.Shown");
  TrackDisplayEvent(DISPLAY_EVENT_WEB_APP_BANNER_CREATED);
  TrackUserResponse(USER_RESPONSE_WEB_APP_ACCEPTED);
  ReportStatus(SHOWING_APP_INSTALLATION_DIALOG);
  CreateWebApp(install_source);
}

void AppBannerManagerDesktop::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (gDisableTriggeringForTesting)
    return;

  AppBannerManager::DidFinishLoad(render_frame_host, validated_url);
}

void AppBannerManagerDesktop::OnEngagementEvent(
    content::WebContents* web_contents,
    const GURL& url,
    double score,
    SiteEngagementService::EngagementType type) {
  if (gDisableTriggeringForTesting)
    return;

  AppBannerManager::OnEngagementEvent(web_contents, url, score, type);
}

void AppBannerManagerDesktop::CreateWebApp(WebappInstallSource install_source) {
  content::WebContents* contents = web_contents();
  DCHECK(contents);

  // TODO(loyso): Take appropriate action if WebApps disabled for profile.
  web_app::CreateWebAppFromManifest(
      contents, install_source,
      base::BindOnce(&AppBannerManagerDesktop::DidFinishCreatingWebApp,
                     weak_factory_.GetWeakPtr()));
}

void AppBannerManagerDesktop::DidFinishCreatingWebApp(
    const web_app::AppId& app_id,
    web_app::InstallResultCode code) {
  content::WebContents* contents = web_contents();
  if (!contents)
    return;

  // BookmarkAppInstallManager returns kFailedUnknownReason for any error.
  // We can't distinguish kUserInstallDeclined case so far.
  // If kFailedUnknownReason, we assume that the confirmation dialog was
  // cancelled. Alternatively, the web app installation may have failed, but
  // we can't tell the difference here.
  // TODO(crbug.com/789381): plumb through enough information to be able to
  // distinguish between extension install failures and user-cancellations of
  // the app install dialog.
  if (code != web_app::InstallResultCode::kSuccess) {
    SendBannerDismissed();
    TrackUserResponse(USER_RESPONSE_WEB_APP_DISMISSED);
    AppBannerSettingsHelper::RecordBannerDismissEvent(
        contents, GetAppIdentifier(), AppBannerSettingsHelper::WEB);
    return;
  }

  SendBannerAccepted();
  AppBannerSettingsHelper::RecordBannerInstallEvent(
      contents, GetAppIdentifier(), AppBannerSettingsHelper::WEB);

  // OnInstall must be called last since it resets Mojo bindings.
  OnInstall(false /* is_native app */, blink::kWebDisplayModeStandalone);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AppBannerManagerDesktop)

}  // namespace banners
