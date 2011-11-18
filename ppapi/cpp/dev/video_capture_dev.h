// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_VIDEO_CAPTURE_DEV_H_
#define PPAPI_CPP_DEV_VIDEO_CAPTURE_DEV_H_

#include "ppapi/c/dev/pp_video_capture_dev.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class Instance;

class VideoCapture_Dev : public Resource {
 public:
  explicit VideoCapture_Dev(const Instance& instance);
  VideoCapture_Dev(PP_Resource resource);
  VideoCapture_Dev(const VideoCapture_Dev& other);

  // Returns true if the required interface is available.
  static bool IsAvailable();

  int32_t StartCapture(const PP_VideoCaptureDeviceInfo_Dev& requested_info,
                       uint32_t buffer_count);
  int32_t ReuseBuffer(uint32_t buffer);
  int32_t StopCapture();
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_VIDEO_CAPTURE_DEV_H_
