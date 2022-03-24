// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_first_mode_settings_tracker.h"

#include "base/metrics/histogram_functions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {
const char kHttpsFirstModeServiceName[] = "HttpsFirstModeService";
const char kHttpsFirstModeSyntheticFieldTrialName[] =
    "HttpsFirstModeClientSetting";
const char kHttpsFirstModeSyntheticFieldTrialEnabledGroup[] = "Enabled";
const char kHttpsFirstModeSyntheticFieldTrialDisabledGroup[] = "Disabled";
}  // namespace

HttpsFirstModeService::HttpsFirstModeService(PrefService* pref_service)
    : pref_service_(pref_service) {
  pref_change_registrar_.Init(pref_service_);
  // Using base::Unretained() here is safe as the PrefChangeRegistrar is owned
  // by `this`.
  pref_change_registrar_.Add(
      prefs::kHttpsOnlyModeEnabled,
      base::BindRepeating(&HttpsFirstModeService::OnHttpsFirstModePrefChanged,
                          base::Unretained(this)));
  // Make sure the pref state is logged and the synthetic field trial state is
  // created at startup (as the pref may never change over the session).
  bool enabled = pref_service_->GetBoolean(prefs::kHttpsOnlyModeEnabled);
  base::UmaHistogramBoolean("Security.HttpsFirstMode.SettingEnabledAtStartup",
                            enabled);
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      kHttpsFirstModeSyntheticFieldTrialName,
      enabled ? kHttpsFirstModeSyntheticFieldTrialEnabledGroup
              : kHttpsFirstModeSyntheticFieldTrialDisabledGroup);
}

HttpsFirstModeService::~HttpsFirstModeService() = default;

void HttpsFirstModeService::OnHttpsFirstModePrefChanged() {
  bool enabled = pref_service_->GetBoolean(prefs::kHttpsOnlyModeEnabled);
  base::UmaHistogramBoolean("Security.HttpsFirstMode.SettingChanged", enabled);
  // Update synthetic field trial group registration.
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      kHttpsFirstModeSyntheticFieldTrialName,
      enabled ? kHttpsFirstModeSyntheticFieldTrialEnabledGroup
              : kHttpsFirstModeSyntheticFieldTrialDisabledGroup);
}

// static
HttpsFirstModeService* HttpsFirstModeServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<HttpsFirstModeService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
HttpsFirstModeServiceFactory* HttpsFirstModeServiceFactory::GetInstance() {
  return base::Singleton<HttpsFirstModeServiceFactory>::get();
}

HttpsFirstModeServiceFactory::HttpsFirstModeServiceFactory()
    : BrowserContextKeyedServiceFactory(
          kHttpsFirstModeServiceName,
          BrowserContextDependencyManager::GetInstance()) {}

HttpsFirstModeServiceFactory::~HttpsFirstModeServiceFactory() = default;

// BrowserContextKeyedServiceFactory:
KeyedService* HttpsFirstModeServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return new HttpsFirstModeService(profile->GetPrefs());
}

content::BrowserContext* HttpsFirstModeServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Don't create a service for non-regular profiles. This includes Incognito
  // (which uses the settings of the main profile) and Guest Mode. Also
  // explicitly check for ChromeOS sign-in profiles (which would cause
  // double-counting of at-startup metrics for ChromeOS restarts) which are not
  // covered by the `IsRegularProfile()` check.
  Profile* const profile = Profile::FromBrowserContext(context);
  if (!profile->IsRegularProfile()) {
    return nullptr;
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (chromeos::ProfileHelper::IsSigninProfile(profile)) {
    return nullptr;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return context;
}
