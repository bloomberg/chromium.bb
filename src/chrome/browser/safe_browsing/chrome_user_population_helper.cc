// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_user_population_helper.h"

#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/browser/user_population.h"
#include "components/sync/driver/sync_service.h"

namespace safe_browsing {

namespace {

absl::optional<ChromeUserPopulation>& GetCachedUserPopulation(
    Profile* profile) {
  static base::NoDestructor<
      std::map<Profile*, absl::optional<ChromeUserPopulation>>>
      instance;
  return (*instance)[profile];
}

NoCachedPopulationReason& GetNoCachedPopulationReason(Profile* profile) {
  static base::NoDestructor<std::map<Profile*, NoCachedPopulationReason>>
      instance;
  auto it = instance->find(profile);
  if (it == instance->end()) {
    (*instance)[profile] = NoCachedPopulationReason::kStartup;
  }

  return (*instance)[profile];
}

void ComparePopulationWithCache(Profile* profile,
                                const ChromeUserPopulation& population) {
  const absl::optional<ChromeUserPopulation>& cached_population =
      GetCachedUserPopulation(profile);
  if (!cached_population) {
    base::UmaHistogramEnumeration("SafeBrowsing.NoCachedPopulationReason",
                                  GetNoCachedPopulationReason(profile));
    return;
  }

  base::UmaHistogramBoolean(
      "SafeBrowsing.PopulationMatchesCachedValue.Population",
      cached_population->user_population() == population.user_population());
  base::UmaHistogramBoolean(
      "SafeBrowsing.PopulationMatchesCachedValue.UserAgent",
      cached_population->user_agent() == population.user_agent());
  base::UmaHistogramBoolean(
      "SafeBrowsing.PopulationMatchesCachedValue.Mbb",
      cached_population->is_mbb_enabled() == population.is_mbb_enabled());
}

}  // namespace

void ClearCachedUserPopulation(Profile* profile,
                               NoCachedPopulationReason reason) {
  GetCachedUserPopulation(profile) = absl::nullopt;
  GetNoCachedPopulationReason(profile) = reason;
}

ChromeUserPopulation GetUserPopulationForProfile(Profile* profile) {
  // |profile| may be null in tests.
  if (!profile)
    return ChromeUserPopulation();

  syncer::SyncService* sync = SyncServiceFactory::GetForProfile(profile);
  bool is_history_sync_enabled =
      sync && sync->IsSyncFeatureActive() && !sync->IsLocalSyncEnabled() &&
      sync->GetActiveDataTypes().Has(syncer::HISTORY_DELETE_DIRECTIVES);

  bool is_under_advanced_protection = false;

#if BUILDFLAG(FULL_SAFE_BROWSING)
  AdvancedProtectionStatusManager* advanced_protection_manager =
      AdvancedProtectionStatusManagerFactory::GetForProfile(profile);
  is_under_advanced_protection =
      advanced_protection_manager &&
      advanced_protection_manager->IsUnderAdvancedProtection();
#endif

  absl::optional<size_t> num_profiles;
  absl::optional<size_t> num_loaded_profiles;
  absl::optional<size_t> num_open_profiles;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // |profile_manager| may be null in tests.
  if (profile_manager) {
    num_profiles = profile_manager->GetNumberOfProfiles();
    num_loaded_profiles = profile_manager->GetLoadedProfiles().size();

    // On ChromeOS multiple profiles doesn't apply, and GetLastOpenedProfiles
    // causes crashes on ChromeOS. See https://crbug.com/1211793.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    num_open_profiles = profile_manager->GetLastOpenedProfiles().size();
#endif
  }

  ChromeUserPopulation population = GetUserPopulation(
      profile->GetPrefs(), profile->IsOffTheRecord(), is_history_sync_enabled,
      is_under_advanced_protection,
      g_browser_process->browser_policy_connector(), std::move(num_profiles),
      std::move(num_loaded_profiles), std::move(num_open_profiles));

  ComparePopulationWithCache(profile, population);
  GetCachedUserPopulation(profile) = population;

  return population;
}

}  // namespace safe_browsing
