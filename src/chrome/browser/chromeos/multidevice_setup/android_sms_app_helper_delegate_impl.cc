// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/multidevice_setup/android_sms_app_helper_delegate_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/common/constants.h"
#include "ui/base/window_open_disposition.h"

namespace {
const char kDefaultToPersistCookieName[] = "default_to_persist";
const char kDefaultToPersistCookieValue[] = "true";

network::mojom::CookieManager* GetCookieManager(Profile* profile) {
  content::StoragePartition* partition =
      content::BrowserContext::GetStoragePartitionForSite(
          profile, chromeos::android_sms::GetAndroidMessagesURL());
  return partition->GetCookieManagerForBrowserProcess();
}

}  // namespace

namespace chromeos {

namespace multidevice_setup {

AndroidSmsAppHelperDelegateImpl::AndroidSmsAppHelperDelegateImpl(
    Profile* profile)
    : pending_app_manager_(
          &web_app::WebAppProvider::Get(profile)->pending_app_manager()),
      profile_(profile),
      host_content_settings_map_(
          HostContentSettingsMapFactory::GetForProfile(profile)),
      cookie_manager_(GetCookieManager(profile)),
      weak_ptr_factory_(this) {}

AndroidSmsAppHelperDelegateImpl::AndroidSmsAppHelperDelegateImpl(
    web_app::PendingAppManager* pending_app_manager,
    HostContentSettingsMap* host_content_settings_map,
    network::mojom::CookieManager* cookie_manager)
    : pending_app_manager_(pending_app_manager),
      host_content_settings_map_(host_content_settings_map),
      cookie_manager_(cookie_manager),
      weak_ptr_factory_(this) {}

AndroidSmsAppHelperDelegateImpl::~AndroidSmsAppHelperDelegateImpl() = default;

void AndroidSmsAppHelperDelegateImpl::SetUpAndroidSmsApp(
    bool launch_on_install) {
  PA_LOG(INFO) << "Setting DefaultToPersist Cookie";
  cookie_manager_->SetCanonicalCookie(
      *net::CanonicalCookie::CreateSanitizedCookie(
          chromeos::android_sms::GetAndroidMessagesURL(),
          kDefaultToPersistCookieName, kDefaultToPersistCookieValue,
          std::string() /* domain */, std::string() /* path */,
          base::Time::Now() /* creation_time */,
          base::Time() /* expiration_time */,
          base::Time::Now() /* last_access_time */, true /* secure */,
          false /* http_only */, net::CookieSameSite::STRICT_MODE,
          net::COOKIE_PRIORITY_DEFAULT),
      true /* secure_source */, false /* modify_http_only */,
      base::BindOnce(&AndroidSmsAppHelperDelegateImpl::
                         OnSetDefaultToPersistCookieForInstall,
                     weak_ptr_factory_.GetWeakPtr(), launch_on_install));
}

void AndroidSmsAppHelperDelegateImpl::OnSetDefaultToPersistCookieForInstall(
    bool launch_on_install,
    bool set_cookie_success) {
  if (!set_cookie_success)
    PA_LOG(WARNING) << "Failed to set default to persist cookie";

  // TODO(crbug.com/874605): Consider retries and error handling here. This call
  // can easily fail.
  web_app::PendingAppManager::AppInfo info(
      chromeos::android_sms::GetAndroidMessagesURLWithParams(),
      web_app::LaunchContainer::kWindow, web_app::InstallSource::kInternal);
  info.override_previous_user_uninstall = true;
  // The service worker does not load in time for the installability
  // check so we bypass it as a workaround.
  info.bypass_service_worker_check = true;
  info.require_manifest = true;
  pending_app_manager_->Install(
      std::move(info),
      base::BindOnce(&AndroidSmsAppHelperDelegateImpl::OnAppInstalled,
                     weak_ptr_factory_.GetWeakPtr(), launch_on_install));
}

void AndroidSmsAppHelperDelegateImpl::SetUpAndroidSmsApp() {
  SetUpAndroidSmsApp(false /* launch_on_install */);
}

void AndroidSmsAppHelperDelegateImpl::SetUpAndLaunchAndroidSmsApp() {
  const extensions::Extension* android_sms_pwa =
      extensions::util::GetInstalledPwaForUrl(
          profile_, chromeos::android_sms::GetAndroidMessagesURL());
  if (!android_sms_pwa) {
    PA_LOG(VERBOSE) << "No Messages app found. Installing it.";
    SetUpAndroidSmsApp(true /* launch_on_install */);
    return;
  }

  LaunchAndroidSmsApp();
}

void AndroidSmsAppHelperDelegateImpl::LaunchAndroidSmsApp() {
  const extensions::Extension* android_sms_pwa =
      extensions::util::GetInstalledPwaForUrl(
          profile_, chromeos::android_sms::GetAndroidMessagesURL());
  DCHECK(android_sms_pwa);

  PA_LOG(VERBOSE) << "Messages app Launching...";
  AppLaunchParams params(
      profile_, android_sms_pwa, extensions::LAUNCH_CONTAINER_WINDOW,
      WindowOpenDisposition::NEW_WINDOW, extensions::SOURCE_CHROME_INTERNAL);
  // OpenApplications() is defined in application_launch.h.
  OpenApplication(params);
}

void AndroidSmsAppHelperDelegateImpl::OnAppInstalled(
    bool launch_on_install,
    const GURL& app_url,
    web_app::InstallResultCode code) {
  UMA_HISTOGRAM_ENUMERATION("AndroidSms.PWAInstallationResult", code);

  if (code == web_app::InstallResultCode::kSuccess) {
    // Pre-Grant notification permission for Messages.
    host_content_settings_map_->SetWebsiteSettingDefaultScope(
        chromeos::android_sms::GetAndroidMessagesURL(),
        GURL() /* top_level_url */,
        ContentSettingsType::CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
        content_settings::ResourceIdentifier(),
        std::make_unique<base::Value>(ContentSetting::CONTENT_SETTING_ALLOW));

    PA_LOG(VERBOSE) << "Messages app installed! URL: " << app_url;
    if (launch_on_install)
      LaunchAndroidSmsApp();
  } else {
    PA_LOG(WARNING) << "Messages app failed to install! URL: " << app_url
                    << ", InstallResultCode: " << static_cast<int>(code);
  }
}

void AndroidSmsAppHelperDelegateImpl::TearDownAndroidSmsApp() {
  PA_LOG(INFO) << "Clearing DefaultToPersist Cookie";
  network::mojom::CookieDeletionFilterPtr filter(
      network::mojom::CookieDeletionFilter::New());
  filter->url = chromeos::android_sms::GetAndroidMessagesURL();
  filter->cookie_name = kDefaultToPersistCookieName;
  cookie_manager_->DeleteCookies(std::move(filter), base::DoNothing());
}

}  // namespace multidevice_setup

}  // namespace chromeos
