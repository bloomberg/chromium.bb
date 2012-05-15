// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_audio.h"

#include <string.h>

#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/testing_instance.h"
#include "ppapi/tests/test_utils.h"

#define ARRAYSIZE_UNSAFE(a) \
  ((sizeof(a) / sizeof(*(a))) / \
   static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))

REGISTER_TEST_CASE(Audio);

const int32_t kMagicValue = 12345;

TestAudio::TestAudio(TestingInstance* instance)
    : TestCase(instance),
      audio_callback_method_(NULL),
      test_callback_(),
      test_done_(false) {
}

TestAudio::~TestAudio() {
}

bool TestAudio::Init() {
  audio_interface_ = static_cast<const PPB_Audio*>(
      pp::Module::Get()->GetBrowserInterface(PPB_AUDIO_INTERFACE));
  audio_config_interface_ = static_cast<const PPB_AudioConfig*>(
      pp::Module::Get()->GetBrowserInterface(PPB_AUDIO_CONFIG_INTERFACE));
  core_interface_ = static_cast<const PPB_Core*>(
      pp::Module::Get()->GetBrowserInterface(PPB_CORE_INTERFACE));
  return audio_interface_ && audio_config_interface_ && core_interface_;
}

void TestAudio::RunTests(const std::string& filter) {
  RUN_TEST(Creation, filter);
  RUN_TEST(DestroyNoStop, filter);
  RUN_TEST(Failures, filter);
  RUN_TEST(AudioCallback1, filter);
  RUN_TEST(AudioCallback2, filter);
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
    PP_AUDIOSAMPLERATE_44100 / 100,  // 10ms @ 44.1kHz
    PP_AUDIOSAMPLERATE_48000 / 100,  // 10ms @ 48kHz
    2 * PP_AUDIOSAMPLERATE_44100 / 100,  // 20ms @ 44.1kHz
    2 * PP_AUDIOSAMPLERATE_48000 / 100,  // 20ms @ 48kHz
    1024,
    2048,
    4096
  };
  PP_AudioSampleRate sample_rate = audio_config_interface_->RecommendSampleRate(
      instance_->pp_instance());
  ASSERT_TRUE(sample_rate == PP_AUDIOSAMPLERATE_NONE ||
              sample_rate == PP_AUDIOSAMPLERATE_44100 ||
              sample_rate == PP_AUDIOSAMPLERATE_48000);
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kSampleRates); i++) {
    PP_AudioSampleRate sample_rate = kSampleRates[i];

    for (size_t j = 0; j < ARRAYSIZE_UNSAFE(kRequestFrameCounts); j++) {
      // Make a config, create the audio resource, and release the config.
      uint32_t request_frame_count = kRequestFrameCounts[j];
      uint32_t frame_count = audio_config_interface_->RecommendSampleFrameCount(
          instance_->pp_instance(), sample_rate, request_frame_count);
      PP_Resource ac = audio_config_interface_->CreateStereo16Bit(
          instance_->pp_instance(), sample_rate, frame_count);
      ASSERT_TRUE(ac);
      PP_Resource audio = audio_interface_->Create(
          instance_->pp_instance(), ac, AudioCallbackTrampoline, this);
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
      audio_callback_method_ = &TestAudio::AudioCallbackTrivial;
      ASSERT_TRUE(audio_interface_->StartPlayback(audio));
      ASSERT_TRUE(audio_interface_->StopPlayback(audio));
      audio_callback_method_ = NULL;

      core_interface_->ReleaseResource(audio);
    }
  }

  PASS();
}

// Test that releasing the resource without calling |StopPlayback()| "works".
std::string TestAudio::TestDestroyNoStop() {
  const PP_AudioSampleRate kSampleRate = PP_AUDIOSAMPLERATE_44100;
  const uint32_t kRequestFrameCount = 2048;

  uint32_t frame_count = audio_config_interface_->RecommendSampleFrameCount(
      instance_->pp_instance(), kSampleRate, kRequestFrameCount);
  PP_Resource ac = audio_config_interface_->CreateStereo16Bit(
      instance_->pp_instance(), kSampleRate, frame_count);
  ASSERT_TRUE(ac);
  audio_callback_method_ = NULL;
  PP_Resource audio = audio_interface_->Create(
      instance_->pp_instance(), ac, AudioCallbackTrampoline, this);
  core_interface_->ReleaseResource(ac);
  ac = 0;

  ASSERT_TRUE(audio);
  ASSERT_TRUE(audio_interface_->IsAudio(audio));

  // Start playback and release the resource.
  audio_callback_method_ = &TestAudio::AudioCallbackTrivial;
  ASSERT_TRUE(audio_interface_->StartPlayback(audio));
  core_interface_->ReleaseResource(audio);
  audio_callback_method_ = NULL;

  PASS();
}

