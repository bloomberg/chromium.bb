// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_DEV_PPB_VIDEO_DECODER_DEV_H_
#define PPAPI_C_DEV_PPB_VIDEO_DECODER_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/dev/pp_video_dev.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_completion_callback.h"

#define PPB_VIDEODECODER_DEV_INTERFACE "PPB_VideoDecoder(Dev);0.2"

struct PPB_VideoDecoder_Dev {
  // Queries capability of the decoder for |codec|.
  // |codec| is the requested codec id.
  // |configs| is a pointer to a buffer containing |config_size| elements.
  // The number of configurations is returned in |num_config|. Element 0 through
  // |num_config| - 1 of |configs| are filled in with valid PP_VideoConfig's.
  // No more than |config_size| PP_VideoConfig's will be returned even if more
  // are available on the device.
  // When this function is called with |configs| = NULL, then no configurations
  // are returned, but the total number of configurations available will be
  // returned in |num_config|.
  //
  // Returns PP_TRUE on success, PP_FALSE otherwise.
  // NOTE: browser owns the memory of all PP_VideoConfig's.
  PP_Bool (*GetConfig)(PP_Instance instance,
                       enum PP_VideoCodecId_Dev codec,
                       PP_VideoConfig_Dev* configs,
                       int32_t config_size,
                       int32_t* num_config);

  // Creates a video decoder with requested |decoder_config|.
  // |input_format| in |decoder_config| specifies the format of input access
  // unit, with PP_VIDEOKEY_CODECID and PP_VIDEOKEY_PAYLOADFORMAT required.
  // Plugin has the option to specify codec profile/level and other
  // information such as PP_VIDEOKEY_ACCELERATION, to let browser choose
  // the most appropriate decoder.
  //
  // |output_format| in |decoder_config| specifies desired decoded frame buffer
  // format, with PP_VIDEOKEY_COLORTYPE and PP_VIDEOKEY_SURFACETYPE required.
  //
  // |output_callback| in |decoder_config| specifies the callback function
  // for decoder to deliver decoded frame buffers. Decoder shall retain it.
  //
  // |input_callback| in |decoder_config| specifies the callback function
  // for decoder to return compressed data buffers to plugin. Decoder shall
  // retain it. When plugin doesn't expect buffer recycling, it shall set
  // |input_callback| to NULL. In this case, plugin shall allocate buffer via
  // |MemAlloc| in PPB_Core interface, and decoder will free buffer via
  // |MemFree| in the same API.
  //
  // |event_handler| in |decoder_config| specifies the function for decoder
  // to deliver events to plugin. Decoder shall retain it.
  //
  // The created decoder is returned as PP_Resource. NULL means failure.
  PP_Resource (*Create)(PP_Instance instance,
                        const struct PP_VideoDecoderConfig_Dev* decoder_config);

  // Sends bit stream in |input_buffer| to the decoder.
  // This is a non-blocking call.
  // The decoded frame will be returned by decoder calling |output_callback|
  // provided by plugin during creation of decoder.
  // The input data buffer is returned to plugin by decoder only when plugin
  // provides |input_callback|.
  // Returns PP_TRUE on decoder successfully accepting buffer, PP_FALSE
  // otherwise.
  //
  PP_Bool (*Decode)(PP_Resource decoder,
                    struct PP_VideoCompressedDataBuffer_Dev* input_buffer);

  // Requests the decoder to flush its input and output buffers. Once done with
  // flushing, the decode will call the |callback|.
  int32_t (*Flush)(PP_Resource decoder, struct PP_CompletionCallback callback);

  // Plugin sends uncompressed data buffers to the decoder.
  // Returns PP_TRUE on decoder successfully accepting the buffer, PP_FALSE
  // otherwise.
  PP_Bool (*ReturnUncompressedDataBuffer)(PP_Resource decoder,
      struct PP_VideoUncompressedDataBuffer_Dev* buffer);
};

#endif  // PPAPI_C_DEV_PPB_VIDEO_DECODER_DEV_H_
