// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Fuzzer launcher script tests.

#include <string>
#include <vector>


#include "base/command_line.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
using testing::EndsWith;
using testing::StartsWith;

TEST(FuzzerLauncherTest, Dict) {
  base::FilePath exe_path;
  PathService::Get(base::FILE_EXE, &exe_path);
  std::string launcher_path =
    exe_path.DirName().Append("test_dict_launcher.sh").value();

  std::string output;
  base::CommandLine cmd({{launcher_path, "-custom_option"}});
  bool success = base::GetAppOutputAndError(cmd, &output);
  EXPECT_TRUE(success);
  std::vector<std::string> fuzzer_args = base::SplitString(
      output, "\n\r", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  EXPECT_EQ(3UL, fuzzer_args.size());

  EXPECT_THAT(fuzzer_args[0], EndsWith("print_args.py"));
  EXPECT_THAT(fuzzer_args[1], StartsWith("-dict"));
  EXPECT_THAT(fuzzer_args[1], EndsWith("test_dict"));
  EXPECT_EQ("-custom_option", fuzzer_args[2]);
}
