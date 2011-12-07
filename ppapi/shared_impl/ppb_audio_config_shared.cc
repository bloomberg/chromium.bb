// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_audio_config_shared.h"

namespace ppapi {

PPB_AudioConfig_Shared::PPB_AudioConfig_Shared(PP_Instance instance)
    : Resource(instance),
      sample_rate_(PP_AUDIOSAMPLERATE_NONE),
      sample_frame_count_(0) {
}

PPB_AudioConfig_Shared::PPB_AudioConfig_Shared(
    const HostResource& host_resource)
    : Resource(host_resource),
      sample_rate_(PP_AUDIOSAMPLERATE_NONE),
      sample_frame_count_(0) {
}

PPB_AudioConfig_Shared::~PPB_AudioConfig_Shared() {
}

// static
PP_Resource PPB_AudioConfig_Shared::CreateAsImpl(
    PP_Instance instance,
    PP_AudioSampleRate sample_rate,
    uint32_t sample_frame_count) {
  scoped_refptr<PPB_AudioConfig_Shared> object(
      new PPB_AudioConfig_Shared(instance));
  if (!object->Init(sample_rate, sample_frame_count))
    return 0;
  return object->GetReference();
}

// static
PP_Resource PPB_AudioConfig_Shared::CreateAsProxy(
    PP_Instance instance,
    PP_AudioSampleRate sample_rate,
    uint32_t sample_frame_count) {
  scoped_refptr<PPB_AudioConfig_Shared> object(new PPB_AudioConfig_Shared(
      HostResource::MakeInstanceOnly(instance)));
  if (!object->Init(sample_rate, sample_frame_count))
    return 0;
  return object->GetReference();
}

thunk::PPB_AudioConfig_API* PPB_AudioConfig_Shared::AsPPB_AudioConfig_API() {
  return this;
}

PP_AudioSampleRate PPB_AudioConfig_Shared::GetSampleRate() {
  return sample_rate_;
}

uint32_t PPB_AudioConfig_Shared::GetSampleFrameCount() {
  return sample_frame_count_;
}

bool PPB_AudioConfig_Shared::Init(PP_AudioSampleRate sample_rate,
                                  uint32_t sample_frame_count) {
  // TODO(brettw): Currently we don't actually check what the hardware
  // supports, so just allow sample rates of the "guaranteed working" ones.
  if (sample_rate != PP_AUDIOSAMPLERATE_44100 &&
      sample_rate != PP_AUDIOSAMPLERATE_48000)
    return false;

  // TODO(brettw): Currently we don't actually query to get a value from the
  // hardware, so just validate the range.
  if (sample_frame_count > PP_AUDIOMAXSAMPLEFRAMECOUNT ||
      sample_frame_count < PP_AUDIOMINSAMPLEFRAMECOUNT)
    return false;

  sample_rate_ = sample_rate;
  sample_frame_count_ = sample_frame_count;
  return true;
}

}  // namespace ppapi
