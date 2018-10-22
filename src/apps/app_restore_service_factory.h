// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_RESTORE_SERVICE_FACTORY_H_
#define APPS_APP_RESTORE_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace apps {

class AppRestoreService;

// Singleton that owns all AppRestoreServices and associates them with
// BrowserContexts. Listens for the BrowserContext's destruction notification
// and cleans up
// the associated AppRestoreService.
class AppRestoreServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static AppRestoreService* GetForBrowserContext(
      content::BrowserContext* context);

  static AppRestoreServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<AppRestoreServiceFactory>;

  AppRestoreServiceFactory();
  ~AppRestoreServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace apps

#endif  // APPS_APP_RESTORE_SERVICE_FACTORY_H_
