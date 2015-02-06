// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/video_encoder_resource.h"

using ppapi::thunk::PPB_VideoEncoder_API;

namespace ppapi {
namespace proxy {

VideoEncoderResource::VideoEncoderResource(Connection connection,
                                           PP_Instance instance)
    : PluginResource(connection, instance) {
}

VideoEncoderResource::~VideoEncoderResource() {
}

PPB_VideoEncoder_API* VideoEncoderResource::AsPPB_VideoEncoder_API() {
  return this;
}

int32_t VideoEncoderResource::GetSupportedProfiles(
    const PP_ArrayOutput& output,
    const scoped_refptr<TrackedCallback>& callback) {
  return PP_ERROR_FAILED;
}

int32_t VideoEncoderResource::Initialize(
    PP_VideoFrame_Format input_format,
    const PP_Size* input_visible_size,
    PP_VideoProfile output_profile,
    uint32_t initial_bitrate,
    PP_HardwareAcceleration acceleration,
    const scoped_refptr<TrackedCallback>& callback) {
  return PP_ERROR_FAILED;
}

int32_t VideoEncoderResource::GetFramesRequired() {
  return PP_ERROR_FAILED;
}

int32_t VideoEncoderResource::GetFrameCodedSize(PP_Size* size) {
  return PP_ERROR_FAILED;
}

int32_t VideoEncoderResource::GetVideoFrame(
    PP_Resource* video_frame,
    const scoped_refptr<TrackedCallback>& callback) {
  return PP_ERROR_FAILED;
}

int32_t VideoEncoderResource::Encode(
    PP_Resource video_frame,
    PP_Bool force_keyframe,
    const scoped_refptr<TrackedCallback>& callback) {
  return PP_ERROR_FAILED;
}

int32_t VideoEncoderResource::GetBitstreamBuffer(
    PP_BitstreamBuffer* picture,
    const scoped_refptr<TrackedCallback>& callback) {
  return PP_ERROR_FAILED;
}

void VideoEncoderResource::RecycleBitstreamBuffer(
    const PP_BitstreamBuffer* picture) {
}

void VideoEncoderResource::RequestEncodingParametersChange(uint32_t bitrate,
                                                           uint32_t framerate) {
}

void VideoEncoderResource::Close() {
}

}  // namespace proxy
}  // namespace ppapi
