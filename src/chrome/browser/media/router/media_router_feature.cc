// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_feature.h"

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_features.h"
#include "crypto/random.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/buildflags.h"

#if defined(OS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "media/base/media_switches.h"
#endif  // defined(OS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS)

#if !defined(OS_ANDROID)
#include "components/prefs/pref_registry_simple.h"
#endif

namespace media_router {

#if !defined(OS_ANDROID)
#if !defined(OFFICIAL_BUILD)
// Enables the media router. Can be useful to disable for local
// development on Mac because DIAL local discovery opens a local port
// and triggers a permission prompt. Only toggleable for developer builds.
const base::Feature kMediaRouter{"MediaRouter",
                                 base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // !defined(OFFICIAL_BUILD)
// Controls if browser side DialMediaRouteProvider is enabled.
const base::Feature kDialMediaRouteProvider{"DialMediaRouteProvider",
                                            base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kCastMediaRouteProvider{"CastMediaRouteProvider",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kCastAllowAllIPsFeature{"CastAllowAllIPs",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kGlobalMediaControlsCastStartStop{
    "GlobalMediaControlsCastStartStop", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kAllowAllSitesToInitiateMirroring{
    "AllowAllSitesToInitiateMirroring", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // !defined(OS_ANDROID)

#if defined(OS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS)
namespace {
const PrefService::Preference* GetMediaRouterPref(
    content::BrowserContext* context) {
  return user_prefs::UserPrefs::Get(context)->FindPreference(
      ::prefs::kEnableMediaRouter);
}
}  // namespace
#endif  // defined(OS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS)

bool MediaRouterEnabled(content::BrowserContext* context) {
#if !defined(OFFICIAL_BUILD) && !defined(OS_ANDROID)
  if (!base::FeatureList::IsEnabled(kMediaRouter))
    return false;
#endif  // !defined(OFFICIAL_BUILD) && !defined(OS_ANDROID)

#if defined(OS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS)
  const PrefService::Preference* pref = GetMediaRouterPref(context);
  // Only use the pref value if it set from a mandatory policy.
  if (pref->IsManaged() && !pref->IsDefaultValue()) {
    bool allowed = false;
    CHECK(pref->GetValue()->GetAsBoolean(&allowed));
    return allowed;
  }

  // The component extension cannot be loaded in guest sessions.
  // TODO(crbug.com/756243): Figure out why.
  return !Profile::FromBrowserContext(context)->IsGuestSession();
#else   // !(defined(OS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS))
  return false;
#endif  // defined(OS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS)
}

#if !defined(OS_ANDROID)
void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kMediaRouterCastAllowAllIPs, false,
                                PrefRegistry::PUBLIC);
}

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  // TODO(imcheng): Migrate existing Media Router prefs to here.
  registry->RegisterStringPref(prefs::kMediaRouterReceiverIdHashToken, "",
                               PrefRegistry::PUBLIC);
}

bool GetCastAllowAllIPsPref(PrefService* pref_service) {
  auto* pref = pref_service->FindPreference(prefs::kMediaRouterCastAllowAllIPs);

  // Only use the pref value if it is set from a mandatory policy.
  bool allow_all_ips = false;
  if (pref->IsManaged() && !pref->IsDefaultValue()) {
    CHECK(pref->GetValue()->GetAsBoolean(&allow_all_ips));
  } else {
    allow_all_ips = base::FeatureList::IsEnabled(kCastAllowAllIPsFeature);
  }

  return allow_all_ips;
}

std::string GetReceiverIdHashToken(PrefService* pref_service) {
  static constexpr size_t kHashTokenSize = 64;
  std::string token =
      pref_service->GetString(prefs::kMediaRouterReceiverIdHashToken);
  if (token.empty()) {
    crypto::RandBytes(base::WriteInto(&token, kHashTokenSize + 1),
                      kHashTokenSize);
    base::Base64Encode(token, &token);
    pref_service->SetString(prefs::kMediaRouterReceiverIdHashToken, token);
  }
  return token;
}

bool DialMediaRouteProviderEnabled() {
  return base::FeatureList::IsEnabled(kDialMediaRouteProvider);
}

bool CastMediaRouteProviderEnabled() {
  return base::FeatureList::IsEnabled(kCastMediaRouteProvider);
}

bool GlobalMediaControlsCastStartStopEnabled() {
  return base::FeatureList::IsEnabled(kGlobalMediaControlsCastStartStop) &&
         base::FeatureList::IsEnabled(media::kGlobalMediaControlsForCast) &&
         base::FeatureList::IsEnabled(
             media::kGlobalMediaControlsOverlayControls);
}

#endif  // !defined(OS_ANDROID)

}  // namespace media_router
