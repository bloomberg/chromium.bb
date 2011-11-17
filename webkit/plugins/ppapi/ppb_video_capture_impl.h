// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_VIDEO_CAPTURE_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_VIDEO_CAPTURE_IMPL_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_callback_factory.h"
#include "base/memory/scoped_ptr.h"
#include "media/video/capture/video_capture.h"
#include "ppapi/c/dev/ppp_video_capture_dev.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_video_capture_api.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/ppb_buffer_impl.h"

struct PP_VideoCaptureDeviceInfo_Dev;

namespace webkit {
namespace ppapi {

class PPB_VideoCapture_Impl : public ::ppapi::Resource,
                              public ::ppapi::thunk::PPB_VideoCapture_API,
                              public media::VideoCapture::EventHandler {
 public:
  explicit PPB_VideoCapture_Impl(PP_Instance instance);
  virtual ~PPB_VideoCapture_Impl();

  bool Init();

  // Resource overrides.
  virtual PPB_VideoCapture_API* AsPPB_VideoCapture_API() OVERRIDE;
  virtual void LastPluginRefWasDeleted() OVERRIDE;

  // PPB_VideoCapture implementation.
  virtual int32_t StartCapture(
      const PP_VideoCaptureDeviceInfo_Dev& requested_info,
      uint32_t buffer_count) OVERRIDE;
  virtual int32_t ReuseBuffer(uint32_t buffer) OVERRIDE;
  virtual int32_t StopCapture() OVERRIDE;

  // media::VideoCapture::EventHandler implementation.
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

 private:
  void ReleaseBuffers();
  void SendStatus();

  scoped_ptr<PluginDelegate::PlatformVideoCapture> platform_video_capture_;

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
  PP_VideoCaptureStatus_Dev status_;

  // Signifies that the plugin has given up all its refs, but the object is
  // still alive, possibly because the backend hasn't released the object as
  // |EventHandler| yet. It can be removed if/when |EventHandler| is made to be
  // refcounted (and made into a "member" of this object instead).
  bool is_dead_;

  DISALLOW_COPY_AND_ASSIGN(PPB_VideoCapture_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_VIDEO_CAPTURE_IMPL_H_
