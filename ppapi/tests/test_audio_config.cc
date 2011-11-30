// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_audio_config.h"

#include "base/basictypes.h"  // For |arraysize()|.
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(AudioConfig);

bool TestAudioConfig::Init() {
  audio_config_interface_ = static_cast<PPB_AudioConfig const*>(
      pp::Module::Get()->GetBrowserInterface(PPB_AUDIO_CONFIG_INTERFACE));
  core_interface_ = static_cast<const PPB_Core*>(
      pp::Module::Get()->GetBrowserInterface(PPB_CORE_INTERFACE));
  return audio_config_interface_ && core_interface_;
}

void TestAudioConfig::RunTests(const std::string& filter) {
  RUN_TEST(Everything, filter);
}

std::string TestAudioConfig::TestEverything() {
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

  for (size_t i = 0; i < arraysize(kSampleRates); i++) {
    PP_AudioSampleRate sample_rate = kSampleRates[i];

    for (size_t j = 0; j < arraysize(kRequestFrameCounts); j++) {
      uint32_t request_frame_count = kRequestFrameCounts[j];
      ASSERT_TRUE(request_frame_count >= PP_AUDIOMINSAMPLEFRAMECOUNT);
      ASSERT_TRUE(request_frame_count <= PP_AUDIOMAXSAMPLEFRAMECOUNT);

      uint32_t frame_count = audio_config_interface_->RecommendSampleFrameCount(
          sample_rate, request_frame_count);
      ASSERT_TRUE(frame_count >= PP_AUDIOMINSAMPLEFRAMECOUNT);
      ASSERT_TRUE(frame_count <= PP_AUDIOMAXSAMPLEFRAMECOUNT);

      PP_Resource ac = audio_config_interface_->CreateStereo16Bit(
          instance_->pp_instance(), sample_rate, frame_count);
      ASSERT_TRUE(ac);
      ASSERT_TRUE(audio_config_interface_->IsAudioConfig(ac));
      ASSERT_EQ(sample_rate, audio_config_interface_->GetSampleRate(ac));
      ASSERT_EQ(frame_count, audio_config_interface_->GetSampleFrameCount(ac));

      core_interface_->ReleaseResource(ac);
    }
  }

  PASS();
}
