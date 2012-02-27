// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_VIDEO_CAPTURE_DEV_H_
#define PPAPI_CPP_DEV_VIDEO_CAPTURE_DEV_H_

#include <vector>

#include "ppapi/c/dev/pp_video_capture_dev.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class CompletionCallback;
class DeviceRef_Dev;
class InstanceHandle;

class VideoCapture_Dev : public Resource {
 public:
  explicit VideoCapture_Dev(const InstanceHandle& instance);
  VideoCapture_Dev(PP_Resource resource);
  VideoCapture_Dev(const VideoCapture_Dev& other);

  virtual ~VideoCapture_Dev();

  VideoCapture_Dev& operator=(const VideoCapture_Dev& other);

  // Returns true if the required interface is available.
  static bool IsAvailable();

  // |devices| must stay alive until either this VideoCapture_Dev object goes
  // away or |callback| is run.
  int32_t EnumerateDevices(std::vector<DeviceRef_Dev>* devices,
                           const CompletionCallback& callback);
  int32_t Open(const DeviceRef_Dev& device_ref,
               const PP_VideoCaptureDeviceInfo_Dev& requested_info,
               uint32_t buffer_count,
               const CompletionCallback& callback);
  int32_t StartCapture();
  int32_t ReuseBuffer(uint32_t buffer);
  int32_t StopCapture();
  void Close();

 private:
  struct EnumerateDevicesState;

  void AbortEnumerateDevices();

  // |user_data| is an EnumerateDevicesState object. It is this method's
  // responsibility to delete it.
  static void OnEnumerateDevicesComplete(void* user_data, int32_t result);

  // Not owned by this object.
  EnumerateDevicesState* enum_state_;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_VIDEO_CAPTURE_DEV_H_
