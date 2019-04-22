// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/tab_restore_service_factory.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/chrome_tab_restore_service_client.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sessions/core/tab_restore_service_impl.h"

// static
sessions::TabRestoreService* TabRestoreServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<sessions::TabRestoreService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
sessions::TabRestoreService* TabRestoreServiceFactory::GetForProfileIfExisting(
    Profile* profile) {
  return static_cast<sessions::TabRestoreService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
void TabRestoreServiceFactory::ResetForProfile(Profile* profile) {
  TabRestoreServiceFactory* factory = GetInstance();
  factory->BrowserContextShutdown(profile);
  factory->BrowserContextDestroyed(profile);
}

TabRestoreServiceFactory* TabRestoreServiceFactory::GetInstance() {
  return base::Singleton<TabRestoreServiceFactory>::get();
}

TabRestoreServiceFactory::TabRestoreServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "sessions::TabRestoreService",
          BrowserContextDependencyManager::GetInstance()) {}

TabRestoreServiceFactory::~TabRestoreServiceFactory() {
}

bool TabRestoreServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

KeyedService* TabRestoreServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(!profile->IsOffTheRecord());
  std::unique_ptr<sessions::TabRestoreServiceClient> client(
      new ChromeTabRestoreServiceClient(profile));

  return new sessions::TabRestoreServiceImpl(std::move(client),
                                             profile->GetPrefs(), nullptr);
}
