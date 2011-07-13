// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_VIDEO_DECODER_DEV_H_
#define PPAPI_CPP_DEV_VIDEO_DECODER_DEV_H_

#include <vector>

#include "ppapi/c/dev/pp_video_dev.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/buffer_dev.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class Context3D_Dev;
class Instance;

// C++ wrapper for the Pepper Video Decoder interface. For more detailed
// documentation refer to the C interfaces.
//
// C++ version of the PPB_VideoDecoder_Dev interface.
class VideoDecoder_Dev : public Resource {
 public:
  // Constructor for the video decoder. Calls the Create on the
  // PPB_VideoDecoder_Dev interface.
  //
  // Parameters:
  //  |instance| is the pointer to the plug-in instance.
  explicit VideoDecoder_Dev(const Instance& instance);
  explicit VideoDecoder_Dev(PP_Resource resource);
  virtual ~VideoDecoder_Dev();

  // PPB_VideoDecoder_Dev implementation.
  int32_t Initialize(const PP_VideoConfigElement* config,
                     const Context3D_Dev& context,
                     CompletionCallback callback);
  void AssignGLESBuffers(const std::vector<PP_GLESBuffer_Dev>& buffers);
  int32_t Decode(const PP_VideoBitstreamBuffer_Dev& bitstream_buffer,
                 CompletionCallback callback);
  void ReusePictureBuffer(int32_t picture_buffer_id);
  int32_t Flush(CompletionCallback callback);
  int32_t Reset(CompletionCallback callback);
  int32_t Destroy(CompletionCallback callback);
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_VIDEO_DECODER_DEV_H_
