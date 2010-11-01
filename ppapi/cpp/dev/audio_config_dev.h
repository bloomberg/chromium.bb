// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_AUDIO_CONFIG_DEV_H_
#define PPAPI_CPP_DEV_AUDIO_CONFIG_DEV_H_

#include "ppapi/c/dev/ppb_audio_config_dev.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/cpp/resource.h"

namespace pp {

// Typical usage:
//
//   // Create an audio config with a supported frame count.
//   uint32_t sample_frame_count =
//       AudioConfig_Dev::RecommendSampleFrameCount(4096);
//   AudioConfig_Dev config(PP_AUDIOSAMPLERATE_44100, sample_frame_count);
//   if (config.is_null())
//     return false;  // Couldn't configure audio.
//
//   // Then use the config to create your audio resource.
//   Audio_dev audio(..., config, ...);
//   if (audio.is_null())
//     return false;  // Couldn't create audio.
class AudioConfig_Dev : public Resource {
 public:
  AudioConfig_Dev();

  // Creates an audio config based on the given sample rate and frame count.
  // If the rate and frame count aren't supported, the resulting resource
  // will be is_null(). Pass the result of RecommendSampleFrameCount as the
  // semple frame count.
  //
  // See PPB_AudioConfigDev.CreateStereo16Bit for more.
  AudioConfig_Dev(PP_AudioSampleRate_Dev sample_rate,
                  uint32_t sample_frame_count);

  // Returns a supported frame count for use in the constructor.
  //
  // See PPB_AudioConfigDev.RecommendSampleFrameCount.
  static uint32_t RecommendSampleFrameCount(
      uint32_t requested_sample_frame_count);

  PP_AudioSampleRate_Dev sample_rate() const { return sample_rate_; }
  uint32_t sample_frame_count() { return sample_frame_count_; }

 private:
  PP_AudioSampleRate_Dev sample_rate_;
  uint32_t sample_frame_count_;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_AUDIO_CONFIG_DEV_H_

