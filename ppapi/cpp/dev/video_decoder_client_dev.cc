// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/video_decoder_client_dev.h"

#include "ppapi/c/dev/ppp_video_decoder_dev.h"
#include "ppapi/cpp/dev/video_decoder_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

const char kPPPVideoDecoderInterface[] = PPP_VIDEODECODER_DEV_INTERFACE;

// Callback to provide buffers for the decoded output pictures.
void ProvidePictureBuffers(PP_Instance instance,
                           uint32_t req_num_of_bufs,
                           struct PP_Size dimensions) {
  void* object = pp::Instance::GetPerInstanceObject(
      instance, kPPPVideoDecoderInterface);
  if (!object)
    return;
  static_cast<VideoDecoderClient_Dev*>(object)->ProvidePictureBuffers(
      req_num_of_bufs, dimensions);
}

void DismissPictureBuffer(PP_Instance instance,
                          int32_t picture_buffer_id) {
  void* object = pp::Instance::GetPerInstanceObject(
      instance, kPPPVideoDecoderInterface);
  if (!object)
    return;
  static_cast<VideoDecoderClient_Dev*>(object)->DismissPictureBuffer(
      picture_buffer_id);
}

void PictureReady(PP_Instance instance, PP_Picture_Dev picture) {
  void* object = pp::Instance::GetPerInstanceObject(
      instance, kPPPVideoDecoderInterface);
  if (!object)
    return;
  static_cast<VideoDecoderClient_Dev*>(object)->PictureReady(picture);
}

void EndOfStream(PP_Instance instance) {
  void* object = pp::Instance::GetPerInstanceObject(
      instance, kPPPVideoDecoderInterface);
  if (!object)
    return;
  static_cast<VideoDecoderClient_Dev*>(object)->EndOfStream();
}

void NotifyError(PP_Instance instance, PP_VideoDecodeError_Dev error) {
  void* object = pp::Instance::GetPerInstanceObject(
      instance, kPPPVideoDecoderInterface);
  if (!object)
    return;
  static_cast<VideoDecoderClient_Dev*>(object)->NotifyError(error);
}

static PPP_VideoDecoder_Dev videodecoder_interface = {
  &ProvidePictureBuffers,
  &DismissPictureBuffer,
  &PictureReady,
  &EndOfStream,
  &NotifyError,
};

}  // namespace

VideoDecoderClient_Dev::VideoDecoderClient_Dev(Instance* instance)
    : associated_instance_(instance) {
  pp::Module::Get()->AddPluginInterface(kPPPVideoDecoderInterface,
                                        &videodecoder_interface);
  associated_instance_->AddPerInstanceObject(
      kPPPVideoDecoderInterface, this);
}

VideoDecoderClient_Dev::~VideoDecoderClient_Dev() {
  associated_instance_->RemovePerInstanceObject(
      kPPPVideoDecoderInterface, this);
}

}  // namespace pp
