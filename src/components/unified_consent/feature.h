// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNIFIED_CONSENT_FEATURE_H_
#define COMPONENTS_UNIFIED_CONSENT_FEATURE_H_

#include "base/feature_list.h"

namespace unified_consent {

// State of the "Unified Consent" feature.
enum class UnifiedConsentFeatureState {
  // Unified consent is disabled.
  kDisabled,
  // Unified consent is enabled, but the bump is not shown.
  kEnabledNoBump,
  // Unified consent is enabled and the bump is shown.
  kEnabledWithBump
};

// Improved and unified consent for privacy-related features.
extern const base::Feature kUnifiedConsent;
extern const char kUnifiedConsentShowBumpParameter[];
extern const base::Feature kForceUnifiedConsentBump;

// Returns true if the unified consent feature state is kEnabledNoBump or
// kEnabledWithBump. Note that the bump may not be enabled, even if this returns
// true. To check if the bump is enabled, use
// IsUnifiedConsentFeatureWithBumpEnabled().
bool IsUnifiedConsentFeatureEnabled();

// Returns true if the unified consent feature state is kEnabledWithBump.
bool IsUnifiedConsentFeatureWithBumpEnabled();

}  // namespace unified_consent

#endif  // COMPONENTS_UNIFIED_CONSENT_FEATURE_H_
