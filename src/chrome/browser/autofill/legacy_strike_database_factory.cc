// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/legacy_strike_database_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/payments/legacy_strike_database.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace autofill {

// static
LegacyStrikeDatabase* LegacyStrikeDatabaseFactory::GetForProfile(
    Profile* profile) {
  return static_cast<LegacyStrikeDatabase*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
LegacyStrikeDatabaseFactory* LegacyStrikeDatabaseFactory::GetInstance() {
  return base::Singleton<LegacyStrikeDatabaseFactory>::get();
}

LegacyStrikeDatabaseFactory::LegacyStrikeDatabaseFactory()
    : BrowserContextKeyedServiceFactory(
          "AutofillLegacyStrikeDatabase",
          BrowserContextDependencyManager::GetInstance()) {}

LegacyStrikeDatabaseFactory::~LegacyStrikeDatabaseFactory() {}

KeyedService* LegacyStrikeDatabaseFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  // Note: This instance becomes owned by an object that never gets destroyed,
  // effectively leaking it until browser close. Only one is created per
  // profile, and closing-then-opening a profile returns the same instance.
  return new LegacyStrikeDatabase(
      profile->GetPath().Append(FILE_PATH_LITERAL("AutofillStrikeDatabase")));
}

}  // namespace autofill
