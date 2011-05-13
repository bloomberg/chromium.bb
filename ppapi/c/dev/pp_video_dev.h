/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_DEV_PP_VIDEO_DEV_H_
#define PPAPI_C_DEV_PP_VIDEO_DEV_H_

#include "ppapi/c/dev/ppb_opengles_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"

// Enumeration defining global dictionary ranges for various purposes that are
// used to handle the configurations of the video decoder.
enum PP_VideoAttributeDictionary {
  PP_VIDEOATTR_DICTIONARY_TERMINATOR = 0,

  PP_VIDEOATTR_DICTIONARY_BITSTREAM_FORMAT_BASE = 0x100,
  // Array of key/value pairs describing video configuration.
  // It could include any keys from PP_VideoKey. Its last element shall be
  // PP_VIDEOATTR_BITSTREAMFORMATKEY_NONE with no corresponding value.
  // An example:
  // {
  //   PP_VIDEOATTR_BITSTREAMFORMATKEY_FOURCC,      PP_VIDEODECODECID_VP8,
  //   PP_VIDEOATTR_BITSTREAMFORMATKEY_VP8_PROFILE,  (PP_VP8PROFILE_1 |
  //                                        PP_VP8PROFILE_2 |
  //                                        PP_VP8PROFILE_3),
  //   PP_VIDEOATTR_DICTIONARY_TERMINATOR
  // };
  // Keys for defining video bitstream format.
  // Value is type of PP_VideoCodecFourcc. Commonly known attributes values are
  // defined in PP_VideoCodecFourcc enumeration.
  PP_VIDEOATTR_BITSTREAMFORMATKEY_FOURCC,
  // Bitrate in bits/s. Attribute value is 32-bit unsigned integer.
  PP_VIDEOATTR_BITSTREAMFORMATKEY_BITRATE,
  // Width and height of the input video bitstream, if known by the application.
  // Decoder will expect the bitstream to match these values and does memory
  // considerations accordingly.
  PP_VIDEOATTR_BITSTREAMFORMATKEY_WIDTH,
  PP_VIDEOATTR_BITSTREAMFORMATKEY_HEIGHT,
  // Following attributes are applicable only in case of VP8.
  // Key for VP8 profile attribute. Attribute value is bitmask of flags defined
  // in PP_VP8Profile_Dev enumeration.
  PP_VIDEOATTR_BITSTREAMFORMATKEY_VP8_PROFILE,
  // Number of partitions per picture. Attribute value is unsigned 32-bit
  // integer.
  PP_VIDEOATTR_BITSTREAMFORMATKEY_VP8_NUM_OF_PARTITIONS,
  // Following attributes are applicable only in case of H.264.
  // Value is bitmask collection from the flags defined in PP_H264Profile.
  PP_VIDEOATTR_BITSTREAMFORMATKEY_H264_PROFILE,
  // Value is type of PP_H264Level.
  PP_VIDEOATTR_BITSTREAMFORMATKEY_H264_LEVEL,
  // Value is type of PP_H264PayloadFormat_Dev.
  PP_VIDEOATTR_BITSTREAMFORMATKEY_H264_PAYLOADFORMAT,
  // Subset for H.264 features, attribute value 0 signifies unsupported.
  // This is needed in case decoder has partial support for certain profile.
  // Default for features are enabled if they're part of supported profile.
  // H264 tool called Flexible Macroblock Ordering.
  PP_VIDEOATTR_BITSTREAMFORMATKEY_H264_FEATURE_FMO,
  // H264 tool called Arbitrary Slice Ordering.
  PP_VIDEOATTR_BITSTREAMFORMATKEY_H264_FEATURE_ASO,
  // H264 tool called Interlacing.
  PP_VIDEOATTR_BITSTREAMFORMATKEY_H264_FEATURE_INTERLACE,
  // H264 tool called Context-Adaptive Binary Arithmetic Coding.
  PP_VIDEOATTR_BITSTREAMFORMATKEY_H264_FEATURE_CABAC,
  // H264 tool called Weighted Prediction.
  PP_VIDEOATTR_BITSTREAMFORMATKEY_H264_FEATURE_WEIGHTEDPREDICTION,

  PP_VIDEOATTR_DICTIONARY_COLOR_FORMAT_BASE = 0x1000,
  // This specifies the output color format for a decoded frame.
  //
  // Value represents 32-bit RGBA format where each component is 8-bit.
  PP_VIDEOATTR_COLORFORMAT_RGBA
};
PP_COMPILE_ASSERT_ENUM_SIZE_IN_BYTES(PP_VideoAttributeDictionary, 4);
typedef int32_t PP_VideoConfigElement;

