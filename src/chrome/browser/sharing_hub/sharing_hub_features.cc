// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing_hub/sharing_hub_features.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/share/share_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

namespace sharing_hub {

namespace {

// Whether the sharing hub feature should be disabled by policy.
bool SharingHubDisabledByPolicy(content::BrowserContext* context) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  const PrefService* prefs = Profile::FromBrowserContext(context)->GetPrefs();
  return !prefs->GetBoolean(prefs::kDesktopSharingHubEnabled);
#else
  return false;
#endif
}

// Whether screenshots-related features should be disabled by policy.
// Currently used by desktop.
// TODO(crbug.com/1261244): possibly apply to Android features.
bool ScreenshotsDisabledByPolicy(content::BrowserContext* context) {
  const PrefService* prefs = Profile::FromBrowserContext(context)->GetPrefs();
  return prefs->GetBoolean(prefs::kDisableScreenshots);
}

}  // namespace

bool SharingHubOmniboxEnabled(content::BrowserContext* context) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (SharingHubDisabledByPolicy(context))
    return false;
#endif

  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile)
    return false;
  return !profile->IsIncognitoProfile() && !profile->IsGuestSession();
}

bool DesktopScreenshotsFeatureEnabled(content::BrowserContext* context) {
  return (base::FeatureList::IsEnabled(kDesktopScreenshots) ||
          share::AreUpcomingSharingFeaturesEnabled()) &&
         !ScreenshotsDisabledByPolicy(context);
}

const base::Feature kDesktopScreenshots{"DesktopScreenshots",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDesktopSharingHubEnabled, true);
}
#endif

}  // namespace sharing_hub
