// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_audio.h"

#include <string.h>

#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/testing_instance.h"

#define ARRAYSIZE_UNSAFE(a) \
  ((sizeof(a) / sizeof(*(a))) / \
   static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))

REGISTER_TEST_CASE(Audio);

bool TestAudio::Init() {
  audio_interface_ = static_cast<PPB_Audio const*>(
      pp::Module::Get()->GetBrowserInterface(PPB_AUDIO_INTERFACE));
  audio_config_interface_ = static_cast<PPB_AudioConfig const*>(
      pp::Module::Get()->GetBrowserInterface(PPB_AUDIO_CONFIG_INTERFACE));
  core_interface_ = static_cast<const PPB_Core*>(
      pp::Module::Get()->GetBrowserInterface(PPB_CORE_INTERFACE));
  return audio_interface_ && audio_config_interface_ && core_interface_;
}

void TestAudio::RunTests(const std::string& filter) {
  RUN_TEST(Creation, filter);
  RUN_TEST(DestroyNoStop, filter);
  RUN_TEST(Failures, filter);
}

// A trivial audio callback to provide when we're no interested in whether it
// gets called or not. It just clears the buffer so we don't play noise.
static void TrivialAudioCallback(void* sample_buffer,
                                 uint32_t buffer_size_in_bytes,
                                 void* user_data) {
  memset(sample_buffer, 0, buffer_size_in_bytes);
}

// Test creating audio resources for all guaranteed sample rates and various
// frame counts.
std::string TestAudio::TestCreation() {
  static const PP_AudioSampleRate kSampleRates[] = {
    PP_AUDIOSAMPLERATE_44100,
    PP_AUDIOSAMPLERATE_48000
  };
  static const uint32_t kRequestFrameCounts[] = {
    PP_AUDIOMINSAMPLEFRAMECOUNT,
    PP_AUDIOMAXSAMPLEFRAMECOUNT,
    // Include some "okay-looking" frame counts; check their validity below.
    1024,
    2048,
    4096
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kSampleRates); i++) {
    PP_AudioSampleRate sample_rate = kSampleRates[i];

    for (size_t j = 0; j < ARRAYSIZE_UNSAFE(kRequestFrameCounts); j++) {
      // Make a config, create the audio resource, and release the config.
      uint32_t request_frame_count = kRequestFrameCounts[j];
      uint32_t frame_count = audio_config_interface_->RecommendSampleFrameCount(
          sample_rate, request_frame_count);
      PP_Resource ac = audio_config_interface_->CreateStereo16Bit(
          instance_->pp_instance(), sample_rate, frame_count);
      ASSERT_TRUE(ac);
      PP_Resource audio = audio_interface_->Create(
          instance_->pp_instance(), ac, TrivialAudioCallback, NULL);
      core_interface_->ReleaseResource(ac);
      ac = 0;

      ASSERT_TRUE(audio);
      ASSERT_TRUE(audio_interface_->IsAudio(audio));

      // Check that the config returned for |audio| matches what we gave it.
      ac = audio_interface_->GetCurrentConfig(audio);
      ASSERT_TRUE(ac);
      ASSERT_TRUE(audio_config_interface_->IsAudioConfig(ac));
      ASSERT_EQ(sample_rate, audio_config_interface_->GetSampleRate(ac));
      ASSERT_EQ(frame_count, audio_config_interface_->GetSampleFrameCount(ac));
      core_interface_->ReleaseResource(ac);
      ac = 0;

      // Start and stop audio playback. The documentation indicates that
      // |StartPlayback()| and |StopPlayback()| may fail, but gives no
      // indication as to why ... so check that they succeed.
      ASSERT_TRUE(audio_interface_->StartPlayback(audio));
      ASSERT_TRUE(audio_interface_->StopPlayback(audio));

      core_interface_->ReleaseResource(audio);
    }
  }

  PASS();
}

// Test that releasing the resource without calling |StopPlayback()| "works".
// TODO(viettrungluu): Figure out how to check that |StopPlayback()| properly
// waits for in-flight callbacks.
std::string TestAudio::TestDestroyNoStop() {
  const PP_AudioSampleRate kSampleRate = PP_AUDIOSAMPLERATE_44100;
  const uint32_t kRequestFrameCount = 2048;

  uint32_t frame_count = audio_config_interface_->RecommendSampleFrameCount(
      kSampleRate, kRequestFrameCount);
  PP_Resource ac = audio_config_interface_->CreateStereo16Bit(
      instance_->pp_instance(), kSampleRate, frame_count);
  ASSERT_TRUE(ac);
  PP_Resource audio = audio_interface_->Create(
      instance_->pp_instance(), ac, TrivialAudioCallback, NULL);
  core_interface_->ReleaseResource(ac);
  ac = 0;

  ASSERT_TRUE(audio);
  ASSERT_TRUE(audio_interface_->IsAudio(audio));

  // Start playback and release the resource.
  ASSERT_TRUE(audio_interface_->StartPlayback(audio));
  core_interface_->ReleaseResource(audio);

  PASS();
}

std::string TestAudio::TestFailures() {
  const PP_AudioSampleRate kSampleRate = PP_AUDIOSAMPLERATE_44100;
  const uint32_t kRequestFrameCount = 2048;

  // Test invalid parameters to |Create()|.

  // We want a valid config for some of our tests of |Create()|.
  uint32_t frame_count = audio_config_interface_->RecommendSampleFrameCount(
      kSampleRate, kRequestFrameCount);
  PP_Resource ac = audio_config_interface_->CreateStereo16Bit(
      instance_->pp_instance(), kSampleRate, frame_count);
  ASSERT_TRUE(ac);

  // Invalid instance -> failure.
  PP_Resource audio = audio_interface_->Create(
      0, ac, TrivialAudioCallback, NULL);
  ASSERT_EQ(0, audio);

  // Invalid config -> failure.
  audio = audio_interface_->Create(
      instance_->pp_instance(), 0, TrivialAudioCallback, NULL);
  ASSERT_EQ(0, audio);

  // Null callback -> failure.
  audio = audio_interface_->Create(
      instance_->pp_instance(), ac, NULL, NULL);
  ASSERT_EQ(0, audio);

  core_interface_->ReleaseResource(ac);
  ac = 0;

  // Test the other functions with an invalid audio resource.
  ASSERT_FALSE(audio_interface_->IsAudio(0));
  ASSERT_EQ(0, audio_interface_->GetCurrentConfig(0));
  ASSERT_FALSE(audio_interface_->StartPlayback(0));
  ASSERT_FALSE(audio_interface_->StopPlayback(0));

  PASS();
}

// TODO(viettrungluu): Test that callbacks get called, playback happens, etc.
