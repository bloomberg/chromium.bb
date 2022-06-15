// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_REMOTE_APPS_REMOTE_APPS_PROXY_LACROS_FACTORY_H_
#define CHROME_BROWSER_LACROS_REMOTE_APPS_REMOTE_APPS_PROXY_LACROS_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace chromeos {

class RemoteAppsProxyLacros;

// Factory for the `RemoteAppsProxyLacros` KeyedService.
class RemoteAppsProxyLacrosFactory : public BrowserContextKeyedServiceFactory {
 public:
  static RemoteAppsProxyLacros* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static RemoteAppsProxyLacrosFactory* GetInstance();

  RemoteAppsProxyLacrosFactory(const RemoteAppsProxyLacrosFactory&) = delete;
  RemoteAppsProxyLacrosFactory& operator=(const RemoteAppsProxyLacrosFactory&) =
      delete;

 private:
  friend class base::NoDestructor<RemoteAppsProxyLacrosFactory>;

  RemoteAppsProxyLacrosFactory();
  ~RemoteAppsProxyLacrosFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const override;
  bool ServiceIsNULLWhileTesting() const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_LACROS_REMOTE_APPS_REMOTE_APPS_PROXY_LACROS_FACTORY_H_