enum PP_VideoCodecFourcc {
  PP_VIDEOCODECFOURCC_NONE = 0,
  PP_VIDEOCODECFOURCC_VP8 = 0x00385056,  // a.k.a. Fourcc 'VP8\0'.
  PP_VIDEOCODECFOURCC_H264 = 0x31637661  // a.k.a. Fourcc 'avc1'.
};
PP_COMPILE_ASSERT_ENUM_SIZE_IN_BYTES(PP_VideoCodecFourcc, 4);

// VP8 specific information to be carried over the APIs.
// Enumeration for flags defining supported VP8 profiles.
enum PP_VP8Profile_Dev {
  PP_VP8PROFILE_NONE = 0,
  PP_VP8PROFILE_0 = 1,
  PP_VP8PROFILE_1 = 1 << 1,
  PP_VP8PROFILE_2 = 1 << 2,
  PP_VP8PROFILE_3 = 1 << 3
};
PP_COMPILE_ASSERT_ENUM_SIZE_IN_BYTES(PP_VP8Profile_Dev, 4);

// H.264 specific information to be carried over the APIs.
// Enumeration for flags defining supported H.264 profiles.
enum PP_H264Profile_Dev {
  PP_H264PROFILE_NONE = 0,
  PP_H264PROFILE_BASELINE = 1,
  PP_H264PROFILE_MAIN = 1 << 2,
  PP_H264PROFILE_EXTENDED = 1 << 3,
  PP_H264PROFILE_HIGH = 1 << 4,
  PP_H264PROFILE_HIGH10PROFILE = 1 << 5,
  PP_H264PROFILE_HIGH422PROFILE = 1 << 6,
  PP_H264PROFILE_HIGH444PREDICTIVEPROFILE = 1 << 7,
  PP_H264PROFILE_SCALABLEBASELINE = 1 << 8,
  PP_H264PROFILE_SCALABLEHIGH = 1 << 9,
  PP_H264PROFILE_STEREOHIGH = 1 << 10,
  PP_H264PROFILE_MULTIVIEWHIGH = 1 << 11
};
PP_COMPILE_ASSERT_ENUM_SIZE_IN_BYTES(PP_H264Profile_Dev, 4);

// Enumeration for defining H.264 level of decoder implementation.
enum PP_H264Level_Dev {
  PP_H264LEVEL_NONE = 0,
  PP_H264LEVEL_10 = 1,
  PP_H264LEVEL_1B = PP_H264LEVEL_10 | 1 << 1,
  PP_H264LEVEL_11 = PP_H264LEVEL_1B | 1 << 2,
  PP_H264LEVEL_12 = PP_H264LEVEL_11 | 1 << 3,
  PP_H264LEVEL_13 = PP_H264LEVEL_12 | 1 << 4,
  PP_H264LEVEL_20 = PP_H264LEVEL_13 | 1 << 5,
  PP_H264LEVEL_21 = PP_H264LEVEL_20 | 1 << 6,
  PP_H264LEVEL_22 = PP_H264LEVEL_21 | 1 << 7,
  PP_H264LEVEL_30 = PP_H264LEVEL_22 | 1 << 8,
  PP_H264LEVEL_31 = PP_H264LEVEL_30 | 1 << 9,
  PP_H264LEVEL_32 = PP_H264LEVEL_31 | 1 << 10,
  PP_H264LEVEL_40 = PP_H264LEVEL_32 | 1 << 11,
  PP_H264LEVEL_41 = PP_H264LEVEL_40 | 1 << 12,
  PP_H264LEVEL_42 = PP_H264LEVEL_41 | 1 << 13,
  PP_H264LEVEL_50 = PP_H264LEVEL_42 | 1 << 14,
  PP_H264LEVEL_51 = PP_H264LEVEL_50 | 1 << 15
};
PP_COMPILE_ASSERT_ENUM_SIZE_IN_BYTES(PP_H264Level_Dev, 4);

