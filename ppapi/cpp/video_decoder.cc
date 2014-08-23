// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/video_decoder.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_video_decoder.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <>
const char* interface_name<PPB_VideoDecoder_0_1>() {
  return PPB_VIDEODECODER_INTERFACE_0_1;
}

template <>
const char* interface_name<PPB_VideoDecoder_0_2>() {
  return PPB_VIDEODECODER_INTERFACE_0_2;
}

}  // namespace

VideoDecoder::VideoDecoder() {
}

VideoDecoder::VideoDecoder(const InstanceHandle& instance) {
  if (has_interface<PPB_VideoDecoder_0_1>()) {
    PassRefFromConstructor(
        get_interface<PPB_VideoDecoder_0_1>()->Create(instance.pp_instance()));
  }
}

VideoDecoder::VideoDecoder(const VideoDecoder& other) : Resource(other) {
}

int32_t VideoDecoder::Initialize(const Graphics3D& context,
                                 PP_VideoProfile profile,
                                 PP_HardwareAcceleration acceleration,
                                 const CompletionCallback& cc) {
  if (has_interface<PPB_VideoDecoder_0_2>()) {
    return get_interface<PPB_VideoDecoder_0_2>()->Initialize(
        pp_resource(),
        context.pp_resource(),
        profile,
        acceleration,
        cc.pp_completion_callback());
  }
  if (has_interface<PPB_VideoDecoder_0_1>()) {
    if (acceleration == PP_HARDWAREACCELERATION_NONE)
      return cc.MayForce(PP_ERROR_NOTSUPPORTED);
    return get_interface<PPB_VideoDecoder_0_1>()->Initialize(
        pp_resource(),
        context.pp_resource(),
        profile,
        acceleration == PP_HARDWAREACCELERATION_WITHFALLBACK
            ? PP_TRUE
            : PP_FALSE,
        cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t VideoDecoder::Decode(uint32_t decode_id,
                             uint32_t size,
                             const void* buffer,
                             const CompletionCallback& cc) {
  if (has_interface<PPB_VideoDecoder_0_2>()) {
    return get_interface<PPB_VideoDecoder_0_2>()->Decode(
        pp_resource(), decode_id, size, buffer, cc.pp_completion_callback());
  }
  if (has_interface<PPB_VideoDecoder_0_1>()) {
    return get_interface<PPB_VideoDecoder_0_1>()->Decode(
        pp_resource(), decode_id, size, buffer, cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t VideoDecoder::GetPicture(
    const CompletionCallbackWithOutput<PP_VideoPicture>& cc) {
  if (has_interface<PPB_VideoDecoder_0_2>()) {
    return get_interface<PPB_VideoDecoder_0_2>()->GetPicture(
        pp_resource(), cc.output(), cc.pp_completion_callback());
  }
  if (has_interface<PPB_VideoDecoder_0_1>()) {
    return get_interface<PPB_VideoDecoder_0_1>()->GetPicture(
        pp_resource(), cc.output(), cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

void VideoDecoder::RecyclePicture(const PP_VideoPicture& picture) {
  if (has_interface<PPB_VideoDecoder_0_2>()) {
    get_interface<PPB_VideoDecoder_0_2>()->RecyclePicture(pp_resource(),
                                                          &picture);
  } else if (has_interface<PPB_VideoDecoder_0_1>()) {
    get_interface<PPB_VideoDecoder_0_1>()->RecyclePicture(pp_resource(),
                                                          &picture);
  }
}

int32_t VideoDecoder::Flush(const CompletionCallback& cc) {
  if (has_interface<PPB_VideoDecoder_0_2>()) {
    return get_interface<PPB_VideoDecoder_0_2>()->Flush(
        pp_resource(), cc.pp_completion_callback());
  }
  if (has_interface<PPB_VideoDecoder_0_1>()) {
    return get_interface<PPB_VideoDecoder_0_1>()->Flush(
        pp_resource(), cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t VideoDecoder::Reset(const CompletionCallback& cc) {
  if (has_interface<PPB_VideoDecoder_0_2>()) {
    return get_interface<PPB_VideoDecoder_0_2>()->Reset(
        pp_resource(), cc.pp_completion_callback());
  }
  if (has_interface<PPB_VideoDecoder_0_1>()) {
    return get_interface<PPB_VideoDecoder_0_1>()->Reset(
        pp_resource(), cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

}  // namespace pp
