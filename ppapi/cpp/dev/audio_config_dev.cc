// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/audio_config_dev.h"

#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_AudioConfig_Dev>() {
  return PPB_AUDIO_CONFIG_DEV_INTERFACE;
}

}  // namespace

AudioConfig_Dev::AudioConfig_Dev()
    : sample_rate_(PP_AUDIOSAMPLERATE_NONE),
      sample_frame_count_(0) {
}

AudioConfig_Dev::AudioConfig_Dev(PP_AudioSampleRate_Dev sample_rate,
                                 uint32_t sample_frame_count)
    : sample_rate_(sample_rate),
      sample_frame_count_(sample_frame_count) {
  if (has_interface<PPB_AudioConfig_Dev>()) {
    PassRefFromConstructor(
        get_interface<PPB_AudioConfig_Dev>()->CreateStereo16Bit(
        Module::Get()->pp_module(), sample_rate, sample_frame_count));
  }
}

// static
uint32_t AudioConfig_Dev::RecommendSampleFrameCount(
    uint32_t requested_sample_frame_count) {
  if (!has_interface<PPB_AudioConfig_Dev>())
    return 0;
  return get_interface<PPB_AudioConfig_Dev>()->
      RecommendSampleFrameCount(requested_sample_frame_count);
}

}  // namespace pp

