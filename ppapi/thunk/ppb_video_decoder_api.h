// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_VIDEO_DECODER_API_H_
#define PPAPI_THUNK_VIDEO_DECODER_API_H_

#include "ppapi/c/dev/ppb_video_decoder_dev.h"

namespace ppapi {
namespace thunk {

class PPB_VideoDecoder_API {
 public:
  virtual ~PPB_VideoDecoder_API() {}

  virtual PP_Bool GetConfigs(const PP_VideoConfigElement* proto_config,
                             PP_VideoConfigElement* matching_configs,
                             uint32_t matching_configs_size,
                             uint32_t* num_of_matching_configs) = 0;
  virtual int32_t Initialize(PP_Resource context_id,
                             const PP_VideoConfigElement* decoder_config,
                             PP_CompletionCallback callback) = 0;
  virtual int32_t Decode(const PP_VideoBitstreamBuffer_Dev* bitstream_buffer,
                         PP_CompletionCallback callback) = 0;
  virtual void AssignGLESBuffers(uint32_t no_of_buffers,
                                 const PP_GLESBuffer_Dev* buffers) = 0;
  virtual void AssignSysmemBuffers(uint32_t no_of_buffers,
                                   const PP_SysmemBuffer_Dev* buffers) = 0;
  virtual void ReusePictureBuffer(int32_t picture_buffer_id) = 0;
  virtual int32_t Flush(PP_CompletionCallback callback) = 0;
  virtual int32_t Abort(PP_CompletionCallback callback) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_VIDEO_DECODER_API_H_
