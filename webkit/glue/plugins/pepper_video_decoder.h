// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_VIDEO_DECODER_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_VIDEO_DECODER_H_

#include <queue>

#include "base/scoped_ptr.h"
#include "third_party/ppapi/c/pp_video.h"
#include "third_party/ppapi/c/ppb_video_decoder.h"
#include "webkit/glue/plugins/pepper_plugin_delegate.h"
#include "webkit/glue/plugins/pepper_resource.h"

namespace pepper {

class PluginInstance;

class VideoDecoder : public Resource {
 public:
  VideoDecoder(PluginInstance* instance);
  virtual ~VideoDecoder();

  // Returns a pointer to the interface implementing PPB_VideoDecoder that is
  // exposed to the plugin.
  static const PPB_VideoDecoder* GetInterface();

  // Resource overrides.
  VideoDecoder* AsVideoDecoder() { return this; }

  PluginInstance* instance() { return instance_.get(); }

  // PPB_VideoDecoder implementation.
  bool Init(const PP_VideoDecoderConfig& decoder_config);
  bool Decode(PP_VideoCompressedDataBuffer& input_buffer);
  int32_t Flush(PP_CompletionCallback& callback);
  bool ReturnUncompressedDataBuffer(PP_VideoUncompressedDataBuffer& buffer);

 private:
  // This is NULL before initialization, and if this VideoDecoder is
  // swapped with another.
  scoped_ptr<PluginDelegate::PlatformVideoDecoder> platform_video_decoder_;
  scoped_refptr<PluginInstance> instance_;
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_VIDEO_DECODER_H_
