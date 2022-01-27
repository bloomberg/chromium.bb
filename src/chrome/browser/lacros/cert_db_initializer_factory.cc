// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/cert_db_initializer_factory.h"

#include "base/system/sys_info.h"
#include "chrome/browser/lacros/cert_db_initializer_impl.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

class CertDbInitializer;

// static
CertDbInitializerFactory* CertDbInitializerFactory::GetInstance() {
  static base::NoDestructor<CertDbInitializerFactory> factory;
  return factory.get();
}

// static
CertDbInitializer* CertDbInitializerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<CertDbInitializerImpl*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/false));
}

CertDbInitializerFactory::CertDbInitializerFactory()
    : BrowserContextKeyedServiceFactory(
          "CertDbInitializerFactory",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

bool CertDbInitializerFactory::ServiceIsCreatedWithBrowserContext() const {
  return should_create_with_browser_context_;
}

void CertDbInitializerFactory::SetCreateWithBrowserContextForTesting(
    bool should_create) {
  should_create_with_browser_context_ = should_create;
}

KeyedService* CertDbInitializerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  CertDbInitializerImpl* result = new CertDbInitializerImpl(profile);
  result->Start();
  return result;
}

bool CertDbInitializerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

content::BrowserContext* CertDbInitializerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
