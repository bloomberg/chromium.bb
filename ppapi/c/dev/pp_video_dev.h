// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_DEV_PP_VIDEO_DEV_H_
#define PPAPI_C_DEV_PP_VIDEO_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

enum PP_VideoKey_Dev {
  PP_VIDEOKEY_NONE = 0,
  // Value is type of PP_VideoCodecId.
  PP_VIDEOKEY_CODECID,
  // Value is type of PP_VideoOperation.
  PP_VIDEOKEY_OPERATION,
  // Value is type of PP_VideoCodecProfile.
  PP_VIDEOKEY_CODECPROFILE,
  // Value is type of PP_VideoCodecLevel.
  PP_VIDEOKEY_CODECLEVEL,
  // Value is 0 or 1.
  PP_VIDEOKEY_ACCELERATION,
  // Value is type of PP_VideoPayloadFormat.
  PP_VIDEOKEY_PAYLOADFORMAT,
  // Value is type of PP_VideoFrameColorType.
  PP_VIDEOKEY_COLORTYPE,
  // Value is type of PP_VideoFrameSurfaceType.
  PP_VIDEOKEY_SURFACETYPE,
  // Value is type of PP_VideoFrameInfoFlag.
  PP_VIDEOKEY_FRAMEINFOFLAG,

  // Subset for H.264 features, value of 1 means supported. This is needed in
  // case decoder has partial support for certain profile.
  PP_VIDEOKEY_H264FEATURE_FMO = 0x100,
  PP_VIDEOKEY_H264FEATURE_ASO,
  PP_VIDEOKEY_H264FEATURE_INTERLACE,
  PP_VIDEOKEY_H264FEATURE_CABAC,
  PP_VIDEOKEY_H264FEATURE_WEIGHTEDPREDICTION
};

enum PP_VideoDecoderEvent_Dev {
  PP_VIDEODECODEREVENT_NONE = 0,
  // Signaling that an error has been hit.
  PP_VIDEODECODEREVENT_ERROR,
  // Signaling new width/height of video frame
  PP_VIDEODECODEREVENT_NEWDIMENSION,
  // Signaling new cropping rectangle
  PP_VIDEODECODEREVENT_NEWCROP
};

enum PP_VideoDecodeError_Dev {
  PP_VIDEODECODEERROR_NONE = 0,
  PP_VIDEODECODEERROR_NOTSUPPORTED,
  PP_VIDEODECODEERROR_INSUFFICIENTRESOURCES,
  PP_VIDEODECODEERROR_UNDEFINED,
  PP_VIDEODECODEERROR_BADINPUT,
  PP_VIDEODECODEERROR_HARDWARE
};

enum PP_VideoCodecId_Dev {
  PP_VIDEODECODECID_NONE = 0,
  PP_VIDEODECODECID_H264,
  PP_VIDEODECODECID_VC1,
  PP_VIDEODECODECID_MPEG2,
  PP_VIDEODECODECID_VP8
};

enum PP_VideoOperation_Dev {
  PP_VIDEOOPERATION_NONE = 0,
  PP_VIDEOOPERATION_DECODE,
  PP_VIDEOOPERATION_ENCODE
};

enum PP_VideoCodecProfile_Dev {
  PP_VIDEOCODECPROFILE_NONE = 0,
  PP_VIDEOCODECPROFILE_H264_BASELINE,
  PP_VIDEOCODECPROFILE_H264_MAIN,
  PP_VIDEOCODECPROFILE_H264_EXTENDED,
  PP_VIDEOCODECPROFILE_H264_HIGH,
  PP_VIDEOCODECPROFILE_H264_SCALABLEBASELINE,
  PP_VIDEOCODECPROFILE_H264_SCALABLEHIGH,
  PP_VIDEOCODECPROFILE_H264_STEREOHIGH,
  PP_VIDEOCODECPROFILE_H264_MULTIVIEWHIGH,

  PP_VIDEOCODECPROFILE_VC1_SIMPLE = 0x40,
  PP_VIDEOCODECPROFILE_VC1_MAIN,
  PP_VIDEOCODECPROFILE_VC1_ADVANCED,

  PP_VIDEOCODECPROFILE_MPEG2_SIMPLE = 0x80,
  PP_VIDEOCODECPROFILE_MPEG2_MAIN,
  PP_VIDEOCODECPROFILE_MPEG2_SNR,
  PP_VIDEOCODECPROFILE_MPEG2_SPATIAL,
  PP_VIDEOCODECPROFILE_MPEG2_HIGH
};

