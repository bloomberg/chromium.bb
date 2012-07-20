// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl/nacl_inttypes.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "native_client/tests/ppapi_test_lib/test_interface.h"
#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_instance.h"

namespace {

struct AudioTestData {
  PP_Resource audio_config;
  PP_Resource audio;
  int callback_count;
  int retry_count;
  bool force_pass;
  AudioTestData() :
    audio_config(kInvalidResource),
    audio(kInvalidResource),
    callback_count(0),
    retry_count(0),
    force_pass(false) { ; }
};

uint32_t RateToSampleFrameCount(PP_AudioSampleRate sample_rate) {
  // For 44100Hz, return a 10ms sample frame count of 441
  // For 48000Hz, return a 10ms sample frame count of 480
  return sample_rate / 100;
}

// Helper to create an audio configuration.  If no sample rate is recommended,
// this function will return an invalid resource.  Otherwise, it will return
// an audio config resource, using the recommended sample rate and the
// recommended sample frame count.
PP_Resource CreateAudioConfig() {
  PP_Resource audio_config = kInvalidResource;
  PP_AudioSampleRate sample_rate = PP_AUDIOSAMPLERATE_NONE;
  sample_rate = PPBAudioConfig()->RecommendSampleRate(pp_instance());
  if (PP_AUDIOSAMPLERATE_NONE != sample_rate) {
    uint32_t sample_frame_count = 0;
    sample_frame_count = PPBAudioConfig()->RecommendSampleFrameCount(
        pp_instance(), sample_rate, RateToSampleFrameCount(sample_rate));
    audio_config = PPBAudioConfig()->CreateStereo16Bit(pp_instance(),
        sample_rate, sample_frame_count);
  }
  return audio_config;
}

// Helper to destroy an audio configuration.
void DestroyAudioConfig(PP_Resource audio_config) {
  if (kInvalidResource != audio_config) {
    PPBCore()->ReleaseResource(audio_config);
  }
}

// Simple callback for audio test.  Plays silence and (optional) counts how many
// callbacks occured.
void SimpleCallback(void* samples, uint32_t num_bytes, void* user_data) {
  int* num_callbacks = static_cast<int*>(user_data);
  memset(samples, 0, num_bytes);
  if (NULL != num_callbacks)
    *num_callbacks = *num_callbacks + 1;
}

// Helper to create an audio resource.
PP_Resource CreateAudio(PP_Resource config, void* user_data) {
  PP_Resource audio = PPBAudio()->Create(pp_instance(), config, SimpleCallback,
      user_data);
  return audio;
}

// Helper to destroy an audio resource.
void DestroyAudio(PP_Resource audio) {
  if (kInvalidResource != audio) {
    PPBCore()->ReleaseResource(audio);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Test Cases
////////////////////////////////////////////////////////////////////////////////

// Tests PPB_AudioConfig::RecommendSampleRate().
void TestAudioConfigRecommendSampleRate() {
  PP_AudioSampleRate sample_rate = PP_AUDIOSAMPLERATE_NONE;
  sample_rate = PPBAudioConfig()->RecommendSampleRate(pp_instance());
  EXPECT(sample_rate == PP_AUDIOSAMPLERATE_NONE ||
         sample_rate == PP_AUDIOSAMPLERATE_44100 ||
         sample_rate == PP_AUDIOSAMPLERATE_48000);
  TEST_PASSED;
}

// Tests PPB_AudioConfig::RecommendSampleFrameCount().
void TestAudioConfigRecommendSampleFrameCount() {
  uint32_t sample_frame_count = 0;
  const uint32_t sample_frame_count_44100 =
      RateToSampleFrameCount(PP_AUDIOSAMPLERATE_44100);
  const uint32_t sample_frame_count_48000 =
      RateToSampleFrameCount(PP_AUDIOSAMPLERATE_48000);
  sample_frame_count = PPBAudioConfig()->RecommendSampleFrameCount(
      pp_instance(), PP_AUDIOSAMPLERATE_44100, sample_frame_count_44100);
  EXPECT(sample_frame_count >= PP_AUDIOMINSAMPLEFRAMECOUNT &&
         sample_frame_count >= sample_frame_count_44100 &&
         sample_frame_count <= PP_AUDIOMAXSAMPLEFRAMECOUNT);
  sample_frame_count = PPBAudioConfig()->RecommendSampleFrameCount(
      pp_instance(), PP_AUDIOSAMPLERATE_48000, sample_frame_count_48000);
  EXPECT(sample_frame_count >= PP_AUDIOMINSAMPLEFRAMECOUNT &&
         sample_frame_count >= sample_frame_count_48000 &&
         sample_frame_count <= PP_AUDIOMAXSAMPLEFRAMECOUNT);
  TEST_PASSED;
}

// Tests PPB_AudioConfig::CreateStereo16Bit().
void TestAudioConfigCreateStereo16Bit() {
  PP_Resource audio_config = CreateAudioConfig();
  if (kInvalidResource != audio_config) {
    DestroyAudioConfig(audio_config);
  }
  TEST_PASSED;
}

// Tests PPB_AudioConfig::IsAudioConfig().
void TestIsAudioConfig() {
  PP_Resource audio_config = CreateAudioConfig();
  if (kInvalidResource != audio_config) {
    EXPECT(PPBAudioConfig()->IsAudioConfig(audio_config));
    EXPECT(!PPBAudioConfig()->IsAudioConfig(kInvalidResource));
    DestroyAudioConfig(audio_config);
  }
  TEST_PASSED;
}

// Tests PPB_AudioConfig::GetSampleRate().
void TestAudioConfigGetSampleRate() {
  PP_Resource audio_config = CreateAudioConfig();
  if (kInvalidResource != audio_config) {
    PP_AudioSampleRate sample_rate = PPBAudioConfig()->RecommendSampleRate(
        pp_instance());
    EXPECT(PPBAudioConfig()->GetSampleRate(audio_config) == sample_rate);
    DestroyAudioConfig(audio_config);
  }
  TEST_PASSED;
}

// Tests PPB_AudioConfig::GetSampleFrameCount().
void TestAudioConfigGetSampleFrameCount() {
  PP_Resource audio_config = CreateAudioConfig();
  if (kInvalidResource != audio_config) {
    PP_AudioSampleRate sample_rate = PPBAudioConfig()->RecommendSampleRate(
        pp_instance());
    uint32_t sample_frame_count = PPBAudioConfig()->RecommendSampleFrameCount(
        pp_instance(), sample_rate, RateToSampleFrameCount(sample_rate));
    EXPECT(PPBAudioConfig()->GetSampleFrameCount(audio_config) ==
        sample_frame_count);
    DestroyAudioConfig(audio_config);
  }
  TEST_PASSED;
}

// Tests PPB_Audio::Create().
void TestAudioCreate() {
  PP_Resource audio_config = CreateAudioConfig();
  if (kInvalidResource != audio_config) {
    PP_Resource audio = CreateAudio(audio_config, NULL);
    EXPECT(kInvalidResource != audio);
    DestroyAudio(audio);
    DestroyAudioConfig(audio_config);
    audio = PPBAudio()->Create(pp_instance(), kInvalidResource, SimpleCallback,
        NULL);
    EXPECT(kInvalidResource == audio);
  }
  TEST_PASSED;
}

// Tests PPB_Audio::IsAudio().
void TestIsAudio() {
  PP_Resource audio_config = CreateAudioConfig();
  if (kInvalidResource != audio_config) {
    PP_Resource audio = CreateAudio(audio_config, NULL);
    EXPECT(kInvalidResource != audio);
    EXPECT(PPBAudio()->IsAudio(audio));
    EXPECT(!PPBAudio()->IsAudio(audio_config));
    DestroyAudio(audio);
    DestroyAudioConfig(audio_config);
  }
  TEST_PASSED;
}

// Tests PPB_Audio::GetCurrentConfig().
void TestAudioGetCurrentConfig() {
  PP_Resource audio_config = CreateAudioConfig();
  if (kInvalidResource != audio_config) {
    PP_Resource audio = CreateAudio(audio_config, NULL);
    EXPECT(kInvalidResource != audio);
    EXPECT(PPBAudio()->GetCurrentConfig(audio) == audio_config);
    DestroyAudio(audio);
    DestroyAudioConfig(audio_config);
  }
  TEST_PASSED;
}

// Tests PPB_Audio::StartPlayback() && StopPlayback().
void TestAudioBasicPlayback() {
  PP_Resource audio_config = CreateAudioConfig();
  if (kInvalidResource != audio_config) {
    int callback_count = 0;
    PP_Resource audio;
    audio = CreateAudio(audio_config, &callback_count);
    EXPECT(kInvalidResource != audio);
    EXPECT(PPBAudio()->StartPlayback(audio));
    EXPECT(PPBAudio()->StopPlayback(audio));
    DestroyAudio(audio);
    DestroyAudioConfig(audio_config);
  }
  TEST_PASSED;
}

// Completion callback invoked when short playback is successful.
void TestAudioShortPlaybackDone(void* data, int32_t result) {
}

// Completion callback invoked from TestAudioShortPlayback()
void TestAudioShortPlaybackCallback(void* data, int32_t result) {
  AudioTestData* playback_test = static_cast<AudioTestData*>(data);
  if (playback_test) {
    // Stop the test if at least one audio callback occurred, or if the
    // test is forced to succeed (such as running on a bot that has no audio
    // device.)
    if (playback_test->callback_count > 0 || playback_test->force_pass) {
      if (kInvalidResource != playback_test->audio) {
        EXPECT(PPBAudio()->StopPlayback(playback_test->audio));
        DestroyAudio(playback_test->audio);
        DestroyAudioConfig(playback_test->audio_config);
      }
      delete playback_test;
      PP_CompletionCallback done_callback =
          MakeTestableCompletionCallback("TestAudioShortPlaybackDone",
                                         TestAudioShortPlaybackDone,
                                         NULL);
      PPBCore()->CallOnMainThread(0, done_callback, PP_OK);
    } else {
      // Otherwise, check again, but eventually give up after a few tries.
      // When this test is run on some VMs, it may take few seconds to spin up.
      playback_test->retry_count++;
      const int kMaxTriesWaitingForAudioCallback = 20;
      EXPECT(playback_test->retry_count < kMaxTriesWaitingForAudioCallback);
      if (playback_test->retry_count < kMaxTriesWaitingForAudioCallback) {
        const int kQuarterSecondDelay = 1000 / 4;
        PP_CompletionCallback completion_callback =
            PP_MakeCompletionCallback(TestAudioShortPlaybackCallback,
                                      playback_test);
        PPBCore()->CallOnMainThread(kQuarterSecondDelay, completion_callback,
            PP_OK);
      }
      return;
    }
  }
}

// Tests PPB_Audio::StartPlayback() && StopPlayback() for a short period of
// time; completion callback checks to see if audio callback was invoked.
void TestAudioShortPlayback() {
  PP_Resource audio_config = CreateAudioConfig();
  AudioTestData *playback_test = new AudioTestData();
  EXPECT(playback_test != NULL);
  playback_test->audio_config = audio_config;
  if (kInvalidResource != audio_config) {
    playback_test->audio = CreateAudio(audio_config,
        &playback_test->callback_count);
    EXPECT(playback_test->audio != kInvalidResource);
    EXPECT(PPBAudio()->StartPlayback(playback_test->audio));
  } else {
    // No audio device, so force this test to pass in the completion callback.
    playback_test->force_pass = true;
  }
  const int kQuarterSecondDelay = 1000 / 4;
  PP_CompletionCallback completion_callback =
      PP_MakeCompletionCallback(TestAudioShortPlaybackCallback, playback_test);
  PPBCore()->CallOnMainThread(kQuarterSecondDelay, completion_callback, PP_OK);
  TEST_PASSED;
}

// Tests various combinations of simultanious audio playback.  A normal
// application would only open one audio device at a time, this test attemps
// to have multiple open, and performs repeated StartPlayback() and
// StopPlayback() calls.
void TestAudioStress() {
  PP_Resource audio_config = CreateAudioConfig();
  if (kInvalidResource != audio_config) {
    const int kNumRepeat = 10;
    const int kNumConcurrent = 10;
    for (int k = 0; k < kNumRepeat; k++) {
      PP_Resource audio[kNumConcurrent];
      for (int j = 0; j < kNumConcurrent; j++) {
        audio[j] = CreateAudio(audio_config, NULL);
        EXPECT(audio[j] != kInvalidResource);
        EXPECT(PPBAudio()->StartPlayback(audio[j]));
      }
      for (int j = 0; j < kNumConcurrent; j++) {
        const int kNumStopStartCycles = 10;
        for (int i = 0; i < kNumStopStartCycles; i++) {
          EXPECT(PPBAudio()->StopPlayback(audio[j]));
          EXPECT(PPBAudio()->StartPlayback(audio[j]));
        }
      }
      for (int j = 0; j < kNumConcurrent; j++) {
        EXPECT(PPBAudio()->StopPlayback(audio[j]));
        DestroyAudio(audio[j]);
      }
    }
    DestroyAudioConfig(audio_config);
  }
  TEST_PASSED;
}

}  // namespace

void SetupTests() {
  RegisterTest("TestAudioConfigRecommendSampleRate",
      TestAudioConfigRecommendSampleRate);
  RegisterTest("TestAudioConfigRecommendSampleFrameCount",
      TestAudioConfigRecommendSampleFrameCount);
  RegisterTest("TestAudioConfigCreateStereo16Bit",
      TestAudioConfigCreateStereo16Bit);
  RegisterTest("TestIsAudioConfig", TestIsAudioConfig);
  RegisterTest("TestAudioConfigGetSampleRate", TestAudioConfigGetSampleRate);
  RegisterTest("TestAudioConfigGetSampleFrameCount",
      TestAudioConfigGetSampleFrameCount);
  RegisterTest("TestAudioCreate", TestAudioCreate);
  RegisterTest("TestIsAudio", TestIsAudio);
  RegisterTest("TestAudioGetCurrentConfig", TestAudioGetCurrentConfig);
  RegisterTest("TestAudioBasicPlayback", TestAudioBasicPlayback);
  RegisterTest("TestAudioShortPlayback", TestAudioShortPlayback);
  RegisterTest("TestAudioStress", TestAudioStress);
}

void SetupPluginInterfaces() {
  // none
}
