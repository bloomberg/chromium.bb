// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/bulk_leak_check_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/bulk_leak_check_service.h"
#include "content/public/browser/storage_partition.h"

BulkLeakCheckServiceFactory::BulkLeakCheckServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PasswordBulkLeakCheck",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

BulkLeakCheckServiceFactory::~BulkLeakCheckServiceFactory() = default;

// static
BulkLeakCheckServiceFactory* BulkLeakCheckServiceFactory::GetInstance() {
  static base::NoDestructor<BulkLeakCheckServiceFactory> instance;
  return instance.get();
}

// static
password_manager::BulkLeakCheckService*
BulkLeakCheckServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<password_manager::BulkLeakCheckService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

KeyedService* BulkLeakCheckServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new password_manager::BulkLeakCheckService(
      IdentityManagerFactory::GetForProfile(profile),
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess());
}
