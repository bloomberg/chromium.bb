// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/security_token_session_controller_factory.h"

#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/chromeos/login/security_token_session_controller.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {
namespace login {

SecurityTokenSessionControllerFactory::SecurityTokenSessionControllerFactory()
    : BrowserContextKeyedServiceFactory(
          "SecurityTokenSessionController",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(CertificateProviderServiceFactory::GetInstance());
}

SecurityTokenSessionControllerFactory::
    ~SecurityTokenSessionControllerFactory() = default;

// static
SecurityTokenSessionController*
SecurityTokenSessionControllerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<SecurityTokenSessionController*>(
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 /*create=*/true));
}

// static
SecurityTokenSessionControllerFactory*
SecurityTokenSessionControllerFactory::GetInstance() {
  return base::Singleton<SecurityTokenSessionControllerFactory>::get();
}

KeyedService* SecurityTokenSessionControllerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // The service should only exist for the primary profile.
  Profile* profile = Profile::FromBrowserContext(context);
  if (!chromeos::ProfileHelper::IsPrimaryProfile(profile))
    return nullptr;

  return new SecurityTokenSessionController(profile->GetPrefs());
}

content::BrowserContext*
SecurityTokenSessionControllerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool SecurityTokenSessionControllerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace login
}  // namespace chromeos
