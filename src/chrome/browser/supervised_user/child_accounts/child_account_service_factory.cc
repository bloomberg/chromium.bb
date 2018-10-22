// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
ChildAccountService* ChildAccountServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ChildAccountService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ChildAccountServiceFactory* ChildAccountServiceFactory::GetInstance() {
  return base::Singleton<ChildAccountServiceFactory>::get();
}

ChildAccountServiceFactory::ChildAccountServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "ChildAccountService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(AccountTrackerServiceFactory::GetInstance());
  DependsOn(SigninManagerFactory::GetInstance());
  DependsOn(SupervisedUserServiceFactory::GetInstance());
}

ChildAccountServiceFactory::~ChildAccountServiceFactory() {}

KeyedService* ChildAccountServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new ChildAccountService(static_cast<Profile*>(profile));
}
