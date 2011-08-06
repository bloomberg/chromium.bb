// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_VIDEO_CAPTURE_API_H_
#define PPAPI_THUNK_VIDEO_CAPTURE_API_H_

#include "ppapi/c/dev/ppb_video_capture_dev.h"

namespace ppapi {
namespace thunk {

class PPB_VideoCapture_API {
 public:
  virtual ~PPB_VideoCapture_API() {}

  virtual int32_t StartCapture(
      const PP_VideoCaptureDeviceInfo_Dev& requested_info,
      uint32_t buffer_count) = 0;
  virtual int32_t ReuseBuffer(uint32_t buffer) = 0;
  virtual int32_t StopCapture() = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_VIDEO_CAPTURE_API_H_
