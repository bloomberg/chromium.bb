// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/query_in_omnibox_factory.h"

#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/query_in_omnibox.h"

// static
QueryInOmnibox* QueryInOmniboxFactory::GetForProfile(Profile* profile) {
  return static_cast<QueryInOmnibox*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
QueryInOmniboxFactory* QueryInOmniboxFactory::GetInstance() {
  return base::Singleton<QueryInOmniboxFactory>::get();
}

// static
std::unique_ptr<KeyedService> QueryInOmniboxFactory::BuildInstanceFor(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  return std::make_unique<QueryInOmnibox>(
      AutocompleteClassifierFactory::GetForProfile(profile),
      TemplateURLServiceFactory::GetForProfile(profile));
}

QueryInOmniboxFactory::QueryInOmniboxFactory()
    : BrowserContextKeyedServiceFactory(
          "QueryInOmnibox",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(AutocompleteClassifierFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

QueryInOmniboxFactory::~QueryInOmniboxFactory() {}

content::BrowserContext* QueryInOmniboxFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool QueryInOmniboxFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

KeyedService* QueryInOmniboxFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return BuildInstanceFor(static_cast<Profile*>(profile)).release();
}
