// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNIFIED_CONSENT_FEATURE_H_
#define COMPONENTS_UNIFIED_CONSENT_FEATURE_H_

#include "base/feature_list.h"

namespace unified_consent {

// Single consent for Google services in Chrome.
extern const base::Feature kUnifiedConsent;

// Returns true if the unified consent feature state is enabled.
bool IsUnifiedConsentFeatureEnabled();

}  // namespace unified_consent

#endif  // COMPONENTS_UNIFIED_CONSENT_FEATURE_H_