// Enumeration to describe which payload format is used within the exchanged
// bitstream buffers.
enum PP_H264PayloadFormat_Dev {
  PP_H264PAYLOADFORMAT_NONE = 0,
  // NALUs separated by Start Code.
  PP_H264PAYLOADFORMAT_BYTESTREAM = 1,
  // Exactly one raw NALU per buffer.
  PP_H264PAYLOADFORMAT_ONE_NALU_PER_BUFFER = 1 << 1,
  // NALU separated by 1-byte interleaved length field.
  PP_H264PAYLOADFORMAT_ONE_BYTE_INTERLEAVED_LENGTH = 1 << 2,
  // NALU separated by 2-byte interleaved length field.
  PP_H264PAYLOADFORMAT_TWO_BYTE_INTERLEAVED_LENGTH = 1 << 3,
  // NALU separated by 4-byte interleaved length field.
  PP_H264PAYLOADFORMAT_FOUR_BYTE_INTERLEAVED_LENGTH = 1 << 4
};
PP_COMPILE_ASSERT_ENUM_SIZE_IN_BYTES(PP_H264PayloadFormat_Dev, 4);

// Enumeration to determine which type of memory for buffer is used.
enum PP_PictureBufferType_Dev {
  PP_PICTUREBUFFERTYPE_NONE = 0,
  // System memory a.k.a. RAM.
  PP_PICTUREBUFFERTYPE_SYSTEM = 1,
  // GLES texture allocated using OpenGL ES APIs.
  PP_PICTUREBUFFERTYPE_GLESTEXTURE = 1 << 1
};
PP_COMPILE_ASSERT_ENUM_SIZE_IN_BYTES(PP_PictureBufferType_Dev, 4);

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

// Struct for specifying data about buffer.
struct PP_BufferInfo_Dev {
  // Client-specified id for the picture buffer. By using this value client can
  // keep track of the buffers it has assigned to the video decoder and how they
  // are passed back to it.
  int32_t id;

  // Dimensions of the buffer.
  struct PP_Size size;
};

// Struct for specifying texture-backed picture data.
struct PP_GLESBuffer_Dev {
  // Context allocated using PPB_Context3D_Dev.
  PP_Resource context;

  // Texture ID in the given context where picture is stored.
  GLuint texture_id;

  // Information about the buffer.
  struct PP_BufferInfo_Dev info;
};

// Struct for specifying sysmem-backed picture data.
struct PP_SysmemBuffer_Dev {
  // Resource representing system memory from shared memory address space.
  // Use PPB_Buffer_Dev interface to handle this resource.
  PP_Resource data;

  // Information about the buffer.
  struct PP_BufferInfo_Dev info;
};

// Structure to describe a decoded output frame.
// The decoded pixels will always begin flush with the upper left-hand corner
// of the buffer (0, 0).
struct PP_Picture_Dev {
  // ID of the picture buffer where the picture is stored.
  int32_t picture_buffer_id;

  // ID of the bitstream from which this data was decoded.
  int32_t bitstream_buffer_id;

  // Visible size of the picture.
  // This describes the dimensions of the picture that is intended to be
  // displayed from the decoded output.
  struct PP_Size visible_size;

  // Decoded size of the picture.
  // This describes the dimensions of the decoded output. This may be slightly
  // larger than the visible size because the stride is sometimes larger than
  // the width of the output. The plugin should handle rendering the frame
  // appropriately with respect to the sizes.
  struct PP_Size decoded_size;
};

// Enumeration for error events that may be reported through
// PP_VideoDecodeErrorHandler_Func_Dev callback function to the plugin. Default
// error handling functionality expected from the plugin is to Flush and Destroy
// the decoder.
enum PP_VideoDecodeError_Dev {
  PP_VIDEODECODEERROR_NONE = 0,
  // Decoder has not been initialized and configured properly.
  PP_VIDEODECODEERROR_UNINITIALIZED,
  // Decoder does not support feature of configuration or bitstream.
  PP_VIDEODECODEERROR_UNSUPPORTED,
  // Decoder did not get valid input.
  PP_VIDEODECODERERROR_INVALIDINPUT,
  // Failure in memory allocation or mapping.
  PP_VIDEODECODERERROR_MEMFAILURE,
  // Decoder was given bitstream that would result in output pictures but it
  // has not been provided buffers to do all this.
  PP_VIDEODECODEERROR_INSUFFICIENT_BUFFERS,
  // Decoder cannot continue operation due to insufficient resources for the
  // current configuration.
  PP_VIDEODECODEERROR_INSUFFICIENTRESOURCES,
  // Decoder hardware has reported hardware error.
  PP_VIDEODECODEERROR_HARDWARE
};
PP_COMPILE_ASSERT_ENUM_SIZE_IN_BYTES(PP_VideoDecodeError_Dev, 4);

#endif  /* PPAPI_C_DEV_PP_VIDEO_DEV_H_ */
