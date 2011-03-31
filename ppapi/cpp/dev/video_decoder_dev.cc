// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/video_decoder_dev.h"

#include <algorithm>
#include <iterator>

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

VideoDecoder::VideoDecoder(
    const Instance* /* instance */,
    VideoDecoderClient* /* picture_interface */) {
  if (!has_interface<PPB_VideoDecoder_Dev>())
    return;
}

VideoDecoder::~VideoDecoder() {}

vector<uint32_t> VideoDecoder::GetConfig(
    Instance* /* instance */,
    const vector<uint32_t>& /* prototype_config */) {
  vector<uint32_t> matching_configs;
  if (!has_interface<PPB_VideoDecoder_Dev>())
    return matching_configs;
  return matching_configs;
}

bool VideoDecoder::Initialize(const std::vector<uint32_t>& /* config */) {
  if (!has_interface<PPB_VideoDecoder_Dev>())
    return false;
  return false;
}

bool VideoDecoder::Decode(
    const PP_VideoBitstreamBuffer_Dev& /* bitstream_buffer */,
    PP_CompletionCallback /* callback */) {
  if (!has_interface<PPB_VideoDecoder_Dev>() || !pp_resource())
    return false;
  return false;
}

int32_t VideoDecoder::Flush(PP_CompletionCallback /* callback */) {
  if (!has_interface<PPB_VideoDecoder_Dev>())
    return PP_ERROR_NOINTERFACE;
  return PP_ERROR_ABORTED;
}

int32_t VideoDecoder::Abort(PP_CompletionCallback /* callback */) {
  if (!has_interface<PPB_VideoDecoder_Dev>())
    return PP_ERROR_NOINTERFACE;
  return PP_ERROR_ABORTED;
}

void VideoDecoder::EventPicture(struct PP_Picture_Dev* /* picture */) {}

void VideoDecoder::EventEndOfStream() {}

void VideoDecoder::EventError(PP_VideoDecodeError_Dev /* error */) {}

}  // namespace pp
