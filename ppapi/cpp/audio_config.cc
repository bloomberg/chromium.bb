// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/audio_config.h"

#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_AudioConfig>() {
  return PPB_AUDIO_CONFIG_INTERFACE;
}

}  // namespace

AudioConfig::AudioConfig()
    : sample_rate_(PP_AUDIOSAMPLERATE_NONE),
      sample_frame_count_(0) {
}

AudioConfig::AudioConfig(const InstanceHandle& instance,
                         PP_AudioSampleRate sample_rate,
                         uint32_t sample_frame_count)
    : sample_rate_(sample_rate),
      sample_frame_count_(sample_frame_count) {
  if (has_interface<PPB_AudioConfig>()) {
    PassRefFromConstructor(
        get_interface<PPB_AudioConfig>()->CreateStereo16Bit(
        instance.pp_instance(), sample_rate, sample_frame_count));
  }
}

// static
PP_AudioSampleRate AudioConfig::RecommendSampleRate(
    const InstanceHandle& instance) {
  if (!has_interface<PPB_AudioConfig>())
    return PP_AUDIOSAMPLERATE_NONE;
  return get_interface<PPB_AudioConfig>()->
      RecommendSampleRate(instance.pp_instance());
}

// static
uint32_t AudioConfig::RecommendSampleFrameCount(
    const InstanceHandle& instance,
    PP_AudioSampleRate sample_rate,
    uint32_t requested_sample_frame_count) {
  if (!has_interface<PPB_AudioConfig>())
    return 0;
  return get_interface<PPB_AudioConfig>()->
      RecommendSampleFrameCount(instance.pp_instance(),
                                sample_rate,
                                requested_sample_frame_count);
}

}  // namespace pp

