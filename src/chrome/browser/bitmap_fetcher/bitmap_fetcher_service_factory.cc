// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service_factory.h"

#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

/// Factory
BitmapFetcherService* BitmapFetcherServiceFactory::GetForBrowserContext(
    content::BrowserContext* profile) {
  return static_cast<BitmapFetcherService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
BitmapFetcherServiceFactory* BitmapFetcherServiceFactory::GetInstance() {
  return base::Singleton<BitmapFetcherServiceFactory>::get();
}

BitmapFetcherServiceFactory::BitmapFetcherServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "BitmapFetcherService",
          BrowserContextDependencyManager::GetInstance()) {
}

BitmapFetcherServiceFactory::~BitmapFetcherServiceFactory() {
}

KeyedService* BitmapFetcherServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  DCHECK(!profile->IsOffTheRecord());
  return new BitmapFetcherService(profile);
}
