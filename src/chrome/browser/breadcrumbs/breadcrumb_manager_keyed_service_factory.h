// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BREADCRUMBS_BREADCRUMB_MANAGER_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_BREADCRUMBS_BREADCRUMB_MANAGER_KEYED_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace breadcrumbs {
class BreadcrumbManagerKeyedService;
}  // namespace breadcrumbs

namespace content {
class BrowserContext;
}  // namespace content

class BreadcrumbManagerKeyedServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static BreadcrumbManagerKeyedServiceFactory* GetInstance();
  static breadcrumbs::BreadcrumbManagerKeyedService* GetForBrowserContext(
      content::BrowserContext* context);

  BreadcrumbManagerKeyedServiceFactory(
      const BreadcrumbManagerKeyedServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<BreadcrumbManagerKeyedServiceFactory>;

  BreadcrumbManagerKeyedServiceFactory();
  ~BreadcrumbManagerKeyedServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_BREADCRUMBS_BREADCRUMB_MANAGER_KEYED_SERVICE_FACTORY_H_
