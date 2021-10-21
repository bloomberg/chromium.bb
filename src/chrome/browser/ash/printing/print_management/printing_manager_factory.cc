// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/print_management/printing_manager_factory.h"

#include "chrome/browser/ash/printing/history/print_job_history_service_factory.h"
#include "chrome/browser/ash/printing/print_management/printing_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace ash {
namespace printing {
namespace print_management {

// static
PrintingManager* PrintingManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<PrintingManager*>(
      PrintingManagerFactory::GetInstance()->GetServiceForBrowserContext(
          profile, /*create=*/true));
}

// static
PrintingManagerFactory* PrintingManagerFactory::GetInstance() {
  return base::Singleton<PrintingManagerFactory>::get();
}

PrintingManagerFactory::PrintingManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "PrintingManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(PrintJobHistoryServiceFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(chromeos::CupsPrintJobManagerFactory::GetInstance());
}

PrintingManagerFactory::~PrintingManagerFactory() = default;

// static
KeyedService* PrintingManagerFactory::BuildInstanceFor(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);

  // We do not want an instance of PrintingManager on the lock screen. The
  // result is multiple print job notifications. https://crbug.com/1011532
  if (!ProfileHelper::IsRegularProfile(profile)) {
    return nullptr;
  }

  return new PrintingManager(
      PrintJobHistoryServiceFactory::GetForBrowserContext(context),
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      chromeos::CupsPrintJobManagerFactory::GetForBrowserContext(context),
      profile->GetPrefs());
}

KeyedService* PrintingManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return BuildInstanceFor(static_cast<Profile*>(context));
}

content::BrowserContext* PrintingManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

void PrintingManagerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  user_prefs->RegisterBooleanPref(prefs::kDeletePrintJobHistoryAllowed, true);
}

bool PrintingManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool PrintingManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace print_management
}  // namespace printing
}  // namespace ash
