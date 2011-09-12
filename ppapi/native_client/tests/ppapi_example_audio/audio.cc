// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include <nacl/nacl_log.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cmath>
#include <limits>
#include <string>
#include <nacl/nacl_inttypes.h>

#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/cpp/audio.h"
#include "ppapi/cpp/audio_config.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"


// Most of this example is borrowed from ppapi/examples/audio/audio.cc

// Separate left and right frequency to make sure we didn't swap L & R.
// Sounds pretty horrible, though...
const double kDefaultFrequencyLeft = 400.0;
const double kDefaultFrequencyRight = 1000.0;
const uint32_t kDefaultDuration = 10000;

// This sample frequency is guaranteed to work.
const PP_AudioSampleRate kSampleFrequency = PP_AUDIOSAMPLERATE_44100;
// Buffer size in units of sample frames.
// 4096 is a conservative size that should avoid underruns on most systems.
const uint32_t kSampleFrameCount = 4096;

const double kPi = 3.141592653589;
const double kTwoPi = 2.0 * kPi;

void LogFailure(const char* msg) {
  NaClLog(LOG_ERROR, "\n*** FAILURE **** example: %s", msg);
}

class MyInstance : public pp::Instance {
 private:
  void ParseArgs(uint32_t argc, const char* argn[], const char* argv[]) {
    NaClLog(1, "example: parsing %d args\n", static_cast<int>(argc));
    for (uint32_t i = 0; i < argc; ++i) {
      NaClLog(1, "example: arg %d: [%s] [%s]\n",
              static_cast<int>(i), argn[i], argv[i]);
      const std::string tag = argn[i];
      if (tag == "frequency_l") frequency_l_ = strtod(argv[i], 0);
      if (tag == "frequency_r") frequency_r_ = strtod(argv[i], 0);
      if (tag == "amplitude_l") amplitude_l_ = strtod(argv[i], 0);
      if (tag == "amplitude_r") amplitude_r_ = strtod(argv[i], 0);
      if (tag == "duration_msec") duration_msec_ = strtod(argv[i], 0);
      if (tag == "basic_tests") basic_tests_ = (0 != atoi(argv[i]));
      if (tag == "stress_tests") stress_tests_ = (0 != atoi(argv[i]));
      if (tag == "headless") headless_ = (0 != atoi(argv[i]));
      // ignore other tags
    }
  }

 public:
  explicit MyInstance(PP_Instance instance)
      : pp::Instance(instance),
        config_(NULL),
        audio_(NULL),
        audio_wave_l_(0.0),
        audio_wave_r_(0.0),
        frequency_l_(kDefaultFrequencyLeft),
        frequency_r_(kDefaultFrequencyRight),
        amplitude_l_(1.0),
        amplitude_r_(1.0),
        headless_(false),
        basic_tests_(false),
        stress_tests_(false),
        duration_msec_(kDefaultDuration),
        obtained_sample_frame_count_(0),
        callback_count_(0) {}

  virtual void HandleMessage(const pp::Var& message) {
    NaClLog(1, "example: received HandleMessage\n");
    if (message.is_string()) {
      if (message.AsString() == "StartPlayback") {
        StartOutput();
      }
    }
  }

  void StartOutput() {
    bool audio_start_playback = audio_->StartPlayback();
    CHECK(true == audio_start_playback);
    NaClLog(1, "example: frequencies are %f %f\n", frequency_l_, frequency_r_);
    NaClLog(1, "example: amplitudes are %f %f\n", amplitude_l_, amplitude_r_);
    NaClLog(1, "example: Scheduling StopOutput on main thread in %"
            NACL_PRIu32"msec\n", duration_msec_);
    // Schedule a callback in duration_msec_ to stop audio output
    pp::CompletionCallback cc(StopOutput, this);
    pp::Module::Get()->core()->CallOnMainThread(duration_msec_, cc, PP_OK);
  }

  static void StopOutput(void* user_data, int32_t err) {
    MyInstance* instance = static_cast<MyInstance*>(user_data);

    const int kMaxResult = 256;
    char result[kMaxResult];
    NaClLog(1, "example: StopOutput() invoked on main thread\n");
    if (PP_OK == err) {
      if (instance->audio_->StopPlayback()) {
        // In headless mode, the build bots may not have an audio driver, in
        // which case the callback won't be invoked.
        // TODO(nfullagar): Other ways to determine if machine has audio
        // capabilities. Currently PPAPI returns a valid resource regardless.
        if ((instance->callback_count_ >= 2) || instance->headless_) {
          snprintf(result, kMaxResult, "StopOutput:PASSED");
        } else {
          snprintf(result, kMaxResult, "StopOutput:FAILED - too "
              "few callbacks (only %d callbacks detected)",
              static_cast<int>(instance->callback_count_));
        }
      }
    } else {
      snprintf(result, kMaxResult,
          "StopOutput: FAILED - returned err is %d", static_cast<int>(err));
    }
    // Release audio & config instance.
    delete instance->audio_;
    delete instance->config_;
    instance->audio_ = NULL;
    instance->config_ = NULL;
    // At this point the test has finished, report result.
    pp::Var message(result);
    instance->PostMessage(message);
  }