std::string TestAudio::TestFailures() {
  const PP_AudioSampleRate kSampleRate = PP_AUDIOSAMPLERATE_44100;
  const uint32_t kRequestFrameCount = 2048;

  // Test invalid parameters to |Create()|.

  // We want a valid config for some of our tests of |Create()|.
  uint32_t frame_count = audio_config_interface_->RecommendSampleFrameCount(
      instance_->pp_instance(), kSampleRate, kRequestFrameCount);
  PP_Resource ac = audio_config_interface_->CreateStereo16Bit(
      instance_->pp_instance(), kSampleRate, frame_count);
  ASSERT_TRUE(ac);

  // Failure cases should never lead to the callback being called.
  audio_callback_method_ = NULL;

  // Invalid instance -> failure.
  PP_Resource audio = audio_interface_->Create(
      0, ac, AudioCallbackTrampoline, this);
  ASSERT_EQ(0, audio);

  // Invalid config -> failure.
  audio = audio_interface_->Create(
      instance_->pp_instance(), 0, AudioCallbackTrampoline, this);
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

// NOTE: |TestAudioCallback1| and |TestAudioCallback2| assume that the audio
// callback is called at least once. If the audio stream does not start up
// correctly or is interrupted this may not be the case and these tests will
// fail. However, in order to properly test the audio callbacks, we must have
// a configuration where audio can successfully play, so we assume this is the
// case on bots.

// This test starts playback and verifies that:
//  1) the audio callback is actually called;
//  2) that |StopPlayback()| waits for the audio callback to finish.
std::string TestAudio::TestAudioCallback1() {
  const PP_AudioSampleRate kSampleRate = PP_AUDIOSAMPLERATE_44100;
  const uint32_t kRequestFrameCount = 1024;

  uint32_t frame_count = audio_config_interface_->RecommendSampleFrameCount(
      instance_->pp_instance(), kSampleRate, kRequestFrameCount);
  PP_Resource ac = audio_config_interface_->CreateStereo16Bit(
      instance_->pp_instance(), kSampleRate, frame_count);
  ASSERT_TRUE(ac);
  audio_callback_method_ = NULL;
  PP_Resource audio = audio_interface_->Create(
      instance_->pp_instance(), ac, AudioCallbackTrampoline, this);
  core_interface_->ReleaseResource(ac);
  ac = 0;

  // |AudioCallbackTest()| calls |test_callback_|, sleeps a bit, then sets
  // |test_done_|.
  TestCompletionCallback test_callback(instance_->pp_instance());
  test_callback_ = static_cast<pp::CompletionCallback>(
      test_callback).pp_completion_callback();
  test_done_ = false;
  callback_fired_ = false;

  audio_callback_method_ = &TestAudio::AudioCallbackTest;
  ASSERT_TRUE(audio_interface_->StartPlayback(audio));

  // Wait for the audio callback to be called.
  test_callback.WaitForResult();
  ASSERT_EQ(kMagicValue, test_callback.result());

  ASSERT_TRUE(audio_interface_->StopPlayback(audio));

  // |StopPlayback()| should wait for the audio callback to finish.
  ASSERT_TRUE(callback_fired_);
  test_done_ = true;

  // If any more audio callbacks are generated, we should crash (which is good).
  audio_callback_method_ = NULL;
  test_callback_ = PP_CompletionCallback();

  core_interface_->ReleaseResource(audio);

  PASS();
}

// This is the same as |TestAudioCallback1()|, except that instead of calling
// |StopPlayback()|, it just releases the resource.
std::string TestAudio::TestAudioCallback2() {
  const PP_AudioSampleRate kSampleRate = PP_AUDIOSAMPLERATE_44100;
  const uint32_t kRequestFrameCount = 1024;

  uint32_t frame_count = audio_config_interface_->RecommendSampleFrameCount(
      instance_->pp_instance(), kSampleRate, kRequestFrameCount);
  PP_Resource ac = audio_config_interface_->CreateStereo16Bit(
      instance_->pp_instance(), kSampleRate, frame_count);
  ASSERT_TRUE(ac);
  audio_callback_method_ = NULL;
  PP_Resource audio = audio_interface_->Create(
      instance_->pp_instance(), ac, AudioCallbackTrampoline, this);
  core_interface_->ReleaseResource(ac);
  ac = 0;

  // |AudioCallbackTest()| calls |test_callback_|, sleeps a bit, then sets
  // |test_done_|.
  TestCompletionCallback test_callback(instance_->pp_instance());
  test_callback_ = static_cast<pp::CompletionCallback>(
      test_callback).pp_completion_callback();
  test_done_ = false;
  callback_fired_ = false;

  audio_callback_method_ = &TestAudio::AudioCallbackTest;
  ASSERT_TRUE(audio_interface_->StartPlayback(audio));

  // Wait for the audio callback to be called.
  test_callback.WaitForResult();
  ASSERT_EQ(kMagicValue, test_callback.result());

  core_interface_->ReleaseResource(audio);

  // The final release should wait for the audio callback to finish.
  ASSERT_TRUE(callback_fired_);
  test_done_ = true;

  // If any more audio callbacks are generated, we should crash (which is good).
  audio_callback_method_ = NULL;
  test_callback_ = PP_CompletionCallback();

  PASS();
}

// TODO(raymes): Test that actually playback happens correctly, etc.

static void Crash() {
  *static_cast<volatile unsigned*>(NULL) = 0xdeadbeef;
}

// static
void TestAudio::AudioCallbackTrampoline(void* sample_buffer,
                                        uint32_t buffer_size_in_bytes,
                                        void* user_data) {
  TestAudio* thiz = static_cast<TestAudio*>(user_data);

  // Crash if not on the main thread.
  if (thiz->core_interface_->IsMainThread())
    Crash();

  AudioCallbackMethod method = thiz->audio_callback_method_;
  (thiz->*method)(sample_buffer, buffer_size_in_bytes);
}

void TestAudio::AudioCallbackTrivial(void* sample_buffer,
                                     uint32_t buffer_size_in_bytes) {
  memset(sample_buffer, 0, buffer_size_in_bytes);
}

void TestAudio::AudioCallbackTest(void* sample_buffer,
                                  uint32_t buffer_size_in_bytes) {
  if (test_done_)
    Crash();

  if (!callback_fired_) {
    memset(sample_buffer, 0, buffer_size_in_bytes);
    core_interface_->CallOnMainThread(0, test_callback_, kMagicValue);
    callback_fired_ = true;
  }
}
