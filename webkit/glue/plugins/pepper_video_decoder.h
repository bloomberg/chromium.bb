// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_VIDEO_DECODER_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_VIDEO_DECODER_H_

#include "base/scoped_ptr.h"
#include "webkit/glue/plugins/pepper_plugin_delegate.h"
#include "webkit/glue/plugins/pepper_resource.h"

struct PP_VideoDecoderConfig_Dev;
struct PP_VideoCompressedDataBuffer_Dev;
struct PP_VideoUncompressedDataBuffer_Dev;
struct PPB_VideoDecoder_Dev;

namespace pepper {

class PluginInstance;

class VideoDecoder : public Resource {
 public:
  VideoDecoder(PluginInstance* instance);
  virtual ~VideoDecoder();

  // Returns a pointer to the interface implementing PPB_VideoDecoder that is
  // exposed to the plugin.
  static const PPB_VideoDecoder_Dev* GetInterface();

  // Resource overrides.
  VideoDecoder* AsVideoDecoder() { return this; }

  PluginInstance* instance() { return instance_.get(); }

  // PPB_VideoDecoder implementation.
  bool Init(const PP_VideoDecoderConfig_Dev& decoder_config);
  bool Decode(PP_VideoCompressedDataBuffer_Dev& input_buffer);
  int32_t Flush(PP_CompletionCallback& callback);
  bool ReturnUncompressedDataBuffer(PP_VideoUncompressedDataBuffer_Dev& buffer);

 private:
  // This is NULL before initialization, and if this VideoDecoder is
  // swapped with another.
  scoped_ptr<PluginDelegate::PlatformVideoDecoder> platform_video_decoder_;
  scoped_refptr<PluginInstance> instance_;
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_VIDEO_DECODER_H_
