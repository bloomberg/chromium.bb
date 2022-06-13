// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/account_manager/account_manager_policy_controller_factory.h"

#include "ash/components/account_manager/account_manager_factory.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/account_manager/account_manager_policy_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace ash {

// static
AccountManagerPolicyController*
AccountManagerPolicyControllerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AccountManagerPolicyController*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
AccountManagerPolicyControllerFactory*
AccountManagerPolicyControllerFactory::GetInstance() {
  static base::NoDestructor<AccountManagerPolicyControllerFactory> instance;
  return instance.get();
}

AccountManagerPolicyControllerFactory::AccountManagerPolicyControllerFactory()
    : BrowserContextKeyedServiceFactory(
          "AccountManagerPolicyController",
          BrowserContextDependencyManager::GetInstance()) {}

AccountManagerPolicyControllerFactory::
    ~AccountManagerPolicyControllerFactory() = default;

KeyedService* AccountManagerPolicyControllerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* const profile = Profile::FromBrowserContext(context);
  auto* factory =
      g_browser_process->platform_part()->GetAccountManagerFactory();
  auto* account_manager =
      factory->GetAccountManager(profile->GetPath().value());

  if (!account_manager)
    return nullptr;

  auto* account_manager_facade =
      ::GetAccountManagerFacade(profile->GetPath().value());

  if (!account_manager_facade)
    return nullptr;

  user_manager::User* const user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return nullptr;

  AccountManagerPolicyController* const service =
      new AccountManagerPolicyController(profile, account_manager,
                                         account_manager_facade,
                                         user->GetAccountId());
  // Auto-start the Service.
  service->Start();

  return service;
}

}  // namespace ash
