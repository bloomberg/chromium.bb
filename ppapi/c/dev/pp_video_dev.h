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
  // Terminating entry for bitstream format descriptions.
  PP_VIDEOATTR_BITSTREAMFORMATKEY_NONE,
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

  PP_VIDEOATTR_DICTIONARY_COLOR_CORMAT_BASE = 0x1000,
  // Keys for definining attributes of a color buffer. Using these attributes
  // users can define color spaces in terms of red, green, blue and alpha
  // components as well as with combination of luma and chroma values with
  // different subsampling schemes. Also planar, semiplanar and interleaved
  // formats can be described by using the provided keys as instructed.
  //
  // Rules for describing the color planes (1 or more) that constitute the whole
  // picture are:
  //   1. Each plane starts with PP_VIDEOATTR_COLORFORMATKEY_PLANE_PIXEL_SIZE
  //   attribute telling how many bits per pixel the plane contains.
  //   2. PP_VIDEOATTR_COLORFORMATKEY_PLANE_PIXEL_SIZE attribute must be
  //   followed either by
  //     a. Red, green and blue components followed optionally by alpha size
  //     attribute.
  //     OR
  //     b. Luma, blue difference chroma and red difference chroma components as
  //     well as three sampling reference factors that tell how the chroma may
  //     have been subsampled with respect to luma.
  //   3. Description must be terminated with PP_VIDEOATTR_COLORFORMATKEY_NONE
  //   key with no value for attribute.
  //
  // For example, semiplanar YCbCr 4:2:2 (2 planes, one containing 8-bit luma,
  // the other containing two interleaved chroma data components) may be
  // described with the following attributes:
  // {
  //    PP_VIDEOATTR_COLORFORMATKEY_PLANE_PIXEL_SIZE, 8,
  //    PP_VIDEOATTR_COLORFORMATKEY_LUMA_SIZE, 8,
  //    PP_VIDEOATTR_COLORFORMATKEY_PLANE_PIXEL_SIZE, 16,
  //    PP_VIDEOATTR_COLORFORMATKEY_CHROMA_BLUE_SIZE, 8,
  //    PP_VIDEOATTR_COLORFORMATKEY_CHROMA_RED_SIZE, 8,
  //    PP_VIDEOATTR_COLORFORMATKEY_HORIZONTAL_SAMPLING_FACTOR_REFERENCE, 4,
  //    PP_VIDEOATTR_COLORFORMATKEY_CHROMA_HORIZONTAL_SUBSAMPLING_FACTOR, 2,
  //    PP_VIDEOATTR_COLORFORMATKEY_CHROMA_VERTICAL_SUBSAMPLING_FACTOR, 2
  //    PP_VIDEOATTR_DICTIONARY_TERMINATOR
  // }
  //
  // Another example, commonly known 16-bit RGB 565 color format may be
  // specified as follows:
  // {
  //   PP_VIDEOATTR_COLORFORMATKEY_PLANE_PIXEL_SIZE, 16,
  //   PP_VIDEOATTR_COLORFORMATKEY_RED_SIZE, 5,
  //   PP_VIDEOATTR_COLORFORMATKEY_GREEN_SIZE, 6,
  //   PP_VIDEOATTR_COLORFORMATKEY_BLUE_SIZE, 5,
  //   PP_VIDEOATTR_DICTIONARY_TERMINATOR
  // }
  // Total color component bits per pixel in the picture buffer.
  PP_VIDEOATTR_COLORFORMATKEY_PLANE_PIXEL_SIZE,
  // Bits of red per pixel in picture buffer.
  PP_VIDEOATTR_COLORFORMATKEY_RED_SIZE,
  // Bits of green per pixel in picture buffer.
  PP_VIDEOATTR_COLORFORMATKEY_GREEN_SIZE,
  // Bits of blue per pixel in picture buffer.
  PP_VIDEOATTR_COLORFORMATKEY_BLUE_SIZE,
  // Bits of alpha in color buffer.
  PP_VIDEOATTR_COLORFORMATKEY_ALPHA_SIZE,
  // Bits of luma per pixel in color buffer.
  PP_VIDEOATTR_COLORFORMATKEY_LUMA_SIZE,
  // Bits of blue difference chroma (Cb) data in color buffer.
  PP_VIDEOATTR_COLORFORMATKEY_CHROMA_BLUE_SIZE,
  // Bits of blue difference chroma (Cr) data in color buffer.
  PP_VIDEOATTR_COLORFORMATKEY_CHROMA_RED_SIZE,
  // Three keys to describe the subsampling of YCbCr sampled digital video
  // signal. For example, 4:2:2 sampling could be defined by setting:
  //   PP_VIDEOATTR_COLORFORMATKEY_HORIZONTAL_SAMPLING_FACTOR_REFERENCE = 4
  //   PP_VIDEOATTR_COLORFORMATKEY_CHROMINANCE_HORIZONTAL_SUBSAMPLING_FACTOR = 2
  //   PP_VIDEOATTR_COLORFORMATKEY_CHROMINANCE_VERTICAL_SUBSAMPLING_FACTOR = 2
  PP_VIDEOATTR_COLORFORMATKEY_HORIZONTAL_SAMPLING_FACTOR_REFERENCE,
  PP_VIDEOATTR_COLORFORMATKEY_CHROMA_HORIZONTAL_SUBSAMPLING_FACTOR,
  PP_VIDEOATTR_COLORFORMATKEY_CHROMA_VERTICAL_SUBSAMPLING_FACTOR,
  // Base for telling implementation specific information about the optimal
  // number of picture buffers to be provided to the implementation.
  PP_VIDEOATTR_DICTIONARY_PICTUREBUFFER_REQUIREMENTS_BASE = 0x10000,
  // Following two keys are used to signal how many buffers are needed by the
  // implementation as a function of the maximum number of reference frames set
  // by the stream. Number of required buffers is
  //   MAX_REF_FRAMES * REFERENCE_PIC_MULTIPLIER + ADDITIONAL_BUFFERS
  PP_VIDEOATTR_PICTUREBUFFER_REQUIREMENTS_ADDITIONAL_BUFFERS,
  PP_VIDEOATTR_PICTUREBUFFER_REQUIREMENTS_REFERENCE_PIC_MULTIPLIER,
  // If decoder does not support pixel accurate strides for picture buffer, this
  // parameter tells the stride multiple that is needed by the decoder. Plugin
  // must obey the given stride in its picture buffer allocations.
  PP_VIDEOATTR_PICTUREBUFFER_REQUIREMENTS_STRIDE_MULTIPLE
};
PP_COMPILE_ASSERT_ENUM_SIZE_IN_BYTES(PP_VideoAttributeDictionary, 4);
typedef int32_t* PP_VideoConfigElement;

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

