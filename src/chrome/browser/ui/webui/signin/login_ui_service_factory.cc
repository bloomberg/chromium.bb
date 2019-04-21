// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

LoginUIServiceFactory::LoginUIServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "LoginUIServiceFactory",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(UnifiedConsentServiceFactory::GetInstance());
}

LoginUIServiceFactory::~LoginUIServiceFactory() {}

// static
LoginUIService* LoginUIServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<LoginUIService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
LoginUIServiceFactory* LoginUIServiceFactory::GetInstance() {
  return base::Singleton<LoginUIServiceFactory>::get();
}

KeyedService* LoginUIServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new LoginUIService(static_cast<Profile*>(profile));
}

bool LoginUIServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}
