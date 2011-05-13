/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_DEV_PPP_VIDEO_DECODER_DEV_H_
#define PPAPI_C_DEV_PPP_VIDEO_DECODER_DEV_H_

#include "ppapi/c/dev/pp_video_dev.h"

#define PPP_VIDEODECODER_DEV_INTERFACE "PPP_VideoDecoder(Dev);0.3"

// PPP_VideoDecoder_Dev structure contains the function pointers that the
// plugin MUST implement to provide services needed by the video decoder
// implementation.
struct PPP_VideoDecoder_Dev {
  // Callback function to provide buffers for the decoded output pictures. If
  // succeeds plugin must provide buffers through AssignPictureBuffers function
  // to the API. If |req_num_of_bufs| matching exactly the specification
  // given in the parameters cannot be allocated decoder should be destroyed.
  //
  // Decoding will not proceed until buffers have been provided.
  //
  // Parameters:
  //  |decoder| is pointer to the Pepper Video Decoder instance.
  //  |req_num_of_bufs| tells how many buffers are needed by the decoder.
  //  |dimensions| tells the dimensions of the buffer to allocate.
  //  |type| specifies whether the buffer lives in system memory or GL texture.
  void (*ProvidePictureBuffers)(
      PP_Resource decoder,
      uint32_t req_num_of_bufs,
      struct PP_Size dimensions,
      enum PP_PictureBufferType_Dev type);

  // Callback function for decoder to deliver unneeded picture buffers back to
  // the plugin.
  //
  // Parameters:
  //  |decoder| is pointer to the Pepper Video Decoder instance.
  //  |picture_buffer| points to the picture buffer that is no longer needed.
  void (*DismissPictureBuffer)(PP_Resource decoder,
                               int32_t picture_buffer_id);

  // Callback function for decoder to deliver decoded pictures ready to be
  // displayed. Decoder expects the plugin to return the buffer back to the
  // decoder through ReusePictureBuffer function in PPB Video Decoder API.
  //
  // Parameters:
  //  |decoder| is pointer to the Pepper Video Decoder instance.
  //  |picture| is the picture that is ready.
  void (*PictureReady)(PP_Resource decoder,
                       struct PP_Picture_Dev picture);

  // Callback function to tell the plugin that decoder has decoded end of stream
  // marker and output all the pictures that should be displayed from the
  // stream.
  //
  // Parameters:
  //  |decoder| is pointer to the Pepper Video Decoder instance.
  void (*EndOfStream)(PP_Resource decoder);

  // Error handler callback for decoder to deliver information about detected
  // errors to the plugin.
  //
  // TODO(vmr): Fill out error result codes and corresponding actions.
  //
  // Parameters:
  //  |decoder| is pointer to the Pepper Video Decoder instance.
  //  |error| error is the enumeration specifying the error.
  void (*NotifyError)(PP_Resource decoder, enum PP_VideoDecodeError_Dev error);
};

#endif  /* PPAPI_C_DEV_PPP_VIDEO_DECODER_DEV_H_ */
