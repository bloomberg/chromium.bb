// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/scoped_unified_consent.h"
#include "ios/chrome/app/tests_hook.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
unified_consent::ScopedUnifiedConsent* gScopedUnifiedConsent = nullptr;
}

namespace tests_hook {

bool DisableAppGroupAccess() {
  return true;
}

bool DisableContentSuggestions() {
  return true;
}

bool DisableContextualSearch() {
  return true;
}

bool DisableFirstRun() {
  return true;
}

bool DisableGeolocation() {
  return true;
}

bool DisableSigninRecallPromo() {
  return true;
}

bool DisableUpdateService() {
  return true;
}

void SetUpTestsIfPresent() {
  // Enables unified consent feature.
  CHECK(!gScopedUnifiedConsent);
  gScopedUnifiedConsent = new unified_consent::ScopedUnifiedConsent(
      unified_consent::UnifiedConsentFeatureState::kEnabled);
}

void RunTestsIfPresent() {
  // No-op for Earl Grey.
}

}  // namespace tests_hook
