// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler_factory.h"

#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "components/app_restore/features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace ash {
namespace app_restore {

// static
AppRestoreArcTaskHandler* AppRestoreArcTaskHandlerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AppRestoreArcTaskHandler*>(
      AppRestoreArcTaskHandlerFactory::GetInstance()
          ->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
AppRestoreArcTaskHandlerFactory*
AppRestoreArcTaskHandlerFactory::GetInstance() {
  return base::Singleton<AppRestoreArcTaskHandlerFactory>::get();
}

AppRestoreArcTaskHandlerFactory::AppRestoreArcTaskHandlerFactory()
    : BrowserContextKeyedServiceFactory(
          "AppRestoreArcTaskHandler",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ArcAppListPrefsFactory::GetInstance());
  DependsOn(apps::AppServiceProxyFactory::GetInstance());
}

AppRestoreArcTaskHandlerFactory::~AppRestoreArcTaskHandlerFactory() = default;

KeyedService* AppRestoreArcTaskHandlerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!::full_restore::features::IsFullRestoreEnabled())
    return nullptr;

  if (!arc::IsArcAllowedForProfile(Profile::FromBrowserContext(context)))
    return nullptr;

  return new AppRestoreArcTaskHandler(Profile::FromBrowserContext(context));
}

}  // namespace app_restore
}  // namespace ash
