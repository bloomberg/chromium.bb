// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_VIDEO_CAPTURE_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_VIDEO_CAPTURE_SHARED_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/ppb_video_capture_api.h"

namespace ppapi {

class PPAPI_SHARED_EXPORT PPB_VideoCapture_Shared
    : public Resource,
      NON_EXPORTED_BASE(public thunk::PPB_VideoCapture_API) {
 public:
  explicit PPB_VideoCapture_Shared(PP_Instance instance);
  explicit PPB_VideoCapture_Shared(const HostResource& host_resource);
  virtual ~PPB_VideoCapture_Shared();

  // Resource implementation.
  virtual thunk::PPB_VideoCapture_API* AsPPB_VideoCapture_API() OVERRIDE;

  // PPB_VideoCapture_API implementation.
  virtual int32_t EnumerateDevices(
      PP_Resource* devices,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t Open(const std::string& device_id,
                       const PP_VideoCaptureDeviceInfo_Dev& requested_info,
                       uint32_t buffer_count,
                       scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t StartCapture() OVERRIDE;
  virtual int32_t ReuseBuffer(uint32_t buffer) OVERRIDE;
  virtual int32_t StopCapture() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual int32_t StartCapture0_1(
      const PP_VideoCaptureDeviceInfo_Dev& requested_info,
      uint32_t buffer_count) OVERRIDE;
  virtual const std::vector<DeviceRefData>& GetDeviceRefData() const OVERRIDE;

  void OnEnumerateDevicesComplete(int32_t result,
                                  const std::vector<DeviceRefData>& devices);
  void OnOpenComplete(int32_t result);

 protected:
  enum OpenState {
    BEFORE_OPEN,
    OPENED,
    CLOSED
  };

  // Subclasses should implement these methods to do impl- and proxy-specific
  // work.
  virtual int32_t InternalEnumerateDevices(
      PP_Resource* devices,
      scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t InternalOpen(
      const std::string& device_id,
      const PP_VideoCaptureDeviceInfo_Dev& requested_info,
      uint32_t buffer_count,
      scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t InternalStartCapture() = 0;
  virtual int32_t InternalReuseBuffer(uint32_t buffer) = 0;
  virtual int32_t InternalStopCapture() = 0;
  virtual void InternalClose() = 0;
  virtual int32_t InternalStartCapture0_1(
      const PP_VideoCaptureDeviceInfo_Dev& requested_info,
      uint32_t buffer_count) = 0;
  virtual const std::vector<DeviceRefData>& InternalGetDeviceRefData(
      ) const = 0;

  // Checks whether |status| is expected and sets |status_| if yes. If |forced|
  // is set to true, this method will bypass sanity check and always set
  // |status_|.
  bool SetStatus(PP_VideoCaptureStatus_Dev status, bool forced);

  OpenState open_state_;
  PP_VideoCaptureStatus_Dev status_;

  scoped_refptr<TrackedCallback> enumerate_devices_callback_;
  scoped_refptr<TrackedCallback> open_callback_;

  // Output parameter of EnumerateDevices(). It should not be accessed after
  // |enumerate_devices_callback_| is run.
  PP_Resource* devices_;

  ResourceObjectType resource_object_type_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PPB_VideoCapture_Shared);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_VIDEO_CAPTURE_SHARED_H_
