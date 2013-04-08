// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/video_writer.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_video_writer.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/video_frame.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_VideoWriter_0_1>() {
  return PPB_VIDEOWRITER_INTERFACE_0_1;
}

}  // namespace

// VideoWriter -----------------------------------------------------------------

VideoWriter::VideoWriter() {
}

VideoWriter::VideoWriter(const InstanceHandle& instance) {
  if (!has_interface<PPB_VideoWriter_0_1>())
    return;
  PassRefFromConstructor(get_interface<PPB_VideoWriter_0_1>()->Create(
      instance.pp_instance()));
}

VideoWriter::VideoWriter(const VideoWriter& other)
    : Resource(other) {
}

VideoWriter::VideoWriter(PassRef, PP_Resource resource)
    : Resource(PASS_REF, resource) {
}

int32_t VideoWriter::Open(const CompletionCallbackWithOutput<Var>& cc) {
  if (has_interface<PPB_VideoWriter_0_1>()) {
    int32_t result =
        get_interface<PPB_VideoWriter_0_1>()->Open(
            pp_resource(),
            cc.output(), cc.pp_completion_callback());
    return result;
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t VideoWriter::PutFrame(const VideoFrame& frame) {
  if (has_interface<PPB_VideoWriter_0_1>()) {
    return get_interface<PPB_VideoWriter_0_1>()->PutFrame(
        pp_resource(),
        &frame.pp_video_frame());
  }
  return PP_ERROR_NOINTERFACE;
}

void VideoWriter::Close() {
  if (has_interface<PPB_VideoWriter_0_1>()) {
    get_interface<PPB_VideoWriter_0_1>()->Close(pp_resource());
  }
}

}  // namespace pp
