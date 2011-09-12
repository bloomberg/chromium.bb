// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include <stdint.h>

#include <cmath>
#include <limits>
#include <string>

#include <nacl/nacl_check.h>
#include <nacl/nacl_log.h>

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/cpp/audio.h"
#include "ppapi/cpp/audio_config.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"

const PP_AudioSampleRate kSampleFrequency = PP_AUDIOSAMPLERATE_44100;
// Buffer size in units of sample frames.
// 4096 is a conservative size that should avoid underruns on most systems.
const uint32_t kSampleFrameCount = 4096;
const uint32_t kDefaultFrequency = 400;
const uint32_t kDefaultDuration = 10000;

const int kNumChannelsForStereo = 2;
const size_t kSizeSingleSample = kNumChannelsForStereo * sizeof(int16_t);

const double kPi = 3.141592653589;
const double kTwoPi = 2.0 * kPi;

class MyInstance : public pp::Instance {
 private:
  pp::Audio audio_;
  uint32_t obtained_sample_frame_count_;
  double audio_wave_time_;   // between -kTwoPi, +kTwoPi
  uint32_t duration_;   // in msec
  uint32_t frequency_;

  static void SoundCallback(void* samples, uint32_t num_bytes, void* thiz) {
    MyInstance* instance = reinterpret_cast<MyInstance*>(thiz);
    // CHECK inside callback is only for testing purposes.
    CHECK(instance->obtained_sample_frame_count_ * kSizeSingleSample ==
          num_bytes);
    const double delta = kTwoPi * instance->frequency_ / kSampleFrequency;
    const int16_t max_int16 = std::numeric_limits<int16_t>::max();
    int16_t* buf = reinterpret_cast<int16_t*>(samples);
    for (size_t i = 0; i < instance->obtained_sample_frame_count_; ++i) {
      const double v = sin(instance->audio_wave_time_) * max_int16;
      *buf++ = static_cast<int16_t>(v);
      *buf++ = static_cast<int16_t>(v);
      // Add delta, keep within -kTwoPi, +TwoPi to preserve precision.
      instance->audio_wave_time_ += delta;
      if (instance->audio_wave_time_ > kTwoPi)
        instance->audio_wave_time_ -= kTwoPi * 2.0;
    }
  }

  static void StopOutput(void* thiz, int32_t err) {
    if (PP_OK == err) {
      MyInstance* instance = static_cast<MyInstance*>(thiz);
      instance->audio_.StopPlayback();
    }
  }

  void ExtraChecks(pp::AudioConfig* config) {
    CHECK(obtained_sample_frame_count_ >= PP_AUDIOMINSAMPLEFRAMECOUNT);
    CHECK(obtained_sample_frame_count_ <= PP_AUDIOMAXSAMPLEFRAMECOUNT);

    PPB_GetInterface get_browser_if =
        pp::Module::Get()->get_browser_interface();

    const struct PPB_AudioConfig* audio_config_if =
        static_cast<const struct PPB_AudioConfig*>(
        get_browser_if(PPB_AUDIO_CONFIG_INTERFACE));

    const struct PPB_Audio* audio_if =
        static_cast<const struct PPB_Audio*>(
          get_browser_if(PPB_AUDIO_INTERFACE));

    CHECK(NULL != audio_config_if);
    CHECK(NULL != audio_if);

    const PP_Resource audio_config_res = config->pp_resource();
    const PP_Resource audio_res = audio_.pp_resource();

    CHECK(PP_TRUE == audio_config_if->IsAudioConfig(audio_config_res));
    CHECK(PP_TRUE == audio_if->IsAudio(audio_res));
    CHECK(PP_FALSE == audio_config_if->IsAudioConfig(audio_res));
    CHECK(PP_FALSE == audio_if->IsAudio(audio_config_res));
    CHECK(audio_if->GetCurrentConfig(audio_res) == audio_config_res);
    CHECK(0 == audio_if->GetCurrentConfig(audio_config_res));
    CHECK(audio_config_if->GetSampleRate(audio_config_res) ==
        config->sample_rate());
    CHECK(audio_config_if->GetSampleFrameCount(audio_config_res) ==
        config->sample_frame_count());
    CHECK(audio_.config().pp_resource() == audio_config_res);
  }

  void ParseArgs(uint32_t argc, const char* argn[], const char* argv[]) {
     for (uint32_t i = 0; i < argc; ++i) {
       const std::string tag = argn[i];
       if (tag == "duration") duration_ = strtol(argv[i], 0, 0);
       if (tag == "frequency") frequency_ = strtol(argv[i], 0, 0);
     }
  }

 public:
  explicit MyInstance(PP_Instance instance)
    : pp::Instance(instance),
      duration_(kDefaultDuration), frequency_(kDefaultFrequency) {}

  virtual ~MyInstance() {}

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    ParseArgs(argc, argn, argv);
    obtained_sample_frame_count_ = pp::AudioConfig::RecommendSampleFrameCount(
      kSampleFrequency, kSampleFrameCount);

    pp::AudioConfig config =
      pp::AudioConfig(this, kSampleFrequency, obtained_sample_frame_count_);

    audio_ = pp::Audio(this, config, SoundCallback, this);

    ExtraChecks(&config);

    CHECK(audio_.StartPlayback());

    pp::CompletionCallback cc(StopOutput, this);
    pp::Module::Get()->core()->CallOnMainThread(duration_, cc, PP_OK);
    return true;
  }
};

// standard boilerplate code below
class MyModule : public pp::Module {
 public:
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MyInstance(instance);
  }
};

namespace pp {
Module* CreateModule() {
  return new MyModule();
}

}  // namespace pp
