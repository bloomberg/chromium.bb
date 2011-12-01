// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_AUDIO_H_
#define PAPPI_TESTS_TEST_AUDIO_H_

#include <string>

#include "ppapi/tests/test_case.h"

struct PPB_Audio;
struct PPB_AudioConfig;
struct PPB_Core;

class TestAudio : public TestCase {
 public:
  explicit TestAudio(TestingInstance* instance) : TestCase(instance) {}

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestCreation();
  std::string TestDestroyNoStop();
  std::string TestFailures();

  const PPB_Audio* audio_interface_;
  const PPB_AudioConfig* audio_config_interface_;
  const PPB_Core* core_interface_;
};

#endif  // PAPPI_TESTS_TEST_AUDIO_H_
