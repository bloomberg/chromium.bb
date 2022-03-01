// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/nss_service_factory.h"

#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/net/nss_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/cert_db_initializer_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

NssService* NssServiceFactory::GetForContext(
    content::BrowserContext* browser_context) {
  return static_cast<NssService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

NssServiceFactory::NssServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "NssServiceFactory",
          BrowserContextDependencyManager::GetInstance()) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  DependsOn(CertDbInitializerFactory::GetInstance());
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

NssServiceFactory::~NssServiceFactory() = default;

NssServiceFactory* NssServiceFactory::GetInstance() {
  static base::NoDestructor<NssServiceFactory> instance;
  return instance.get();
}

KeyedService* NssServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new NssService(context);
}

content::BrowserContext* NssServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Create separate service for incognito profiles.
  return context;
}
