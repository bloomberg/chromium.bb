// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "media/base/media_switches.h"
#include "services/audio/service.h"
#include "services/audio/standalone_unittest_catalog_source.h"
#include "services/audio/test/service_lifetime_test_template.h"
#include "services/service_manager/public/cpp/test/test_service.h"
#include "services/service_manager/public/cpp/test/test_service_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace audio {

class StandaloneAudioServiceTest : public testing::Test {
 public:
  StandaloneAudioServiceTest()
      : test_service_manager_(CreateStandaloneUnittestCatalog()),
        test_service_(
            test_service_manager_.RegisterTestInstance("audio_unittests")) {}

 protected:
  service_manager::Connector* connector() { return test_service_.connector(); }

  void SetUp() override {
    base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
    cmd_line->AppendSwitchASCII(switches::kAudioServiceQuitTimeoutMs,
                                base::UintToString(10));
  }

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  service_manager::TestServiceManager test_service_manager_;
  service_manager::TestService test_service_;

  DISALLOW_COPY_AND_ASSIGN(StandaloneAudioServiceTest);
};

INSTANTIATE_TYPED_TEST_CASE_P(StandaloneAudioService,
                              ServiceLifetimeTestTemplate,
                              StandaloneAudioServiceTest);
}  // namespace audio