  // To enable basic tests, use basic_tests="1" in the embed tag.
  void BasicTests() {
    // Verify obtained_sample_frame_count isn't out of range.
    CHECK(obtained_sample_frame_count_ >= PP_AUDIOMINSAMPLEFRAMECOUNT);
    CHECK(obtained_sample_frame_count_ <= PP_AUDIOMAXSAMPLEFRAMECOUNT);
    // Do some sanity checks below; verify c & cpp interfaces agree.
    // Note: This is test code and is not normally needed for an application.
    PPB_GetInterface get_browser_interface =
        pp::Module::Get()->get_browser_interface();
    const struct PPB_AudioConfig* audio_config_interface =
        static_cast<const struct PPB_AudioConfig*>(
        get_browser_interface(PPB_AUDIO_CONFIG_INTERFACE));
    const struct PPB_Audio* audio_interface =
        static_cast<const struct PPB_Audio*>(
        get_browser_interface(PPB_AUDIO_INTERFACE));
    PP_Resource audio_config_resource = config_->pp_resource();
    PP_Resource audio_resource = audio_->pp_resource();
    NaClLog(1, "example: audio config resource: %"NACL_PRId32"\n",
            audio_config_resource);
    NaClLog(1, "example: audio resource: %"NACL_PRId32"\n", audio_resource);
    CHECK(PP_TRUE == audio_config_interface->
        IsAudioConfig(audio_config_resource));
    CHECK(PP_TRUE == audio_interface->IsAudio(audio_resource));
    CHECK(PP_FALSE == audio_config_interface->IsAudioConfig(audio_resource));
    CHECK(PP_FALSE == audio_interface->IsAudio(audio_config_resource));
    CHECK(audio_interface->GetCurrentConfig(audio_resource) ==
        audio_config_resource);
    CHECK(0 == audio_interface->GetCurrentConfig(audio_config_resource));
    CHECK(audio_config_interface->GetSampleRate(audio_config_resource) ==
        config_->sample_rate());
    CHECK(audio_config_interface->GetSampleFrameCount(audio_config_resource) ==
        config_->sample_frame_count());
    CHECK(audio_->config().pp_resource() == audio_config_resource);
  }

  // To enable stress tests, use stress_tests="1" in the embed tag.
  void StressTests() {
    // Attempt to create many audio devices, then immediately shut them down.
    // Chrome may generate some warnings on the console, but should not crash.
    const int kNumManyAudio = 1000;
    pp::Audio* many_audio[kNumManyAudio];
    for (int i = 0; i < kNumManyAudio; ++i)
      many_audio[i] = new pp::Audio(this, *config_, SilenceCallback, this);
    for (int i = 0; i < kNumManyAudio; ++i)
      CHECK(true == many_audio[i]->StartPlayback());
    for (int i = 0; i < kNumManyAudio; ++i)
      delete many_audio[i];
  }

  void TestSuite() {
    if (basic_tests_)
      BasicTests();
    if (stress_tests_)
      StressTests();
  }

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    ParseArgs(argc, argn, argv);
    obtained_sample_frame_count_ = pp::AudioConfig::RecommendSampleFrameCount(
        kSampleFrequency, kSampleFrameCount);
    config_ = new
       pp::AudioConfig(this, kSampleFrequency, obtained_sample_frame_count_);
    CHECK(NULL != config_);
    audio_ = new pp::Audio(this, *config_, SineWaveCallback, this);
    CHECK(NULL != audio_);
    // Run through test suite before attempting real playback.
    TestSuite();
    return true;
  }

 private:
  static void SineWaveCallback(void* samples, uint32_t num_bytes, void* thiz) {
    MyInstance* instance = reinterpret_cast<MyInstance*>(thiz);
    const double delta_l = kTwoPi * instance->frequency_l_ / kSampleFrequency;
    const double delta_r = kTwoPi * instance->frequency_r_ / kSampleFrequency;

    // Verify num_bytes and obtained_sample_frame_count match up.
    const int kNumChannelsForStereo = 2;
    const int kSizeOfSample = sizeof(int16_t);
    const size_t single_sample = kNumChannelsForStereo * kSizeOfSample;

    // CHECK inside callback is only for testing purposes.
    CHECK(instance->obtained_sample_frame_count_ * single_sample == num_bytes);

    // Use per channel audio wave value to avoid clicks on buffer boundries.
    double wave_l = instance->audio_wave_l_;
    double wave_r = instance->audio_wave_r_;
    const int16_t max_int16 = std::numeric_limits<int16_t>::max();
    int16_t* buf = reinterpret_cast<int16_t*>(samples);
    for (size_t i = 0; i < instance->obtained_sample_frame_count_; ++i) {
      const double l = sin(wave_l) * instance->amplitude_l_ * max_int16;
      const double r = sin(wave_r) * instance->amplitude_r_ * max_int16;
      *buf++ = static_cast<int16_t>(l);
      *buf++ = static_cast<int16_t>(r);
      // Add delta, keep within -kTwoPi..kTwoPi to preserve precision.
      wave_l += delta_l;
      if (wave_l > kTwoPi)
        wave_l -= kTwoPi * 2.0;
      wave_r += delta_r;
      if (wave_r > kTwoPi)
        wave_r -= kTwoPi * 2.0;
    }
    // Store current value to use as starting point for next callback.
    instance->audio_wave_l_ = wave_l;
    instance->audio_wave_r_ = wave_r;

    ++instance->callback_count_;
  }

  static void SilenceCallback(void* samples, uint32_t num_bytes, void* thiz) {
    memset(samples, 0, num_bytes);
  }

  // Audio config resource. Allocated in Init().
  pp::AudioConfig* config_;

  // Audio resource. Allocated in Init().
  pp::Audio* audio_;

  // Current audio wave position, used to prevent sine wave skips
  // on buffer boundaries.
  double audio_wave_l_;
  double audio_wave_r_;

  double frequency_l_;
  double frequency_r_;

  double amplitude_l_;
  double amplitude_r_;

  bool headless_;

  bool basic_tests_;
  bool stress_tests_;

  uint32_t duration_msec_;
  uint32_t obtained_sample_frame_count_;

  int callback_count_;
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
  NaClLogModuleInit();
  return new MyModule();
}

}  // namespace pp
