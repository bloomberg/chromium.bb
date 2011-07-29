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

  virtual int32_t Decode(const PP_VideoBitstreamBuffer_Dev* bitstream_buffer,
                         PP_CompletionCallback callback) = 0;
  virtual void AssignPictureBuffers(uint32_t no_of_buffers,
                                    const PP_PictureBuffer_Dev* buffers) = 0;
  virtual void ReusePictureBuffer(int32_t picture_buffer_id) = 0;
  virtual int32_t Flush(PP_CompletionCallback callback) = 0;
  virtual int32_t Reset(PP_CompletionCallback callback) = 0;
  virtual void Destroy() = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_VIDEO_DECODER_API_H_
