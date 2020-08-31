// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/apps/apk_web_app_service_factory.h"

#include "chrome/browser/chromeos/apps/apk_web_app_service.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {

// static
ApkWebAppService* ApkWebAppServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<ApkWebAppService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ApkWebAppServiceFactory* ApkWebAppServiceFactory::GetInstance() {
  return base::Singleton<ApkWebAppServiceFactory>::get();
}

ApkWebAppServiceFactory::ApkWebAppServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ApkWebAppService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ArcAppListPrefsFactory::GetInstance());
  DependsOn(web_app::WebAppProviderFactory::GetInstance());
}

ApkWebAppServiceFactory::~ApkWebAppServiceFactory() {}

bool ApkWebAppServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

KeyedService* ApkWebAppServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  if (!arc::IsArcAllowedForProfile(profile))
    return nullptr;

  return new ApkWebAppService(profile);
}

content::BrowserContext* ApkWebAppServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Mirrors ArcAppListPrefs.
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace chromeos
