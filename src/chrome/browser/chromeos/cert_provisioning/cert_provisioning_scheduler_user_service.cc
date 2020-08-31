// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_scheduler_user_service.h"

#include "base/no_destructor.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {
namespace cert_provisioning {

// ================== CertProvisioningSchedulerUserService =====================

CertProvisioningSchedulerUserService::CertProvisioningSchedulerUserService(
    Profile* profile)
    : scheduler_(CertProvisioningScheduler::CreateUserCertProvisioningScheduler(
          profile)) {}

CertProvisioningSchedulerUserService::~CertProvisioningSchedulerUserService() =
    default;

// ================ CertProvisioningSchedulerUserServiceFactory ================

// static
CertProvisioningSchedulerUserService*
CertProvisioningSchedulerUserServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<CertProvisioningSchedulerUserService*>(
      CertProvisioningSchedulerUserServiceFactory::GetInstance()
          ->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
CertProvisioningSchedulerUserServiceFactory*
CertProvisioningSchedulerUserServiceFactory::GetInstance() {
  static base::NoDestructor<CertProvisioningSchedulerUserServiceFactory>
      factory;
  return factory.get();
}

CertProvisioningSchedulerUserServiceFactory::
    CertProvisioningSchedulerUserServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "CertProvisioningSchedulerUserService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(chromeos::platform_keys::PlatformKeysServiceFactory::GetInstance());
  DependsOn(invalidation::ProfileInvalidationProviderFactory::GetInstance());
}

bool CertProvisioningSchedulerUserServiceFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

KeyedService*
CertProvisioningSchedulerUserServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile || !ProfileHelper::IsRegularProfile(profile) ||
      !profile->GetProfilePolicyConnector()->IsManaged()) {
    return nullptr;
  }

  return new CertProvisioningSchedulerUserService(profile);
}

}  // namespace cert_provisioning
}  // namespace chromeos
