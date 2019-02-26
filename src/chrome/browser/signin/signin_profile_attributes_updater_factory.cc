// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_profile_attributes_updater_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_profile_attributes_updater.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/core/browser/signin_manager.h"

// static
SigninProfileAttributesUpdater*
SigninProfileAttributesUpdaterFactory::GetForProfile(Profile* profile) {
  return static_cast<SigninProfileAttributesUpdater*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SigninProfileAttributesUpdaterFactory*
SigninProfileAttributesUpdaterFactory::GetInstance() {
  return base::Singleton<SigninProfileAttributesUpdaterFactory>::get();
}

SigninProfileAttributesUpdaterFactory::SigninProfileAttributesUpdaterFactory()
    : BrowserContextKeyedServiceFactory(
          "SigninProfileAttributesUpdater",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(SigninErrorControllerFactory::GetInstance());
  DependsOn(SigninManagerFactory::GetInstance());
}

SigninProfileAttributesUpdaterFactory::
    ~SigninProfileAttributesUpdaterFactory() {}

KeyedService* SigninProfileAttributesUpdaterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new SigninProfileAttributesUpdater(
      SigninManagerFactory::GetForProfile(profile),
      SigninErrorControllerFactory::GetForProfile(profile), profile->GetPath());
}

bool SigninProfileAttributesUpdaterFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}
