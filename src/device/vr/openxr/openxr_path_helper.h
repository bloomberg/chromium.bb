// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_PATH_HELPER_H_
#define DEVICE_VR_OPENXR_OPENXR_PATH_HELPER_H_

#include <string>
#include <vector>

#include "device/vr/openxr/openxr_interaction_profiles.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXRPathHelper {
 public:
  OpenXRPathHelper();
  ~OpenXRPathHelper();

  XrResult Initialize(XrInstance instance);

  std::vector<std::string> GetInputProfiles(
      OpenXrInteractionProfileType interaction_profile) const;

  OpenXrInteractionProfileType GetInputProfileType(
      XrPath interaction_profile) const;

  XrPath GetInteractionProfileXrPath(OpenXrInteractionProfileType type) const;

 private:
  bool initialized_{false};

  std::unordered_map<OpenXrInteractionProfileType, XrPath>
      declared_interaction_profile_paths_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_PATH_HELPER_H_