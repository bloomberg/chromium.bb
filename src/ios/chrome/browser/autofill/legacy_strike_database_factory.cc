// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/autofill/legacy_strike_database_factory.h"

#include <utility>

#include "base/no_destructor.h"
#include "components/autofill/core/browser/legacy_strike_database.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

namespace autofill {

// static
LegacyStrikeDatabase* LegacyStrikeDatabaseFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<LegacyStrikeDatabase*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
LegacyStrikeDatabaseFactory* LegacyStrikeDatabaseFactory::GetInstance() {
  static base::NoDestructor<LegacyStrikeDatabaseFactory> instance;
  return instance.get();
}

LegacyStrikeDatabaseFactory::LegacyStrikeDatabaseFactory()
    : BrowserStateKeyedServiceFactory(
          "AutofillLegacyStrikeDatabase",
          BrowserStateDependencyManager::GetInstance()) {}

LegacyStrikeDatabaseFactory::~LegacyStrikeDatabaseFactory() {}

std::unique_ptr<KeyedService>
LegacyStrikeDatabaseFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<autofill::LegacyStrikeDatabase>(
      chrome_browser_state->GetStatePath().Append(
          FILE_PATH_LITERAL("AutofillStrikeDatabase")));
}

}  // namespace autofill