enum PP_VideoCodecLevel_Dev {
  PP_VIDEOCODECLEVEL_NONE = 0,
  PP_VIDEOCODECLEVEL_H264_10,
  PP_VIDEOCODECLEVEL_H264_1B,
  PP_VIDEOCODECLEVEL_H264_11,
  PP_VIDEOCODECLEVEL_H264_12,
  PP_VIDEOCODECLEVEL_H264_13,
  PP_VIDEOCODECLEVEL_H264_20,
  PP_VIDEOCODECLEVEL_H264_21,
  PP_VIDEOCODECLEVEL_H264_22,
  PP_VIDEOCODECLEVEL_H264_30,
  PP_VIDEOCODECLEVEL_H264_31,
  PP_VIDEOCODECLEVEL_H264_32,
  PP_VIDEOCODECLEVEL_H264_40,
  PP_VIDEOCODECLEVEL_H264_41,
  PP_VIDEOCODECLEVEL_H264_42,
  PP_VIDEOCODECLEVEL_H264_50,
  PP_VIDEOCODECLEVEL_H264_51,

  PP_VIDEOCODECLEVEL_VC1_LOW = 0x40,
  PP_VIDEOCODECLEVEL_VC1_MEDIUM,
  PP_VIDEOCODECLEVEL_VC1_HIGH,
  PP_VIDEOCODECLEVEL_VC1_L0,
  PP_VIDEOCODECLEVEL_VC1_L1,
  PP_VIDEOCODECLEVEL_VC1_L2,
  PP_VIDEOCODECLEVEL_VC1_L3,
  PP_VIDEOCODECLEVEL_VC1_L4,

  PP_VIDEOCODECLEVEL_MPEG2_LOW = 0x80,
  PP_VIDEOCODECLEVEL_MPEG2_MAIN,
  PP_VIDEOCODECLEVEL_MPEG2_HIGH1440,
  PP_VIDEOCODECLEVEL_MPEG2_HIGH
};

enum PP_VideoPayloadFormat_Dev {
  PP_VIDEOPAYLOADFORMAT_NONE = 0,
  PP_VIDEOPAYLOADFORMAT_BYTESTREAM,
  PP_VIDEOPAYLOADFORMAT_RTPPAYLOAD
};

enum PP_VideoFrameColorType_Dev {
  PP_VIDEOFRAMECOLORTYPE_NONE = 0,
  PP_VIDEOFRAMECOLORTYPE_RGB565,
  PP_VIDEOFRAMECOLORTYPE_ARGB8888,
  PP_VIDEOFRAMECOLORTYPE_YUV,
  PP_VIDEOFRAMECOLORTYPE_Monochrome,
  PP_VIDEOFRAMECOLORTYPE_YUV420PLANAR,
  PP_VIDEOFRAMECOLORTYPE_YUV422PLANAR,
  PP_VIDEOFRAMECOLORTYPE_YUV444PLANAR
};

enum PP_VideoFrameSurfaceType_Dev {
  PP_VIDEOFRAMESURFACETYPE_NONE = 0,
  PP_VIDEOFRAMESURFACETYPE_SYSTEMMEMORY,
  PP_VIDEOFRAMESURFACETYPE_GLTEXTURE,
  PP_VIDEOFRAMESURFACETYPE_PIXMAP
};

enum PP_VideoFrameInfoFlag_Dev {
  PP_VIDEOFRAMEINFOFLAG_NONE = 0,
  // Indicate this is the end of stream. Used by both plugin and browser.
  PP_VIDEOFRAMEINFOFLAG_EOS = 1 << 0,
  // Decode the frame only, don't return decoded frame. Used by plugin.
  PP_VIDEOFRAMEINFOFLAG_NOEMIT = 1 << 1,
  // Indicate this is an anchor frame. Used by plugin.
  PP_VIDEOFRAMEINFOFLAG_SYNCFRAME = 1 << 2,
  // Indicate the decoded frame has data corruption. Used by browser.
  PP_VIDEOFRAMEINFOFLAG_DATACORRUPT = 1 << 3
};

enum PP_VideoFrameBufferConst_Dev {
  // YUV formats
  PP_VIDEOFRAMEBUFFER_YPLANE = 0,
  PP_VIDEOFRAMEBUFFER_UPLANE = 1,
  PP_VIDEOFRAMEBUFFER_VPLANE = 2,
  PP_VIDEOFRAMEBUFFER_NUMBERYUVPLANES = 3,

  // RGBA formats
  PP_VIDEOFRAMEBUFFER_RGBAPLANE = 0,
  PP_VIDEOFRAMEBUFFER_NUMBERRGBAPLANES = 1,

  PP_VIDEOFRAMEBUFFER_MAXNUMBERPLANES = 4
};

