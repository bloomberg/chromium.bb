// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_AUDIO_H_
#define PAPPI_TESTS_TEST_AUDIO_H_

#include <string>

#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/tests/test_case.h"

class TestAudio : public TestCase {
 public:
  explicit TestAudio(TestingInstance* instance);
  ~TestAudio();

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestCreation();
  std::string TestDestroyNoStop();
  std::string TestFailures();
  std::string TestAudioCallback1();
  std::string TestAudioCallback2();

  // Calls |audio_callback_method_| (where |user_data| is "this").
  static void AudioCallbackTrampoline(void* sample_buffer,
                                      uint32_t buffer_size_in_bytes,
                                      void* user_data);

  typedef void (TestAudio::*AudioCallbackMethod)(void* sample_buffer,
                                                 uint32_t buffer_size_in_bytes);

  // Method called by |AudioCallbackTrampoline()|. Set only when the callback
  // can't be running (before |StartPlayback()|, after |StopPlayback()| or
  // releasing the last reference to the audio resource).
  AudioCallbackMethod audio_callback_method_;

  // An |AudioCallbackMethod| that just clears |sample_buffer|.
  void AudioCallbackTrivial(void* sample_buffer, uint32_t buffer_size_in_bytes);

  // |AudioCallbackMethod| used by |TestAudioCallbackN()|.
  void AudioCallbackTest(void* sample_buffer, uint32_t buffer_size_in_bytes);

  // Used by |TestAudioCallbackN()|.
  PP_CompletionCallback test_callback_;

  bool test_done_;
  bool callback_fired_;

  // Raw C-level interfaces, set in |Init()|; do not modify them elsewhere.
  const PPB_Audio* audio_interface_;
  const PPB_AudioConfig* audio_config_interface_;
  const PPB_Core* core_interface_;
};

#endif  // PAPPI_TESTS_TEST_AUDIO_H_
