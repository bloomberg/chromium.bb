// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/face_detection_impl_mac_vision.h"

#include <dlfcn.h>
#include <objc/runtime.h>
#include <utility>

#include "base/logging.h"

namespace shape_detection {

FaceDetectionImplMacVision::FaceDetectionImplMacVision() {
  static void* const vision_framework =
      dlopen("/System/Library/Frameworks/Vision.framework/Vision", RTLD_LAZY);
  if (!vision_framework) {
    DLOG(ERROR) << "Failed to load Vision.framework";
    return;
  }

  Class request_class = NSClassFromString(@"VNDetectFaceLandmarksRequest");
  if (!request_class) {
    DLOG(ERROR) << "Failed to create VNDetectFaceLandmarksRequest object";
    return;
  }
  landmarks_request_.reset([[request_class alloc] init]);
}

FaceDetectionImplMacVision::~FaceDetectionImplMacVision() = default;

void FaceDetectionImplMacVision::Detect(const SkBitmap& bitmap,
                                        DetectCallback callback) {
  if (landmarks_request_)
    std::move(callback).Run({});
}

}  // namespace shape_detection
