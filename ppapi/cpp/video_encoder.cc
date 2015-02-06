// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/video_encoder.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_video_encoder.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <>
const char* interface_name<PPB_VideoEncoder_0_1>() {
  return PPB_VIDEOENCODER_INTERFACE_0_1;
}

}  // namespace

VideoEncoder::VideoEncoder() {
}

VideoEncoder::VideoEncoder(const InstanceHandle& instance) {
  if (has_interface<PPB_VideoEncoder_0_1>()) {
    PassRefFromConstructor(
        get_interface<PPB_VideoEncoder_0_1>()->Create(instance.pp_instance()));
  }
}

VideoEncoder::VideoEncoder(const VideoEncoder& other) : Resource(other) {
}

int32_t VideoEncoder::GetSupportedProfiles(const CompletionCallbackWithOutput<
    std::vector<PP_VideoProfileDescription> >& cc) {
  if (has_interface<PPB_VideoEncoder_0_1>()) {
    return get_interface<PPB_VideoEncoder_0_1>()->GetSupportedProfiles(
        pp_resource(), cc.output(), cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t VideoEncoder::Initialize(const PP_VideoFrame_Format& input_format,
                                 const Size& input_visible_size,
                                 const PP_VideoProfile& output_profile,
                                 const uint32_t initial_bitrate,
                                 PP_HardwareAcceleration acceleration,
                                 const CompletionCallback& cc) {
  if (has_interface<PPB_VideoEncoder_0_1>()) {
    return get_interface<PPB_VideoEncoder_0_1>()->Initialize(
        pp_resource(), input_format, &input_visible_size.pp_size(),
        output_profile, initial_bitrate, acceleration,
        cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t VideoEncoder::GetFramesRequired() {
  if (has_interface<PPB_VideoEncoder_0_1>()) {
    return get_interface<PPB_VideoEncoder_0_1>()->GetFramesRequired(
        pp_resource());
  }
  return PP_ERROR_NOINTERFACE;
}

int32_t VideoEncoder::GetFrameCodedSize(Size* coded_size) {
  if (has_interface<PPB_VideoEncoder_0_1>()) {
    return get_interface<PPB_VideoEncoder_0_1>()->GetFrameCodedSize(
        pp_resource(), &coded_size->pp_size());
  }
  return PP_ERROR_NOINTERFACE;
}

int32_t VideoEncoder::GetVideoFrame(
    const CompletionCallbackWithOutput<VideoFrame>& cc) {
  if (has_interface<PPB_VideoEncoder_0_1>()) {
    return get_interface<PPB_VideoEncoder_0_1>()->GetVideoFrame(
        pp_resource(), cc.output(), cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t VideoEncoder::Encode(const VideoFrame& video_frame,
                             bool force_keyframe,
                             const CompletionCallback& cc) {
  if (has_interface<PPB_VideoEncoder_0_1>()) {
    return get_interface<PPB_VideoEncoder_0_1>()->Encode(
        pp_resource(), video_frame.pp_resource(), PP_FromBool(force_keyframe),
        cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t VideoEncoder::GetBitstreamBuffer(
    const CompletionCallbackWithOutput<PP_BitstreamBuffer>& cc) {
  if (has_interface<PPB_VideoEncoder_0_1>()) {
    return get_interface<PPB_VideoEncoder_0_1>()->GetBitstreamBuffer(
        pp_resource(), cc.output(), cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

void VideoEncoder::RecycleBitstreamBuffer(
    const PP_BitstreamBuffer& bitstream_buffer) {
  if (has_interface<PPB_VideoEncoder_0_1>()) {
    get_interface<PPB_VideoEncoder_0_1>()->RecycleBitstreamBuffer(
        pp_resource(), &bitstream_buffer);
  }
}

void VideoEncoder::RequestEncodingParametersChange(uint32_t bitrate,
                                                   uint32_t framerate) {
  if (has_interface<PPB_VideoEncoder_0_1>()) {
    get_interface<PPB_VideoEncoder_0_1>()->RequestEncodingParametersChange(
        pp_resource(), bitrate, framerate);
  }
}

void VideoEncoder::Close() {
  if (has_interface<PPB_VideoEncoder_0_1>()) {
    get_interface<PPB_VideoEncoder_0_1>()->Close(pp_resource());
  }
}

}  // namespace pp
