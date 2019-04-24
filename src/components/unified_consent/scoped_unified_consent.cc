// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/scoped_unified_consent.h"

#include <map>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "components/unified_consent/feature.h"

namespace unified_consent {

ScopedUnifiedConsent::ScopedUnifiedConsent(UnifiedConsentFeatureState state) {
  switch (state) {
    case UnifiedConsentFeatureState::kDisabled:
      unified_consent_feature_list_.InitAndDisableFeature(kUnifiedConsent);
      break;
    case UnifiedConsentFeatureState::kEnabled:
      unified_consent_feature_list_.InitAndEnableFeature(kUnifiedConsent);
      break;
  }
}

ScopedUnifiedConsent::~ScopedUnifiedConsent() {}

}  // namespace unified_consent
