// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"

#include "base/time/time.h"
#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/prefs/pref_registry_simple.h"
#endif

namespace media_router {

#if !BUILDFLAG(IS_ANDROID)

void RegisterAccessCodeProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kAccessCodeCastEnabled, false,
                                PrefRegistry::PUBLIC);
  registry->RegisterIntegerPref(prefs::kAccessCodeCastDeviceDuration, 0,
                                PrefRegistry::PUBLIC);
}

bool GetAccessCodeCastEnabledPref(PrefService* pref_service) {
  return pref_service->GetBoolean(prefs::kAccessCodeCastEnabled);
}

base::TimeDelta GetAccessCodeDeviceDurationPref(PrefService* pref_service) {
  if (!GetAccessCodeCastEnabledPref(pref_service)) {
    return base::Seconds(0);
  }
  return base::Seconds(
      pref_service->GetInteger(prefs::kAccessCodeCastDeviceDuration));
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace media_router
