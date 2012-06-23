// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_VIDEO_CAPTURE_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_VIDEO_CAPTURE_IMPL_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "media/video/capture/video_capture.h"
#include "media/video/capture/video_capture_types.h"
#include "ppapi/c/dev/ppp_video_capture_dev.h"
#include "ppapi/shared_impl/ppb_video_capture_shared.h"
#include "ppapi/shared_impl/resource.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/ppb_buffer_impl.h"

struct PP_VideoCaptureDeviceInfo_Dev;

namespace webkit {
namespace ppapi {

class PPB_VideoCapture_Impl
    : public ::ppapi::PPB_VideoCapture_Shared,
      public PluginDelegate::PlatformVideoCaptureEventHandler,
      public base::SupportsWeakPtr<PPB_VideoCapture_Impl> {
 public:
  explicit PPB_VideoCapture_Impl(PP_Instance instance);
  virtual ~PPB_VideoCapture_Impl();

  bool Init();

  // PluginDelegate::PlatformVideoCaptureEventHandler implementation.
  virtual void OnStarted(media::VideoCapture* capture) OVERRIDE;
  virtual void OnStopped(media::VideoCapture* capture) OVERRIDE;
  virtual void OnPaused(media::VideoCapture* capture) OVERRIDE;
  virtual void OnError(media::VideoCapture* capture, int error_code) OVERRIDE;
  virtual void OnRemoved(media::VideoCapture* capture) OVERRIDE;
  virtual void OnBufferReady(
      media::VideoCapture* capture,
      scoped_refptr<media::VideoCapture::VideoFrameBuffer> buffer) OVERRIDE;
  virtual void OnDeviceInfoReceived(
      media::VideoCapture* capture,
      const media::VideoCaptureParams& device_info) OVERRIDE;
  virtual void OnInitialized(media::VideoCapture* capture,
                             bool succeeded) OVERRIDE;

 private:
  typedef std::vector< ::ppapi::DeviceRefData> DeviceRefDataVector;

  // PPB_VideoCapture_Shared implementation.
  virtual int32_t InternalEnumerateDevices(
      PP_Resource* devices,
      scoped_refptr< ::ppapi::TrackedCallback> callback) OVERRIDE;
  virtual int32_t InternalOpen(
      const std::string& device_id,
      const PP_VideoCaptureDeviceInfo_Dev& requested_info,
      uint32_t buffer_count,
      scoped_refptr< ::ppapi::TrackedCallback> callback) OVERRIDE;
  virtual int32_t InternalStartCapture() OVERRIDE;
  virtual int32_t InternalReuseBuffer(uint32_t buffer) OVERRIDE;
  virtual int32_t InternalStopCapture() OVERRIDE;
  virtual void InternalClose() OVERRIDE;
  virtual int32_t InternalStartCapture0_1(
      const PP_VideoCaptureDeviceInfo_Dev& requested_info,
      uint32_t buffer_count) OVERRIDE;
  virtual const DeviceRefDataVector& InternalGetDeviceRefData() const OVERRIDE;

  void ReleaseBuffers();
  void SendStatus();

  void SetRequestedInfo(const PP_VideoCaptureDeviceInfo_Dev& device_info,
                        uint32_t buffer_count);

  void DetachPlatformVideoCapture();

  void EnumerateDevicesCallbackFunc(int request_id,
                                    bool succeeded,
                                    const DeviceRefDataVector& devices);

  scoped_refptr<PluginDelegate::PlatformVideoCapture> platform_video_capture_;

  size_t buffer_count_hint_;
  struct BufferInfo {
    BufferInfo();
    ~BufferInfo();

    bool in_use;
    void* data;
    scoped_refptr<PPB_Buffer_Impl> buffer;
  };
  std::vector<BufferInfo> buffers_;

  const PPP_VideoCapture_Dev* ppp_videocapture_;

  DeviceRefDataVector devices_data_;

  media::VideoCaptureCapability capability_;

  DISALLOW_COPY_AND_ASSIGN(PPB_VideoCapture_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_VIDEO_CAPTURE_IMPL_H_
