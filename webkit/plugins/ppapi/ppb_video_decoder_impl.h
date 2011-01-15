// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_VIDEO_DECODER_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_VIDEO_DECODER_IMPL_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/resource.h"

struct PP_VideoDecoderConfig_Dev;
struct PP_VideoCompressedDataBuffer_Dev;
struct PP_VideoUncompressedDataBuffer_Dev;
struct PPB_VideoDecoder_Dev;

namespace webkit {
namespace ppapi {

class PluginInstance;

class PPB_VideoDecoder_Impl : public Resource {
 public:
  PPB_VideoDecoder_Impl(PluginInstance* instance);
  virtual ~PPB_VideoDecoder_Impl();

  // Returns a pointer to the interface implementing PPB_VideoDecoder that is
  // exposed to the plugin.
  static const PPB_VideoDecoder_Dev* GetInterface();

  // Resource overrides.
  virtual PPB_VideoDecoder_Impl* AsPPB_VideoDecoder_Impl();

  // PPB_VideoDecoder implementation.
  bool Init(const PP_VideoDecoderConfig_Dev& decoder_config);
  bool Decode(PP_VideoCompressedDataBuffer_Dev& input_buffer);
  int32_t Flush(PP_CompletionCallback& callback);
  bool ReturnUncompressedDataBuffer(PP_VideoUncompressedDataBuffer_Dev& buffer);

 private:
  // This is NULL before initialization, and if this PPB_VideoDecoder_Impl is
  // swapped with another.
  scoped_ptr<PluginDelegate::PlatformVideoDecoder> platform_video_decoder_;

  DISALLOW_COPY_AND_ASSIGN(PPB_VideoDecoder_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_VIDEO_DECODER_IMPL_H_
