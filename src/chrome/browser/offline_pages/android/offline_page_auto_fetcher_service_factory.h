// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_ANDROID_OFFLINE_PAGE_AUTO_FETCHER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_OFFLINE_PAGES_ANDROID_OFFLINE_PAGE_AUTO_FETCHER_SERVICE_FACTORY_H_

#include <memory>
#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace offline_pages {
class OfflinePageAutoFetcherService;

// A factory to create one unique OfflinePageAutoFetcherService.
class OfflinePageAutoFetcherServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static OfflinePageAutoFetcherServiceFactory* GetInstance();
  static OfflinePageAutoFetcherService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  class ServiceDelegate;
  friend struct base::DefaultSingletonTraits<
      OfflinePageAutoFetcherServiceFactory>;

  OfflinePageAutoFetcherServiceFactory();
  ~OfflinePageAutoFetcherServiceFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  std::unique_ptr<ServiceDelegate> service_delegate_;
  DISALLOW_COPY_AND_ASSIGN(OfflinePageAutoFetcherServiceFactory);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_ANDROID_OFFLINE_PAGE_AUTO_FETCHER_SERVICE_FACTORY_H_
