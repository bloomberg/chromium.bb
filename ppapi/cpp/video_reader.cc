// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/video_reader.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_video_reader.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/video_frame.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_VideoReader_0_1>() {
  return PPB_VIDEOREADER_INTERFACE_0_1;
}

}  // namespace

// VideoReader -----------------------------------------------------------------

VideoReader::VideoReader() {
}

VideoReader::VideoReader(const InstanceHandle& instance) {
  if (!has_interface<PPB_VideoReader_0_1>())
    return;
  PassRefFromConstructor(get_interface<PPB_VideoReader_0_1>()->Create(
      instance.pp_instance()));
}

VideoReader::VideoReader(const VideoReader& other)
    : Resource(other) {
}

VideoReader::VideoReader(PassRef, PP_Resource resource)
    : Resource(PASS_REF, resource) {
}

int32_t VideoReader::Open(const std::string& stream_id,
                          const CompletionCallback& cc) {
  if (has_interface<PPB_VideoReader_0_1>()) {
    Var id(stream_id);
    int32_t result =
        get_interface<PPB_VideoReader_0_1>()->Open(
            pp_resource(),
            id.pp_var(), cc.pp_completion_callback());
    return result;
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t VideoReader::GetFrame(
    const CompletionCallbackWithOutput<VideoFrame>& cc) {
  if (has_interface<PPB_VideoReader_0_1>()) {
    return get_interface<PPB_VideoReader_0_1>()->GetFrame(
        pp_resource(),
        cc.output(), cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

void VideoReader::Close() {
  if (has_interface<PPB_VideoReader_0_1>()) {
    get_interface<PPB_VideoReader_0_1>()->Close(pp_resource());
  }
}

}  // namespace pp
