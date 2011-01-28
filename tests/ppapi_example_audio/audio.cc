// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <stdio.h>
#include <stdlib.h>

#include <cmath>
#include <limits>

#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/cpp/audio.h"
#include "ppapi/cpp/audio_config.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"

// Most of this example is borrowed from ppapi/examples/audio/audio.cc

using ppapi_proxy::DebugPrintf;

// Separate left and right frequency to make sure we didn't swap L & R.
// Sounds pretty horrible, though...
const double frequency_l = 400;
const double frequency_r = 1000;

// This sample frequency is guaranteed to work.
const PP_AudioSampleRate kSampleFrequency = PP_AUDIOSAMPLERATE_44100;
// Buffer size in units of sample frames.
// 4096 is a conservative size that should avoid underruns on most systems.
const uint32_t kSampleFrameCount = 4096;
uint32_t obtained_sample_frame_count = 0;

const double kPi = 3.141592653589;
const double kTwoPi = 2.0 * kPi;

class MyInstance : public pp::Instance {
 public:
  explicit MyInstance(PP_Instance instance)
      : pp::Instance(instance),
        audio_wave_l_(0.0),
        audio_wave_r_(0.0) {
  }

  static void StopOutput(void* user_data, int32_t err) {
    DebugPrintf("example: StopOutput() invoked on main thread\n");
    if (PP_OK == err) {
      MyInstance* instance = static_cast<MyInstance*>(user_data);
      DebugPrintf("example: calling StopPlayback()\n");
      instance->audio_.StopPlayback();
    }
  }

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    pp::AudioConfig config;
    obtained_sample_frame_count = pp::AudioConfig::RecommendSampleFrameCount(
        kSampleFrequency, kSampleFrameCount);
    // Verify obtained_sample_frame_count isn't out of range.
    if ((obtained_sample_frame_count < PP_AUDIOMINSAMPLEFRAMECOUNT) ||
        (obtained_sample_frame_count > PP_AUDIOMAXSAMPLEFRAMECOUNT)) {
      DebugPrintf("example: obtained_sample_frame_count failed.\n");
      return false;
    }
    config = pp::AudioConfig(this, kSampleFrequency,
        obtained_sample_frame_count);
    audio_ = pp::Audio(this, config, SineWaveCallback, this);

    // Do some sanity checks; verify c & cpp interfaces agree.
    // Note: This is test code and is not normally needed for an application.
    PPB_GetInterface get_browser_interface =
        pp::Module::Get()->get_browser_interface();
    const struct PPB_AudioConfig* audio_config_interface =
        static_cast<const struct PPB_AudioConfig*>(
        get_browser_interface(PPB_AUDIO_CONFIG_INTERFACE));
    const struct PPB_Audio* audio_interface =
        static_cast<const struct PPB_Audio*>(
        get_browser_interface(PPB_AUDIO_INTERFACE));
    if (NULL == audio_config_interface) {
      DebugPrintf("example: failed to obtain PPB_AUDIO_CONFIG_INTERFACE\n");
      return false;
    }
    if (NULL == audio_interface) {
      DebugPrintf("example: failed to obtain PPB_AUDIO_INTERFACE\n");
      return false;
    }
    PP_Resource audio_config_resource = config.pp_resource();
    PP_Resource audio_resource = audio_.pp_resource();
    DebugPrintf("example: audio config resource: %d\n", audio_config_resource);
    DebugPrintf("example: audio resource: %d\n", audio_resource);
    if (!audio_config_interface->IsAudioConfig(audio_config_resource)) {
      DebugPrintf("example: failed on IsAudioConfig(audio_config_resource\n");
      return false;
    }
    if (!audio_interface->IsAudio(audio_resource)) {
      DebugPrintf("example: failed on IsAudio(audio_resource\n");
      return false;
    }
    if (audio_config_interface->GetSampleRate(audio_config_resource) !=
        config.sample_rate()) {
      DebugPrintf("example: GetSampleRate(audio_config_resource) !=\n");
      DebugPrintf("         config.sample_rate()\n");
      return false;
    }
    if (audio_config_interface->GetSampleFrameCount(audio_config_resource) !=
       config.sample_frame_count()) {
      DebugPrintf("example: GetSampleFrameCount(audio_config_resource) !=\n");
      DebugPrintf("         config.sample_frame_count()\n");
      return false;
    }
    if (audio_.config().pp_resource() != audio_config_resource) {
      DebugPrintf("example: audio_.config().pp_resource() !=\n");
      DebugPrintf("         audio_config_resource\n");
      return false;
    }
    // Attempt to start playback
    if (false == audio_.StartPlayback()) {
      DebugPrintf("example: StartPlayback() failed!\n");
      return false;
    }
    // Schedule a callback in 10 seconds to stop audio output
    pp::CompletionCallback cc(StopOutput, this);
    DebugPrintf("example: Scheduling StopOutput on main thread in 10sec\n");
    pp::Module::Get()->core()->CallOnMainThread(10000, cc, PP_OK);

    // Past sanity checks above, start playback.
    return true;
  }

 private:
  static void SineWaveCallback(void* samples, size_t num_bytes, void* thiz) {
    const double delta_l = kTwoPi * frequency_l / kSampleFrequency;
    const double delta_r = kTwoPi * frequency_r / kSampleFrequency;

    // Verify num_bytes and obtained_sample_frame_count match up.
    const int kNumChannelsForStereo = 2;
    const int kSizeOfSample = sizeof(int16_t);
    if (obtained_sample_frame_count * kNumChannelsForStereo * kSizeOfSample !=
        num_bytes) {
      DebugPrintf("example: In SineWaveCallback, num_bytes does not match\n");
      DebugPrintf("         expected buffer size.\n");
    }

    // Use per channel audio wave value to avoid clicks on buffer boundries.
    double wave_l = reinterpret_cast<MyInstance*>(thiz)->audio_wave_l_;
    double wave_r = reinterpret_cast<MyInstance*>(thiz)->audio_wave_r_;
    const int16_t max_int16 = std::numeric_limits<int16_t>::max();
    int16_t* buf = reinterpret_cast<int16_t*>(samples);
    for (size_t sample = 0; sample < obtained_sample_frame_count; ++sample) {
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