typedef int64_t PP_VideoDecodeData_Dev;

// Array of key/value pairs describing video configuration.
// It could include any keys from PP_VideoKey. Its last element shall be
// PP_VIDEOKEY_NONE with no corresponding value.
// An example:
// {
//   PP_VIDEOKEY_CODECID,      PP_VIDEODECODECID_H264,
//   PP_VIDEOKEY_OPERATION,    PP_VIDEOOPERATION_DECODE,
//   PP_VIDEOKEY_CODECPROFILE, PP_VIDEOCODECPROFILE_H264_HIGH,
//   PP_VIDEOKEY_CODECLEVEL,   PP_VIDEOCODECLEVEL_H264_41,
//   PP_VIDEOKEY_ACCELERATION, 1
//   PP_VIDEOKEY_NONE,
// };
typedef int32_t* PP_VideoConfig_Dev;
typedef int32_t PP_VideoConfigElement_Dev;

// The data structure for compressed data buffer.
struct PP_VideoCompressedDataBuffer_Dev {
  // The buffer is created through PPB_Buffer API.
  // TODO(wjia): would uint8_t* be good, too?
  PP_Resource buffer;
  // number of bytes with real data in the buffer.
  int32_t filled_size;

  // Time stamp of the frame in microsecond.
  uint64_t time_stamp_us;

  // Bit mask of PP_VideoFrameInfoFlag.
  uint32_t flags;
};

struct PP_VideoFrameBuffer_Dev {
  union {
    struct {
      int32_t planes;
      struct {
        int32_t width;
        int32_t height;
        int32_t stride;

        // TODO(wjia): uint8* would be better for some cases.
        PP_Resource buffer;
      } data_plane[PP_VIDEOFRAMEBUFFER_MAXNUMBERPLANES];
    } sys_mem;

    // Handle for pixmap, gl texture, etc.
    void* handle;
  } buffer;

  // Storage for decoder to save some private data. It could be useful when
  // plugin returns frame buffer to decoder.
  void* private_handle;
};

struct PP_VideoUncompressedDataBuffer_Dev {
  PP_VideoConfig_Dev format;
  struct PP_VideoFrameBuffer_Dev buffer;

  // Time stamp of the frame in microsecond.
  uint64_t time_stamp_us;

  // Bit mask of PP_VideoFrameInfoFlag.
  uint32_t flags;

  // Output from decoder, indicating the decoded frame has error pixels. This
  // could be resulted from corrupted input bit stream and error concealment
  // in decoding.  PP_TRUE indicates error.
  // TODO(wjia): add more info about error pixels, such as error MB map, etc.
  PP_Bool error;
};

// Plugin callback for decoder to deliver decoded frame buffers.
// |format| in |buffer| specifies the format of decoded frame, with
// PP_VIDEOKEY_COLORTYPE and PP_VIDEOKEY_SURFACETYPE required.
typedef void (*PP_VideoDecodeOutputCallback_Func_Dev)(
    PP_Instance instance,
    struct PP_VideoUncompressedDataBuffer_Dev* buffer);

// Plugin callback for decoder to return input data buffers.
// Plugin can optionally provide this callback only when it wants to recycle
// input data buffers.
typedef void (*PP_VideoDecodeInputCallback_Func_Dev)(
    PP_Instance instance,
    struct PP_VideoCompressedDataBuffer_Dev* buffer);

// Event handling Function for decoder to deliver events to plugin.
// The correspondence between event and data1, data2:
// When event == PP_VIDEODECODEREVENT_ERROR,
//   data1 is type of PP_VideoDecodeError id and data2 is ignored;
// When event == PP_VIDEODECODEREVENT_NEWDIMENSION,
//   data1 is type of PP_Size*, data2 is ignored;
// When event == PP_VIDEODECODEREVENT_NEWCROP,
//   data1 is type of PP_Rect*, data2 is ignored;
typedef void (*PP_VideoDecodeEventHandler_Func_Dev)(
    PP_Instance instance,
    enum PP_VideoDecoderEvent_Dev event,
    PP_VideoDecodeData_Dev data1,
    PP_VideoDecodeData_Dev data2);

// Requested decoder configuration and callback from plugin.
struct PP_VideoDecoderConfig_Dev {
  PP_VideoConfig_Dev input_format;
  PP_VideoConfig_Dev output_format;
  PP_VideoDecodeOutputCallback_Func_Dev output_callback;
  PP_VideoDecodeInputCallback_Func_Dev input_callback;
  PP_VideoDecodeEventHandler_Func_Dev event_handler;
};

#endif  // PPAPI_C_DEV_PP_VIDEO_DEV_H_
