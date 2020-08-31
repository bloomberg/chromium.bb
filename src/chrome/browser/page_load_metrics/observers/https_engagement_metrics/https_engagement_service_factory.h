// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_HTTPS_ENGAGEMENT_METRICS_HTTPS_ENGAGEMENT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_HTTPS_ENGAGEMENT_METRICS_HTTPS_ENGAGEMENT_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

class HttpsEngagementService;

// Singleton that owns all HttpsEngagementKeyServices and associates them with
// BrowserContexts.
class HttpsEngagementServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static HttpsEngagementService* GetForBrowserContext(
      content::BrowserContext* context);
  static HttpsEngagementServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<HttpsEngagementServiceFactory>;

  HttpsEngagementServiceFactory();
  ~HttpsEngagementServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;

  DISALLOW_COPY_AND_ASSIGN(HttpsEngagementServiceFactory);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_HTTPS_ENGAGEMENT_METRICS_HTTPS_ENGAGEMENT_SERVICE_FACTORY_H_
