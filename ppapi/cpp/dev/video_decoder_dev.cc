// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/video_decoder_dev.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/common.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace {

DeviceFuncs<PPB_VideoDecoder_Dev> video_decoder_f(
    PPB_VIDEODECODER_DEV_INTERFACE);

}  // namespace


namespace pp {

VideoDecoder_Dev::VideoDecoder_Dev(PP_Resource resource) : Resource(resource) {
}

VideoDecoder_Dev::VideoDecoder_Dev(
    const Instance& instance,
    const PP_VideoDecoderConfig_Dev& decoder_config) {
  if (!video_decoder_f)
    return;
  PassRefFromConstructor(video_decoder_f->Create(instance.pp_instance(),
                                                 &decoder_config));
}

VideoDecoder_Dev::VideoDecoder_Dev(const VideoDecoder_Dev& other)
    : Resource(other) {
}

VideoDecoder_Dev& VideoDecoder_Dev::operator=(const VideoDecoder_Dev& other) {
  VideoDecoder_Dev copy(other);
  swap(copy);
  return *this;
}

void VideoDecoder_Dev::swap(VideoDecoder_Dev& other) {
  Resource::swap(other);
}

// static
bool VideoDecoder_Dev::GetConfig(const Instance& instance,
                             PP_VideoCodecId_Dev codec,
                             PP_VideoConfig_Dev* configs,
                             int32_t config_size,
                             int32_t* num_config) {
  if (!video_decoder_f)
    return false;
  return PPBoolToBool(video_decoder_f->GetConfig(instance.pp_instance(),
                                                 codec,
                                                 configs,
                                                 config_size,
                                                 num_config));
}

bool VideoDecoder_Dev::Decode(PP_VideoCompressedDataBuffer_Dev& input_buffer) {
  if (!video_decoder_f || !pp_resource())
    return false;
  return PPBoolToBool(video_decoder_f->Decode(pp_resource(), &input_buffer));
}

int32_t VideoDecoder_Dev::Flush(PP_CompletionCallback callback) {
  if (!video_decoder_f)
    return PP_ERROR_NOINTERFACE;
  return video_decoder_f->Flush(pp_resource(), callback);
}

bool VideoDecoder_Dev::ReturnUncompressedDataBuffer(
    PP_VideoUncompressedDataBuffer_Dev& buffer) {
  if (!video_decoder_f || !pp_resource())
    return false;
  return PPBoolToBool(
      video_decoder_f->ReturnUncompressedDataBuffer(pp_resource(),
                                                    &buffer));
}

}  // namespace pp
