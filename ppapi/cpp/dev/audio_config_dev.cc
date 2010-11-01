// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/audio_config_dev.h"

#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

DeviceFuncs<PPB_AudioConfig_Dev> audio_cfg_f(PPB_AUDIO_CONFIG_DEV_INTERFACE);

namespace pp {

AudioConfig_Dev::AudioConfig_Dev()
    : sample_rate_(PP_AUDIOSAMPLERATE_NONE),
      sample_frame_count_(0) {
}

AudioConfig_Dev::AudioConfig_Dev(PP_AudioSampleRate_Dev sample_rate,
                                 uint32_t sample_frame_count)
    : sample_rate_(sample_rate),
      sample_frame_count_(sample_frame_count) {
  if (audio_cfg_f) {
    PassRefFromConstructor(audio_cfg_f->CreateStereo16Bit(
        Module::Get()->pp_module(), sample_rate,
        sample_frame_count));
  }
}

// static
uint32_t AudioConfig_Dev::RecommendSampleFrameCount(
    uint32_t requested_sample_frame_count) {
  if (!audio_cfg_f)
    return 0;
  return audio_cfg_f->RecommendSampleFrameCount(requested_sample_frame_count);
}

}  // namespace pp

