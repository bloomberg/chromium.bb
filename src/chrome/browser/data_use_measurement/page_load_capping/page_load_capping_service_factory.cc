// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_use_measurement/page_load_capping/page_load_capping_service_factory.h"

#include "chrome/browser/data_use_measurement/page_load_capping/page_load_capping_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace {

base::LazyInstance<PageLoadCappingServiceFactory>::DestructorAtExit
    g_previews_factory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
PageLoadCappingService* PageLoadCappingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PageLoadCappingService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
PageLoadCappingServiceFactory* PageLoadCappingServiceFactory::GetInstance() {
  return g_previews_factory.Pointer();
}

PageLoadCappingServiceFactory::PageLoadCappingServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PageLoadCappingService",
          BrowserContextDependencyManager::GetInstance()) {}

PageLoadCappingServiceFactory::~PageLoadCappingServiceFactory() {}

KeyedService* PageLoadCappingServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new PageLoadCappingService();
}
