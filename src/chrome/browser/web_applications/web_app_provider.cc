// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_provider.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/bookmark_apps/external_web_apps.h"
#include "chrome/browser/web_applications/bookmark_apps/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/bookmark_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/extensions/pending_bookmark_app_manager.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_ids_map.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "content/public/browser/notification_source.h"

namespace web_app {

// static
WebAppProvider* WebAppProvider::Get(Profile* profile) {
  return WebAppProviderFactory::GetForProfile(profile);
}

WebAppProvider::WebAppProvider(Profile* profile)
    : pending_app_manager_(
          std::make_unique<extensions::PendingBookmarkAppManager>(profile)) {
  if (WebAppPolicyManager::ShouldEnableForProfile(profile)) {
    web_app_policy_manager_ = std::make_unique<WebAppPolicyManager>(
        profile, pending_app_manager_.get());
  }

  system_web_app_manager_ = std::make_unique<SystemWebAppManager>(
      profile, pending_app_manager_.get());

  notification_registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                              content::Source<Profile>(profile));

  web_app::ScanForExternalWebApps(
      profile, base::BindOnce(&WebAppProvider::OnScanForExternalWebApps,
                              weak_ptr_factory_.GetWeakPtr()));
}

WebAppProvider::~WebAppProvider() = default;

// static
void WebAppProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  ExtensionIdsMap::RegisterProfilePrefs(registry);
  WebAppPolicyManager::RegisterProfilePrefs(registry);
}

void WebAppProvider::Reset() {
  // PendingAppManager is used by WebAppPolicyManager and therefore should be
  // deleted after it.
  web_app_policy_manager_.reset();
  system_web_app_manager_.reset();
  pending_app_manager_.reset();
}

void WebAppProvider::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& detals) {
  DCHECK_EQ(chrome::NOTIFICATION_PROFILE_DESTROYED, type);

  // KeyedService::Shutdown() gets called when the profile is being destroyed,
  // but after DCHECK'ing that no RenderProcessHosts are being leaked. The
  // "chrome::NOTIFICATION_PROFILE_DESTROYED" notification gets sent before the
  // DCHECK so we use that to clean up RenderProcessHosts instead.
  Reset();
}

void WebAppProvider::OnScanForExternalWebApps(
    std::vector<web_app::PendingAppManager::AppInfo> app_infos) {
  pending_app_manager_->SynchronizeInstalledApps(
      std::move(app_infos), InstallSource::kExternalDefault);
}

}  // namespace web_app
