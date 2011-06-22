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
#include "ppapi/thunk/ppb_video_decoder_api.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/resource.h"

struct PP_GLESBuffer_Dev;
struct PP_SysmemBuffer_Dev;
struct PP_VideoDecoderConfig_Dev;
struct PP_VideoBitstreamBuffer_Dev;
struct PPB_VideoDecoder_Dev;
struct PPP_VideoDecoder_Dev;

namespace webkit {
namespace ppapi {

class PluginInstance;

class PPB_VideoDecoder_Impl : public Resource,
                              public ::ppapi::thunk::PPB_VideoDecoder_API,
                              public media::VideoDecodeAccelerator::Client {
 public:
  explicit PPB_VideoDecoder_Impl(PluginInstance* instance);
  virtual ~PPB_VideoDecoder_Impl();

  // ResourceObjectBase overrides.
  virtual PPB_VideoDecoder_API* AsPPB_VideoDecoder_API() OVERRIDE;

  // PPB_VideoDecoder implementation.
  virtual PP_Bool GetConfigs(const PP_VideoConfigElement* requested_configs,
                             PP_VideoConfigElement* matching_configs,
                             uint32_t matching_configs_size,
                             uint32_t* num_of_matching_configs) OVERRIDE;
  virtual int32_t Initialize(PP_Resource context_id,
                             const PP_VideoConfigElement* dec_config,
                             PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t Decode(const PP_VideoBitstreamBuffer_Dev* bitstream_buffer,
                         PP_CompletionCallback callback) OVERRIDE;
  virtual void AssignGLESBuffers(uint32_t no_of_buffers,
                                 const PP_GLESBuffer_Dev* buffers) OVERRIDE;
  virtual void AssignSysmemBuffers(uint32_t no_of_buffers,
                                   const PP_SysmemBuffer_Dev* buffers) OVERRIDE;
  virtual void ReusePictureBuffer(int32_t picture_buffer_id) OVERRIDE;
  virtual int32_t Flush(PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t Abort(PP_CompletionCallback callback) OVERRIDE;

  // media::VideoDecodeAccelerator::Client implementation.
  virtual void ProvidePictureBuffers(
      uint32 requested_num_of_buffers,
      const gfx::Size& dimensions,
      media::VideoDecodeAccelerator::MemoryType type) OVERRIDE;
  virtual void DismissPictureBuffer(int32 picture_buffer_id) OVERRIDE;
  virtual void PictureReady(const media::Picture& picture) OVERRIDE;
  virtual void NotifyInitializeDone() OVERRIDE;
  virtual void NotifyEndOfStream() OVERRIDE;
  virtual void NotifyError(
      media::VideoDecodeAccelerator::Error error) OVERRIDE;
  virtual void NotifyFlushDone() OVERRIDE;
  virtual void NotifyEndOfBitstreamBuffer(int32 buffer_id) OVERRIDE;
  virtual void NotifyAbortDone() OVERRIDE;

 private:
  // Key: bitstream_buffer_id, value: callback to run when bitstream decode is
  // done.
  typedef std::map<int32, PP_CompletionCallback> CallbackById;

  // This is NULL before initialization, and if this PPB_VideoDecoder_Impl is
  // swapped with another.
  scoped_ptr<PluginDelegate::PlatformVideoDecoder> platform_video_decoder_;

  // Factory to produce our callbacks.
  base::ScopedCallbackFactory<PPB_VideoDecoder_Impl> callback_factory_;

  PP_CompletionCallback initialization_callback_;
  PP_CompletionCallback abort_callback_;
  PP_CompletionCallback flush_callback_;
  CallbackById bitstream_buffer_callbacks_;

  // Reference to the plugin requesting this interface.
  const PPP_VideoDecoder_Dev* ppp_videodecoder_;

  DISALLOW_COPY_AND_ASSIGN(PPB_VideoDecoder_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_VIDEO_DECODER_IMPL_H_
