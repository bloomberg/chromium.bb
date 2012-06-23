// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_audio_config_shared.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"

namespace ppapi {

PPB_AudioConfig_Shared::PPB_AudioConfig_Shared(ResourceObjectType type,
                                               PP_Instance instance)
    : Resource(type, instance),
      sample_rate_(PP_AUDIOSAMPLERATE_NONE),
      sample_frame_count_(0) {
}

PPB_AudioConfig_Shared::~PPB_AudioConfig_Shared() {
}

PP_Resource PPB_AudioConfig_Shared::Create(
    ResourceObjectType type,
    PP_Instance instance,
    PP_AudioSampleRate sample_rate,
    uint32_t sample_frame_count) {
  scoped_refptr<PPB_AudioConfig_Shared> object(
      new PPB_AudioConfig_Shared(type, instance));
  if (!object->Init(sample_rate, sample_frame_count))
    return 0;
  return object->GetReference();
}

// static
uint32_t PPB_AudioConfig_Shared::RecommendSampleFrameCount_1_0(
    PP_AudioSampleRate sample_rate,
    uint32_t requested_sample_frame_count) {
  // Version 1.0: Don't actually query to get a value from the
  // hardware; instead return the input for in-range values.
  if (requested_sample_frame_count < PP_AUDIOMINSAMPLEFRAMECOUNT)
    return PP_AUDIOMINSAMPLEFRAMECOUNT;
  if (requested_sample_frame_count > PP_AUDIOMAXSAMPLEFRAMECOUNT)
    return PP_AUDIOMAXSAMPLEFRAMECOUNT;
  return requested_sample_frame_count;
}

// static
uint32_t PPB_AudioConfig_Shared::RecommendSampleFrameCount_1_1(
    PP_Instance instance,
    PP_AudioSampleRate sample_rate,
    uint32_t sample_frame_count) {
  // Version 1.1: Query the back-end hardware for sample rate and buffer size,
  // and recommend a best fit based on request.
  thunk::EnterInstanceNoLock enter(instance);
  if (enter.failed())
    return 0;

  // Get the hardware config.
  PP_AudioSampleRate hardware_sample_rate = static_cast<PP_AudioSampleRate>(
      enter.functions()->GetAudioHardwareOutputSampleRate(instance));
  uint32_t hardware_sample_frame_count =
      enter.functions()->GetAudioHardwareOutputBufferSize(instance);
  if (sample_frame_count < PP_AUDIOMINSAMPLEFRAMECOUNT)
    sample_frame_count = PP_AUDIOMINSAMPLEFRAMECOUNT;

  // If client is using same sample rate as audio hardware, then recommend a
  // multiple of the audio hardware's sample frame count.
  if (hardware_sample_rate == sample_rate && hardware_sample_frame_count > 0) {
    // Round up input sample_frame_count to nearest multiple.
    uint32_t multiple = (sample_frame_count + hardware_sample_frame_count - 1) /
        hardware_sample_frame_count;
    uint32_t recommendation = hardware_sample_frame_count * multiple;
    if (recommendation > PP_AUDIOMAXSAMPLEFRAMECOUNT)
      recommendation = PP_AUDIOMAXSAMPLEFRAMECOUNT;
    return recommendation;
  }

  // Otherwise, recommend a conservative 30ms buffer based on sample rate.
  const uint32_t kDefault30msAt44100kHz = 1323;
  const uint32_t kDefault30msAt48000kHz = 1440;
  switch (sample_rate) {
    case PP_AUDIOSAMPLERATE_44100:
      return kDefault30msAt44100kHz;
    case PP_AUDIOSAMPLERATE_48000:
      return kDefault30msAt48000kHz;
    case PP_AUDIOSAMPLERATE_NONE:
      return 0;
  }
  // Unable to make a recommendation.
  return 0;
}

// static
PP_AudioSampleRate PPB_AudioConfig_Shared::RecommendSampleRate(
    PP_Instance instance) {
  thunk::EnterInstanceNoLock enter(instance);
  if (enter.failed())
    return PP_AUDIOSAMPLERATE_NONE;
  PP_AudioSampleRate hardware_sample_rate = static_cast<PP_AudioSampleRate>(
    enter.functions()->GetAudioHardwareOutputSampleRate(instance));
  return hardware_sample_rate;
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
