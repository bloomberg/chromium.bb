// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_auto_fetcher_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/offline_pages/offline_page_auto_fetcher_service.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace offline_pages {

// static
OfflinePageAutoFetcherServiceFactory*
OfflinePageAutoFetcherServiceFactory::GetInstance() {
  return base::Singleton<OfflinePageAutoFetcherServiceFactory>::get();
}

// static
OfflinePageAutoFetcherService*
OfflinePageAutoFetcherServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  KeyedService* service =
      GetInstance()->GetServiceForBrowserContext(context, true);
  if (!service)
    return nullptr;
  return static_cast<OfflinePageAutoFetcherService*>(service);
}

OfflinePageAutoFetcherServiceFactory::OfflinePageAutoFetcherServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "OfflinePageAutoFetcherService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(RequestCoordinatorFactory::GetInstance());
}

OfflinePageAutoFetcherServiceFactory::~OfflinePageAutoFetcherServiceFactory() {}

KeyedService* OfflinePageAutoFetcherServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  RequestCoordinator* coordinator =
      RequestCoordinatorFactory::GetForBrowserContext(context);
  return new OfflinePageAutoFetcherService(coordinator);
}

}  // namespace offline_pages
