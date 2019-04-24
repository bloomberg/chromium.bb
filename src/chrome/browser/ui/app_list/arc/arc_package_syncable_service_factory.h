// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_PACKAGE_SYNCABLE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_PACKAGE_SYNCABLE_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace arc {

class ArcPackageSyncableService;

class ArcPackageSyncableServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static ArcPackageSyncableService* GetForBrowserContext(
      content::BrowserContext* context);

  static ArcPackageSyncableServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ArcPackageSyncableServiceFactory>;

  ArcPackageSyncableServiceFactory();
  ~ArcPackageSyncableServiceFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(ArcPackageSyncableServiceFactory);
};

}  // namespace arc

#endif  // CHROME_BROWSER_UI_APP_LIST_ARC_ARC_PACKAGE_SYNCABLE_SERVICE_FACTORY_H_
