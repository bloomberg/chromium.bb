// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/audio_frame.h"

#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_AudioFrame_0_1>() {
  return PPB_AUDIOFRAME_INTERFACE_0_1;
}

}  // namespace

AudioFrame::AudioFrame() {
}

AudioFrame::AudioFrame(const AudioFrame& other) : Resource(other) {
}

AudioFrame::AudioFrame(const Resource& resource) : Resource(resource) {
}

AudioFrame::AudioFrame(PassRef, PP_Resource resource)
    : Resource(PASS_REF, resource) {
}

AudioFrame::~AudioFrame() {
}

PP_TimeDelta AudioFrame::GetTimestamp() const {
  if (has_interface<PPB_AudioFrame_0_1>())
    return get_interface<PPB_AudioFrame_0_1>()->GetTimestamp(pp_resource());
  return 0.0;
}

void AudioFrame::SetTimestamp(PP_TimeDelta timestamp) {
  if (has_interface<PPB_AudioFrame_0_1>())
    get_interface<PPB_AudioFrame_0_1>()->SetTimestamp(pp_resource(), timestamp);
}

uint32_t AudioFrame::GetSampleSize() const {
  if (has_interface<PPB_AudioFrame_0_1>())
    return get_interface<PPB_AudioFrame_0_1>()->GetSampleSize(pp_resource());
  return 0;
}

uint32_t AudioFrame::GetNumberOfChannels() const {
  if (has_interface<PPB_AudioFrame_0_1>()) {
    return get_interface<PPB_AudioFrame_0_1>()->GetNumberOfChannels(
        pp_resource());
  }
  return 0;
}

uint32_t AudioFrame::GetNumberOfSamples() const {
  if (has_interface<PPB_AudioFrame_0_1>()) {
    return get_interface<PPB_AudioFrame_0_1>()->GetNumberOfSamples(
        pp_resource());
  }
  return 0;
}

void* AudioFrame::GetDataBuffer() {
  if (has_interface<PPB_AudioFrame_0_1>())
    return get_interface<PPB_AudioFrame_0_1>()->GetDataBuffer(pp_resource());
  return NULL;
}

uint32_t AudioFrame::GetDataBufferSize() const {
  if (has_interface<PPB_AudioFrame_0_1>()) {
    return get_interface<PPB_AudioFrame_0_1>()->GetDataBufferSize(
        pp_resource());
  }
  return 0;
}

}  // namespace pp
