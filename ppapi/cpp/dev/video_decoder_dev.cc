// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/video_decoder_dev.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/common.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_VideoDecoder_Dev>() {
  return PPB_VIDEODECODER_DEV_INTERFACE;
}

}  // namespace

VideoDecoder_Dev::VideoDecoder_Dev(PP_Resource resource) : Resource(resource) {
}

VideoDecoder_Dev::VideoDecoder_Dev(
    const Instance& instance,
    const PP_VideoDecoderConfig_Dev& decoder_config) {
  if (!has_interface<PPB_VideoDecoder_Dev>())
    return;
  PassRefFromConstructor(get_interface<PPB_VideoDecoder_Dev>()->Create(
      instance.pp_instance(), &decoder_config));
}

VideoDecoder_Dev::VideoDecoder_Dev(const VideoDecoder_Dev& other)
    : Resource(other) {
}

// static
bool VideoDecoder_Dev::GetConfig(const Instance& instance,
                             PP_VideoCodecId_Dev codec,
                             PP_VideoConfig_Dev* configs,
                             int32_t config_size,
                             int32_t* num_config) {
  if (!has_interface<PPB_VideoDecoder_Dev>())
    return false;
  return PPBoolToBool(get_interface<PPB_VideoDecoder_Dev>()->GetConfig(
      instance.pp_instance(), codec, configs, config_size, num_config));
}

bool VideoDecoder_Dev::Decode(PP_VideoCompressedDataBuffer_Dev& input_buffer) {
  if (!has_interface<PPB_VideoDecoder_Dev>() || !pp_resource())
    return false;
  return PPBoolToBool(get_interface<PPB_VideoDecoder_Dev>()->Decode(
      pp_resource(), &input_buffer));
}

int32_t VideoDecoder_Dev::Flush(PP_CompletionCallback callback) {
  if (!has_interface<PPB_VideoDecoder_Dev>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_VideoDecoder_Dev>()->Flush(pp_resource(), callback);
}

bool VideoDecoder_Dev::ReturnUncompressedDataBuffer(
    PP_VideoUncompressedDataBuffer_Dev& buffer) {
  if (!has_interface<PPB_VideoDecoder_Dev>() || !pp_resource())
    return false;
  return PPBoolToBool(
      get_interface<PPB_VideoDecoder_Dev>()->ReturnUncompressedDataBuffer(
          pp_resource(), &buffer));
}

}  // namespace pp
