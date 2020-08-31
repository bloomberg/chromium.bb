// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/domain_diversity_reporter_factory.h"

#include <string>

#include "base/bind.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "chrome/browser/history/domain_diversity_reporter.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

// static
DomainDiversityReporter* DomainDiversityReporterFactory::GetForProfile(
    Profile* profile) {
  return static_cast<DomainDiversityReporter*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
DomainDiversityReporterFactory* DomainDiversityReporterFactory::GetInstance() {
  return base::Singleton<DomainDiversityReporterFactory>::get();
}

// static
std::unique_ptr<KeyedService> DomainDiversityReporterFactory::BuildInstanceFor(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);

  return std::make_unique<DomainDiversityReporter>(
      history_service, profile->GetPrefs(), base::DefaultClock::GetInstance());
}

DomainDiversityReporterFactory::DomainDiversityReporterFactory()
    : BrowserContextKeyedServiceFactory(
          "DomainDiversityReporter",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HistoryServiceFactory::GetInstance());
}

DomainDiversityReporterFactory::~DomainDiversityReporterFactory() = default;

KeyedService* DomainDiversityReporterFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return BuildInstanceFor(static_cast<Profile*>(profile)).release();
}

void DomainDiversityReporterFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  DomainDiversityReporter::RegisterProfilePrefs(registry);
}

content::BrowserContext* DomainDiversityReporterFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool DomainDiversityReporterFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

bool DomainDiversityReporterFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}
