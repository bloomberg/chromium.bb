// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/settings/settings.h"

#include "base/command_line.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

class ReporterSettingsTest : public testing::Test {
 protected:
  Settings* ReinitializeSettings(const base::CommandLine& command_line) {
    Settings* settings = Settings::GetInstance();
    settings->Initialize(command_line, TargetBinary::kReporter);
    return settings;
  }
};

TEST_F(ReporterSettingsTest, ReporterLogsPermissions) {
  for (bool sber : {false, true}) {
    for (bool uploading_enabled : {false, true}) {
      base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
      if (sber)
        command_line.AppendSwitch(kExtendedSafeBrowsingEnabledSwitch);
      if (!uploading_enabled)
        command_line.AppendSwitch(kNoReportUploadSwitch);

      Settings* settings = ReinitializeSettings(command_line);
      EXPECT_EQ(sber, settings->logs_collection_enabled());
      EXPECT_EQ(sber && uploading_enabled, settings->logs_upload_allowed());
    }
  }
}

}  // namespace chrome_cleaner
