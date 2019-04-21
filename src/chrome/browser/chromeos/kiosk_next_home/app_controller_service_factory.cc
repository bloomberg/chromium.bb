// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_next_home/app_controller_service_factory.h"

#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/kiosk_next_home/app_controller_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {
namespace kiosk_next_home {

// static
AppControllerService* AppControllerServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AppControllerService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
AppControllerServiceFactory* AppControllerServiceFactory::GetInstance() {
  static base::NoDestructor<AppControllerServiceFactory> factory;
  return factory.get();
}

AppControllerServiceFactory::AppControllerServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "KioskNextAppControllerServiceFactory",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(apps::AppServiceProxyFactory::GetInstance());
  DependsOn(ArcAppListPrefsFactory::GetInstance());
}

AppControllerServiceFactory::~AppControllerServiceFactory() = default;

KeyedService* AppControllerServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AppControllerService(Profile::FromBrowserContext(context));
}

}  // namespace kiosk_next_home
}  // namespace chromeos
