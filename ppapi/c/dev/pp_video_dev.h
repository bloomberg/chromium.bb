/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_DEV_PP_VIDEO_DEV_H_
#define PPAPI_C_DEV_PP_VIDEO_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb_opengles2.h"

// Video decoder configuration-related enums.

// NOTE: these must be kept in sync with the versions in
// media/video/video_decode_accelerator.h!

// Video stream profile.
enum PP_VideoDecoder_Profile {
  // Keep the values in this enum unique, as they imply format (h.264 vs. VP8,
  // for example), and keep the values for a particular format grouped together
  // for clarity.
  PP_VIDEODECODER_H264PROFILE_NONE = 0,
  PP_VIDEODECODER_H264PROFILE_BASELINE,
  PP_VIDEODECODER_H264PROFILE_MAIN,
  PP_VIDEODECODER_H264PROFILE_EXTENDED,
  PP_VIDEODECODER_H264PROFILE_HIGH,
  PP_VIDEODECODER_H264PROFILE_HIGH10PROFILE,
  PP_VIDEODECODER_H264PROFILE_HIGH422PROFILE,
  PP_VIDEODECODER_H264PROFILE_HIGH444PREDICTIVEPROFILE,
  PP_VIDEODECODER_H264PROFILE_SCALABLEBASELINE,
  PP_VIDEODECODER_H264PROFILE_SCALABLEHIGH,
  PP_VIDEODECODER_H264PROFILE_STEREOHIGH,
  PP_VIDEODECODER_H264PROFILE_MULTIVIEWHIGH
};

// The data structure for video bitstream buffer.
struct PP_VideoBitstreamBuffer_Dev {
  // Client-specified identifier for the bitstream buffer.
  int32_t id;

  // Buffer to hold the bitstream data. Should be allocated using the PPB_Buffer
  // interface for consistent interprocess behaviour.
  PP_Resource data;

  // Size of the bitstream contained in buffer (in bytes).
  int32_t size;
};

// Struct for specifying texture-backed picture data.
struct PP_PictureBuffer_Dev {
  // Client-specified id for the picture buffer. By using this value client can
  // keep track of the buffers it has assigned to the video decoder and how they
  // are passed back to it.
  int32_t id;

  // Dimensions of the buffer.
  struct PP_Size size;

  // Texture ID in the given context where picture is stored.
  GLuint texture_id;
};

// Structure to describe a decoded output frame.
struct PP_Picture_Dev {
  // ID of the picture buffer where the picture is stored.
  int32_t picture_buffer_id;

  // ID of the bitstream from which this data was decoded.
  int32_t bitstream_buffer_id;
};

// Decoder error codes reported to the plugin.  A reasonable naive
// error handling policy is for the plugin to Destroy() the decoder on error.
// Note: Keep these in sync with media::VideoDecodeAccelerator::Error.
enum PP_VideoDecodeError_Dev {
  // An operation was attempted during an incompatible decoder state.
  PP_VIDEODECODERERROR_ILLEGAL_STATE = 1,
  // Invalid argument was passed to an API method.
  PP_VIDEODECODERERROR_INVALID_ARGUMENT,
  // Encoded input is unreadable.
  PP_VIDEODECODERERROR_UNREADABLE_INPUT,
  // A failure occurred at the browser layer or lower.  Examples of such
  // failures include GPU hardware failures, GPU driver failures, GPU library
  // failures, browser programming errors, and so on.
  PP_VIDEODECODERERROR_PLATFORM_FAILURE
};
PP_COMPILE_ASSERT_ENUM_SIZE_IN_BYTES(PP_VideoDecodeError_Dev, 4);

#endif  /* PPAPI_C_DEV_PP_VIDEO_DEV_H_ */
