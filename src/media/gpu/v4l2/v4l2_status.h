// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_STATUS_H_
#define MEDIA_GPU_V4L2_V4L2_STATUS_H_

#include "media/base/status.h"

namespace media {

enum class V4L2StatusCodes : StatusCodeType {
  kOk = 0,
  kNoDevice = 1,
  kFailedToStopStreamQueue = 2,
  kNoProfile = 3,
  kMaxDecoderInstanceCount = 4,
  kNoDriverSupportForFourcc = 5,
  kFailedFileCapabilitiesCheck = 6,
  kFailedResourceAllocation = 7,
  kBadFormat = 8,
  kFailedToStartStreamQueue = 9,
};

struct V4L2StatusTraits {
  using Codes = V4L2StatusCodes;
  static constexpr StatusGroupType Group() { return "V4L2StatusCode"; }
  static constexpr V4L2StatusCodes DefaultEnumValue() { return Codes::kOk; }
};
using V4L2Status = TypedStatus<V4L2StatusTraits>;

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_STATUS_H_