// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "third_party/webrtc/system_wrappers/include/runtime_enabled_features.h"

// Define webrtc::runtime_features::IsFeatureEnabled to provide webrtc with a
// chromium runtime flags access.

namespace webrtc {

namespace runtime_enabled_features {


const base::Feature kWebRtcDualStreamMode{"WebRTC-DualStreamMode",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

bool IsFeatureEnabled(std::string feature_name) {
  if (feature_name == "WebRtcDualStreamMode")
    return base::FeatureList::IsEnabled(kWebRtcDualStreamMode);
  return false;
}

}  // namespace runtime_enabled_features

}  // namespace webrtc
