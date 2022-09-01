// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/remote_apps/remote_apps_proxy_lacros_factory.h"

#include "chrome/browser/lacros/remote_apps/remote_apps_proxy_lacros.h"
#include "chromeos/components/remote_apps/mojom/remote_apps.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {

// static
RemoteAppsProxyLacros* RemoteAppsProxyLacrosFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<RemoteAppsProxyLacros*>(
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 /* create= */ true));
}

// static
RemoteAppsProxyLacrosFactory* RemoteAppsProxyLacrosFactory::GetInstance() {
  static base::NoDestructor<RemoteAppsProxyLacrosFactory> instance;
  return instance.get();
}

RemoteAppsProxyLacrosFactory::RemoteAppsProxyLacrosFactory()
    : BrowserContextKeyedServiceFactory(
          "RemoteAppsProxyLacros",
          BrowserContextDependencyManager::GetInstance()) {}

RemoteAppsProxyLacrosFactory::~RemoteAppsProxyLacrosFactory() = default;

KeyedService* RemoteAppsProxyLacrosFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  if (!chromeos::LacrosService::Get()
           ->IsAvailable<
               chromeos::remote_apps::mojom::RemoteAppsLacrosBridge>()) {
    return nullptr;
  }

  return new RemoteAppsProxyLacros();
}

bool RemoteAppsProxyLacrosFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

bool RemoteAppsProxyLacrosFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace chromeos
