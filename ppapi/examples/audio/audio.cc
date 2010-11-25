// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <limits>

#include "ppapi/cpp/dev/audio_dev.h"
#include "ppapi/cpp/dev/audio_config_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"

// Separate left and right frequency to make sure we didn't swap L & R.
// Sounds pretty horrible, though...
const double frequency_l = 200;
const double frequency_r = 1000;

// This sample frequency is guaranteed to work.
const PP_AudioSampleRate_Dev sample_frequency = PP_AUDIOSAMPLERATE_44100;
const uint32_t sample_count = 4096;
uint32_t obtained_sample_count = 0;

class MyInstance : public pp::Instance {
 public:
  explicit MyInstance(PP_Instance instance)
      : pp::Instance(instance),
        audio_time_(0) {
  }

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    pp::AudioConfig_Dev config;
    obtained_sample_count = pp::AudioConfig_Dev::RecommendSampleFrameCount(
        sample_count);
    config = pp::AudioConfig_Dev(sample_frequency, obtained_sample_count);
    audio_ = pp::Audio_Dev(*this, config, SineWaveCallback, this);
    return audio_.StartPlayback();
  }

 private:
  static void SineWaveCallback(void* samples, size_t num_bytes, void* thiz) {
    const double th_l = 2 * 3.141592653589 * frequency_l / sample_frequency;
    const double th_r = 2 * 3.141592653589 * frequency_r / sample_frequency;

    // Store time value to avoid clicks on buffer boundries.
    size_t t = reinterpret_cast<MyInstance*>(thiz)->audio_time_;

    uint16_t* buf = reinterpret_cast<uint16_t*>(samples);
    for (size_t sample = 0; sample < obtained_sample_count; ++sample) {
      *buf++ = static_cast<uint16_t>(sin(th_l * t)
          * std::numeric_limits<uint16_t>::max());
      *buf++ = static_cast<uint16_t>(sin(th_r * t++)
          * std::numeric_limits<uint16_t>::max());
    }
    reinterpret_cast<MyInstance*>(thiz)->audio_time_ = t;
  }

  // Audio resource. Allocated in Init(), freed on destruction.
  pp::Audio_Dev audio_;

  // Audio buffer time. Used to make prevent sine wave skips on buffer
  // boundaries.
  size_t audio_time_;
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
