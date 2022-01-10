// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"

namespace enterprise_connectors {

const base::Feature kDeviceTrustConnectorEnabled{
    "DeviceTrustConnectorEnabled", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsDeviceTrustConnectorFeatureEnabled() {
  return base::FeatureList::IsEnabled(kDeviceTrustConnectorEnabled);
}

}  // namespace enterprise_connectors
