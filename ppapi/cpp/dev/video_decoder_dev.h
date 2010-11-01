// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_VIDEO_DECODER_DEV_H_
#define PPAPI_CPP_DEV_VIDEO_DECODER_DEV_H_

#include "ppapi/c/dev/pp_video_dev.h"
#include "ppapi/c/dev/ppb_video_decoder_dev.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class Instance;

// Provides access to video decoders.
class VideoDecoder_Dev : public Resource {
 public:
  // Creates an is_null() VideoDecoder object.
  VideoDecoder_Dev() {}

  explicit VideoDecoder_Dev(PP_Resource resource);

  VideoDecoder_Dev(const Instance& instance,
                   const PP_VideoDecoderConfig_Dev& decoder_config);
  VideoDecoder_Dev(const VideoDecoder_Dev& other);

  VideoDecoder_Dev& operator=(const VideoDecoder_Dev& other);
  void swap(VideoDecoder_Dev& other);

  // PPB_VideoDecoder methods:
  static bool GetConfig(const Instance& instance,
                        PP_VideoCodecId_Dev codec,
                        PP_VideoConfig_Dev* configs,
                        int32_t config_size,
                        int32_t* num_config);

  bool Decode(PP_VideoCompressedDataBuffer_Dev& input_buffer);

  int32_t Flush(PP_CompletionCallback callback);

  bool ReturnUncompressedDataBuffer(PP_VideoUncompressedDataBuffer_Dev& buffer);
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_VIDEO_DECODER_DEV_H_
