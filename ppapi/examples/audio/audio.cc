// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <limits>

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/audio.h"
#include "ppapi/cpp/audio_config.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"

// Separate left and right frequency to make sure we didn't swap L & R.
// Sounds pretty horrible, though...
const double frequency_l = 400;
const double frequency_r = 1000;

// This sample frequency is guaranteed to work.
const PP_AudioSampleRate sample_frequency = PP_AUDIOSAMPLERATE_44100;
const uint32_t sample_count = 4096;
uint32_t obtained_sample_count = 0;

const double kPi = 3.141592653589;
const double kTwoPi = 2.0 * kPi;

class MyInstance : public pp::Instance {
 public:
  explicit MyInstance(PP_Instance instance)
      : pp::Instance(instance),
        audio_wave_l_(0.0),
        audio_wave_r_(0.0) {
  }

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    pp::AudioConfig config;
    obtained_sample_count = pp::AudioConfig::RecommendSampleFrameCount(
        sample_frequency, sample_count);
    config = pp::AudioConfig(this, sample_frequency, obtained_sample_count);
    audio_ = pp::Audio(this, config, SineWaveCallback, this);
    return audio_.StartPlayback();
  }

 private:
  static void SineWaveCallback(void* samples, uint32_t num_bytes, void* thiz) {
    const double delta_l = kTwoPi * frequency_l / sample_frequency;
    const double delta_r = kTwoPi * frequency_r / sample_frequency;

    // Use per channel audio wave value to avoid clicks on buffer boundries.
    double wave_l = reinterpret_cast<MyInstance*>(thiz)->audio_wave_l_;
    double wave_r = reinterpret_cast<MyInstance*>(thiz)->audio_wave_r_;
    const int16_t max_int16 = std::numeric_limits<int16_t>::max();
    int16_t* buf = reinterpret_cast<int16_t*>(samples);
    for (size_t sample = 0; sample < obtained_sample_count; ++sample) {
      *buf++ = static_cast<int16_t>(sin(wave_l) * max_int16);
      *buf++ = static_cast<int16_t>(sin(wave_r) * max_int16);
      // Add delta, keep within -kTwoPi..kTwoPi to preserve precision.
      wave_l += delta_l;
      if (wave_l > kTwoPi)
        wave_l -= kTwoPi * 2.0;
      wave_r += delta_r;
      if (wave_r > kTwoPi)
        wave_r -= kTwoPi * 2.0;
    }
    // Store current value to use as starting point for next callback.
    reinterpret_cast<MyInstance*>(thiz)->audio_wave_l_ = wave_l;
    reinterpret_cast<MyInstance*>(thiz)->audio_wave_r_ = wave_r;
  }

  // Audio resource. Allocated in Init(), freed on destruction.
  pp::Audio audio_;

  // Current audio wave position, used to prevent sine wave skips
  // on buffer boundaries.
  double audio_wave_l_;
  double audio_wave_r_;
};

class MyModule : public pp::Module {
 public:
  // Override CreateInstance to create your customized Instance object.
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MyInstance(instance);
  }
};

namespace pp {

// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new MyModule();
}

}  // namespace pp
