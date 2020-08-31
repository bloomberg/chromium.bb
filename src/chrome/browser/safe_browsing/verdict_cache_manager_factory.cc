// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/verdict_cache_manager_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/safe_browsing/core/verdict_cache_manager.h"
#include "content/public/browser/browser_context.h"

namespace safe_browsing {

// static
VerdictCacheManager* VerdictCacheManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<VerdictCacheManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, /* create= */ true));
}

// static
VerdictCacheManagerFactory* VerdictCacheManagerFactory::GetInstance() {
  return base::Singleton<VerdictCacheManagerFactory>::get();
}

VerdictCacheManagerFactory::VerdictCacheManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "VerdictCacheManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

KeyedService* VerdictCacheManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new VerdictCacheManager(
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      HostContentSettingsMapFactory::GetForProfile(profile));
}

content::BrowserContext* VerdictCacheManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace safe_browsing
