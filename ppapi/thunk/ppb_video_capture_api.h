// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_VIDEO_CAPTURE_API_H_
#define PPAPI_THUNK_VIDEO_CAPTURE_API_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "ppapi/c/dev/ppb_video_capture_dev.h"

namespace ppapi {

struct DeviceRefData;
class TrackedCallback;

namespace thunk {

class PPB_VideoCapture_API {
 public:
  virtual ~PPB_VideoCapture_API() {}

  virtual int32_t EnumerateDevices(PP_Resource* devices,
                                   scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t Open(const std::string& device_id,
                       const PP_VideoCaptureDeviceInfo_Dev& requested_info,
                       uint32_t buffer_count,
                       scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t StartCapture() = 0;
  virtual int32_t ReuseBuffer(uint32_t buffer) = 0;
  virtual int32_t StopCapture() = 0;
  virtual void Close() = 0;

  // For backward compatibility.
  virtual int32_t StartCapture0_1(
      const PP_VideoCaptureDeviceInfo_Dev& requested_info,
      uint32_t buffer_count) = 0;

  // This function is not exposed through the C API, but returns the internal
  // data for easy proxying.
  virtual const std::vector<DeviceRefData>& GetDeviceRefData() const = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_VIDEO_CAPTURE_API_H_
