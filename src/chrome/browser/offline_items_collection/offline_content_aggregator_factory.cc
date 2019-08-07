// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "content/public/browser/browser_context.h"

#if defined(OS_ANDROID)
static offline_items_collection::OfflineContentAggregator*
    g_offline_content_aggregator = nullptr;
#endif

// static
OfflineContentAggregatorFactory*
OfflineContentAggregatorFactory::GetInstance() {
  return base::Singleton<OfflineContentAggregatorFactory>::get();
}

// static
offline_items_collection::OfflineContentAggregator*
OfflineContentAggregatorFactory::GetForBrowserContext(
    content::BrowserContext* context) {
#if defined(OS_ANDROID)
  if (!g_offline_content_aggregator) {
    g_offline_content_aggregator =
        new offline_items_collection::OfflineContentAggregator();
  }
  return g_offline_content_aggregator;
#else
  return static_cast<offline_items_collection::OfflineContentAggregator*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
#endif
}

OfflineContentAggregatorFactory::OfflineContentAggregatorFactory()
    : BrowserContextKeyedServiceFactory(
          "offline_items_collection::OfflineContentAggregator",
          BrowserContextDependencyManager::GetInstance()) {}

OfflineContentAggregatorFactory::~OfflineContentAggregatorFactory() = default;

KeyedService* OfflineContentAggregatorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(!context->IsOffTheRecord());
  return new offline_items_collection::OfflineContentAggregator();
}

content::BrowserContext*
OfflineContentAggregatorFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}
