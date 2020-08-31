// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/personal_data_manager_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/variations/service/variations_service.h"

namespace autofill {

namespace {

// Return the latest country code from the chrome variation service.
// If the variation service is not available, an empty string is returned.
const std::string GetCountryCodeFromVariations() {
  variations::VariationsService* variation_service =
      g_browser_process->variations_service();

  return variation_service
             ? base::ToUpperASCII(variation_service->GetLatestCountry())
             : std::string();
}
}  // namespace

// static
PersonalDataManager* PersonalDataManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<PersonalDataManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PersonalDataManagerFactory* PersonalDataManagerFactory::GetInstance() {
  return base::Singleton<PersonalDataManagerFactory>::get();
}

PersonalDataManagerFactory::PersonalDataManagerFactory()
    : BrowserContextKeyedServiceFactory(
        "PersonalDataManager",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(WebDataServiceFactory::GetInstance());
}

PersonalDataManagerFactory::~PersonalDataManagerFactory() {
}

KeyedService* PersonalDataManagerFactory::BuildPersonalDataManager(
    autofill::AutofillProfileValidator* autofill_validator,
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  PersonalDataManager* service =
      new PersonalDataManager(g_browser_process->GetApplicationLocale(),
                              GetCountryCodeFromVariations());
  auto local_storage = WebDataServiceFactory::GetAutofillWebDataForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  auto account_storage = WebDataServiceFactory::GetAutofillWebDataForAccount(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  auto* history_service = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  service->Init(local_storage, account_storage, profile->GetPrefs(),
                IdentityManagerFactory::GetForProfile(profile),
                autofill_validator, history_service,
                profile->IsOffTheRecord());
  return service;
}

KeyedService* PersonalDataManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return BuildPersonalDataManager(
      AutofillProfileValidatorFactory::GetInstance(), context);
}

content::BrowserContext* PersonalDataManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace autofill