// Structure to describe storage properties for a picture.
struct PP_PictureBufferProperties_Dev {
  // Size of the storage (as per width & height in pixels).
  struct PP_Size size;

  // Type of the picture buffer (GLES, system memory).
  enum PP_PictureBufferType_Dev type;

  // Key-attribute pairs defining color format for the buffer.
  PP_VideoConfigElement color_format;
};

// Requested decoder configuration and callback from plugin.
struct PP_VideoDecoderConfig_Dev {
  // Input bitstream properties.
  PP_VideoConfigElement bitstream_properties;
  // Output picture properties.
  struct PP_PictureBufferProperties_Dev picture_properties;
};

// The data structure for video bitstream buffer.
struct PP_VideoBitstreamBuffer_Dev {
  // Buffer to hold the bitstream data. Should be allocated using the PPB_Buffer
  // interface for consistent interprocess behaviour.
  PP_Resource bitstream;

  // Size of the bitstream contained in buffer (in bytes).
  int32_t bitstream_size;

  // Optional pointer for application to associate information with a sample.
  // The pointer will be associated with the resulting decoded picture.
  // Typically applications associate timestamps with buffers.
  void* user_handle;

  // TODO(vmr): Add information about access unit boundaries.
};

// Union for specifying picture data.
union PP_PictureData_Dev {
  // Resource representing system memory from shared memory address space.
  // Use PPB_Buffer_Dev interface to handle this resource.
  PP_Resource sysmem;
  // Structure to define explicitly a GLES2 context.
  struct {
    // Context allocated using. Use PPB_Context3D_Dev interface to handle this
    // resource.
    PP_Resource context;
    // Texture ID in the given context where picture is stored.
    GLuint textureId;
  } gles2_texture;
  // Client-specified id for the picture buffer. By using this value client can
  // keep track of the buffers it has assigned to the video decoder and how they
  // are passed back to it.
  int32_t id;
};

// Structure to describe the decoded output picture for the plug-in along with
// optional metadata associated with the picture.
struct PP_Picture_Dev {
  // Resource that represents the buffer where the picture data is stored.
  // Actual implementation style of the picture buffer may be OpenGL ES texture
  // (allocated using PPB_OpenGLES2_Dev) or system memory (allocated using
  // PPB_Buffer_Dev).
  union PP_PictureData_Dev picture_data;

  // Optional pointer to associated metadata with the picture. Typical
  // information carried over metadata includes timestamps. If there is
  // multiple NAL units each with their own respective metadata, only the
  // metadata from the latest call to Decode will be carried over.
  void* metadata;
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
