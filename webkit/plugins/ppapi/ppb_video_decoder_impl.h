// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_VIDEO_DECODER_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_VIDEO_DECODER_IMPL_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_callback_factory.h"
#include "base/memory/scoped_ptr.h"
#include "ppapi/c/dev/pp_video_dev.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/video_decoder_impl.h"
#include "ppapi/thunk/ppb_video_decoder_api.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"

struct PP_PictureBuffer_Dev;
struct PP_VideoBitstreamBuffer_Dev;
struct PPB_VideoDecoder_Dev;
struct PPP_VideoDecoder_Dev;

namespace gpu {
namespace gles2 {
class GLES2Implementation;
}  // namespace gles2
}  // namespace gpu

namespace webkit {
namespace ppapi {

class PPB_VideoDecoder_Impl : public ::ppapi::Resource,
                              public ::ppapi::VideoDecoderImpl,
                              public media::VideoDecodeAccelerator::Client {
 public:
  virtual ~PPB_VideoDecoder_Impl();
  // See PPB_VideoDecoder_Dev::Create.  Returns 0 on failure to create &
  // initialize.
  static PP_Resource Create(PP_Instance instance,
                            PP_Resource graphics_context,
                            PP_VideoDecoder_Profile profile);

  // Resource overrides.
  virtual PPB_VideoDecoder_API* AsPPB_VideoDecoder_API() OVERRIDE;

  // PPB_VideoDecoder_API implementation.
  virtual int32_t Decode(const PP_VideoBitstreamBuffer_Dev* bitstream_buffer,
                         PP_CompletionCallback callback) OVERRIDE;
  virtual void AssignPictureBuffers(
      uint32_t no_of_buffers, const PP_PictureBuffer_Dev* buffers) OVERRIDE;
  virtual void ReusePictureBuffer(int32_t picture_buffer_id) OVERRIDE;
  virtual int32_t Flush(PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t Reset(PP_CompletionCallback callback) OVERRIDE;
  virtual void Destroy() OVERRIDE;

  // media::VideoDecodeAccelerator::Client implementation.
  virtual void ProvidePictureBuffers(
      uint32 requested_num_of_buffers, const gfx::Size& dimensions) OVERRIDE;
  virtual void DismissPictureBuffer(int32 picture_buffer_id) OVERRIDE;
  virtual void PictureReady(const media::Picture& picture) OVERRIDE;
  virtual void NotifyInitializeDone() OVERRIDE;
  virtual void NotifyEndOfStream() OVERRIDE;
  virtual void NotifyError(
      media::VideoDecodeAccelerator::Error error) OVERRIDE;
  virtual void NotifyFlushDone() OVERRIDE;
  virtual void NotifyEndOfBitstreamBuffer(int32 buffer_id) OVERRIDE;
  virtual void NotifyResetDone() OVERRIDE;

 private:
  explicit PPB_VideoDecoder_Impl(PP_Instance instance);
  bool Init(PP_Resource graphics_context,
            PluginDelegate::PlatformContext3D* context,
            gpu::gles2::GLES2Implementation* gles2_impl,
            PP_VideoDecoder_Profile profile);

  // This is NULL before initialization, and if this PPB_VideoDecoder_Impl is
  // swapped with another.
  scoped_refptr<PluginDelegate::PlatformVideoDecoder> platform_video_decoder_;

  // Reference to the plugin requesting this interface.
  const PPP_VideoDecoder_Dev* ppp_videodecoder_;

  DISALLOW_COPY_AND_ASSIGN(PPB_VideoDecoder_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_VIDEO_DECODER_IMPL_H_
