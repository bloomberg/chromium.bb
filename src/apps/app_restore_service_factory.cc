// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_restore_service_factory.h"

#include "apps/app_lifetime_monitor_factory.h"
#include "apps/app_restore_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace apps {

// static
AppRestoreService* AppRestoreServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AppRestoreService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

AppRestoreServiceFactory* AppRestoreServiceFactory::GetInstance() {
  return base::Singleton<AppRestoreServiceFactory>::get();
}

AppRestoreServiceFactory::AppRestoreServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "AppRestoreService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(AppLifetimeMonitorFactory::GetInstance());
}

AppRestoreServiceFactory::~AppRestoreServiceFactory() = default;

KeyedService* AppRestoreServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AppRestoreService(context);
}

bool AppRestoreServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace apps
