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
                              public media::VideoDecodeAccelerator::Client {
 public:
  explicit PPB_VideoDecoder_Impl(PluginInstance* instance);
  virtual ~PPB_VideoDecoder_Impl();

  // Returns a pointer to the interface implementing PPB_VideoDecoder that is
  // exposed to the plugin.
  static const PPB_VideoDecoder_Dev* GetInterface();

  // Resource overrides.
  virtual PPB_VideoDecoder_Impl* AsPPB_VideoDecoder_Impl();

  // PPB_VideoDecoder implementation.
  bool GetConfigs(PP_VideoConfigElement* requested_configs,
                  PP_VideoConfigElement* matching_configs,
                  uint32_t matching_configs_size,
                  uint32_t* num_of_matching_configs);
  bool Init(PP_VideoConfigElement* dec_config);
  bool Decode(PP_VideoBitstreamBuffer_Dev* bitstream_buffer,
              PP_CompletionCallback callback);
  void AssignGLESBuffers(uint32_t no_of_buffers,
                         PP_GLESBuffer_Dev* buffers);
  void AssignSysmemBuffers(uint32_t no_of_buffers,
                           PP_SysmemBuffer_Dev* buffers);
  void ReusePictureBuffer(int32_t picture_buffer_id);
  bool Flush(PP_CompletionCallback callback);
  bool Abort(PP_CompletionCallback callback);

  // media::VideoDecodeAccelerator::Client implementation.
  virtual void ProvidePictureBuffers(
      uint32 requested_num_of_buffers,
      const gfx::Size& dimensions,
      media::VideoDecodeAccelerator::MemoryType type) OVERRIDE;
  virtual void DismissPictureBuffer(int32 picture_buffer_id) OVERRIDE;
  virtual void PictureReady(const media::Picture& picture) OVERRIDE;
  virtual void NotifyEndOfStream() OVERRIDE;
  virtual void NotifyError(
      media::VideoDecodeAccelerator::Error error) OVERRIDE;
  virtual void NotifyFlushDone() OVERRIDE;
  virtual void NotifyEndOfBitstreamBuffer(int32 buffer_id) OVERRIDE;
  virtual void NotifyAbortDone() OVERRIDE;

 private:
  // This is NULL before initialization, and if this PPB_VideoDecoder_Impl is
  // swapped with another.
  scoped_ptr<PluginDelegate::PlatformVideoDecoder> platform_video_decoder_;

  // Factory to produce our callbacks.
  base::ScopedCallbackFactory<PPB_VideoDecoder_Impl> callback_factory_;

  PP_CompletionCallback abort_callback_;
  PP_CompletionCallback flush_callback_;
  PP_CompletionCallback bitstream_buffer_callback_;

  // Reference to the plugin requesting this interface.
  const PPP_VideoDecoder_Dev* ppp_videodecoder_;

  DISALLOW_COPY_AND_ASSIGN(PPB_VideoDecoder_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_VIDEO_DECODER_IMPL_H_
