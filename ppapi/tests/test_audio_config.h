// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_AUDIO_CONFIG_H_
#define PAPPI_TESTS_TEST_AUDIO_CONFIG_H_

#include <string>

#include "ppapi/tests/test_case.h"

struct PPB_AudioConfig;
struct PPB_Core;

class TestAudioConfig : public TestCase {
 public:
  explicit TestAudioConfig(TestingInstance* instance) : TestCase(instance) {}

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestValidConfigs();
  std::string TestInvalidConfigs();

  const PPB_AudioConfig* audio_config_interface_;
  const PPB_Core* core_interface_;
};

#endif  // PAPPI_TESTS_TEST_AUDIO_CONFIG_H_
