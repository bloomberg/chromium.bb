// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/help_app_ui/help_app_manager_factory.h"
#include "chromeos/components/help_app_ui/help_app_manager.h"
#include "chromeos/components/local_search_service/public/cpp/local_search_service_proxy_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {
namespace help_app {

// static
HelpAppManager* HelpAppManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<HelpAppManager*>(
      HelpAppManagerFactory::GetInstance()->GetServiceForBrowserContext(
          context, /*create=*/true));
}

// static
HelpAppManagerFactory* HelpAppManagerFactory::GetInstance() {
  return base::Singleton<HelpAppManagerFactory>::get();
}

HelpAppManagerFactory::HelpAppManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "HelpAppManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(
      local_search_service::LocalSearchServiceProxyFactory::GetInstance());
}

HelpAppManagerFactory::~HelpAppManagerFactory() = default;

content::BrowserContext* HelpAppManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // The service should exist in incognito mode.
  return context;
}

KeyedService* HelpAppManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new HelpAppManager(
      local_search_service::LocalSearchServiceProxyFactory::
          GetForBrowserContext(context));
}

bool HelpAppManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace help_app
}  // namespace chromeos
