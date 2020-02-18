// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"

#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/predictor_database_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace predictors {

// static
AutocompleteActionPredictor* AutocompleteActionPredictorFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AutocompleteActionPredictor*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AutocompleteActionPredictorFactory*
    AutocompleteActionPredictorFactory::GetInstance() {
  return base::Singleton<AutocompleteActionPredictorFactory>::get();
}

AutocompleteActionPredictorFactory::AutocompleteActionPredictorFactory()
    : BrowserContextKeyedServiceFactory(
        "AutocompleteActionPredictor",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(PredictorDatabaseFactory::GetInstance());
}

AutocompleteActionPredictorFactory::~AutocompleteActionPredictorFactory() {}

content::BrowserContext*
AutocompleteActionPredictorFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

KeyedService* AutocompleteActionPredictorFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new AutocompleteActionPredictor(static_cast<Profile*>(profile));
}

}  // namespace predictors
