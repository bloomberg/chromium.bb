// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/video_decoder_dev.h"

#include "ppapi/c/dev/ppb_video_decoder_dev.h"
#include "ppapi/c/dev/ppp_video_decoder_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/common.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

using std::vector;

namespace pp {

namespace {

template <> const char* interface_name<PPB_VideoDecoder_Dev>() {
  return PPB_VIDEODECODER_DEV_INTERFACE;
}

}  // namespace

VideoDecoder::VideoDecoder(const Instance* /* instance */,
                           const std::vector<uint32_t>& /* config */,
                           Client* client)
    : client_(client) {
  if (!has_interface<PPB_VideoDecoder_Dev>())
    return;
  // TODO(vmr): Implement.
}

VideoDecoder::~VideoDecoder() {}

vector<uint32_t> VideoDecoder::GetConfigs(
    Instance* /* instance */,
    const vector<uint32_t>& /* prototype_config */) {
  // TODO(vmr): Implement.
  vector<uint32_t> matching_configs;
  if (!has_interface<PPB_VideoDecoder_Dev>())
    return matching_configs;
  return matching_configs;
}

void VideoDecoder::AssignGLESBuffers(uint32_t /* no_of_buffers */,
                                     const PP_GLESBuffer_Dev& /* buffers */) {
  // TODO(vmr): Implement.
}

void VideoDecoder::AssignSysmemBuffers(
    uint32_t /* no_of_buffers */,
    const PP_SysmemBuffer_Dev& /* buffers */) {
  // TODO(vmr): Implement.
}

bool VideoDecoder::Decode(
    const PP_VideoBitstreamBuffer_Dev& /* bitstream_buffer */,
    CompletionCallback /* callback */) {
  // TODO(vmr): Implement.
  if (!has_interface<PPB_VideoDecoder_Dev>() || !pp_resource())
    return false;
  return false;
}

void VideoDecoder::ReusePictureBuffer(int32_t /* picture_buffer_id */) {
  // TODO(vmr): Implement.
}

bool VideoDecoder::Flush(CompletionCallback /* callback */) {
  // TODO(vmr): Implement.
  if (!has_interface<PPB_VideoDecoder_Dev>())
    return false;
  return true;
}

bool VideoDecoder::Abort(CompletionCallback /* callback */) {
  // TODO(vmr): Implement.
  if (!has_interface<PPB_VideoDecoder_Dev>())
    return false;
  return true;
}

}  // namespace pp
