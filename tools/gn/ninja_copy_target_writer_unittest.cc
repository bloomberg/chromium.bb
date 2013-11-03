// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/file_template.h"
#include "tools/gn/ninja_copy_target_writer.h"
#include "tools/gn/test_with_scope.h"

TEST(NinjaCopyTargetWriter, Run) {
  TestWithScope setup;
  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));
  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::COPY_FILES);

  target.sources().push_back(SourceFile("//foo/input1.txt"));
  target.sources().push_back(SourceFile("//foo/input2.txt"));

  target.script_values().outputs().push_back(
      SourceFile("//out/Debug/{{source_name_part}}.out"));

  // Posix.
  {
    setup.settings()->set_target_os(Settings::LINUX);

    std::ostringstream out;
    NinjaCopyTargetWriter writer(&target, setup.toolchain(), out);
    writer.Run();

    const char expected_linux[] =
        "build input1.out: copy ../../foo/input1.txt\n"
        "build input2.out: copy ../../foo/input2.txt\n"
        "\n"
        "build obj/foo/bar.stamp: stamp input1.out input2.out\n";
    std::string out_str = out.str();
#if defined(OS_WIN)
    std::replace(out_str.begin(), out_str.end(), '\\', '/');
#endif
    EXPECT_EQ(expected_linux, out_str);
  }

  // Windows.
  {
    setup.settings()->set_target_os(Settings::WIN);

    std::ostringstream out;
    NinjaCopyTargetWriter writer(&target, setup.toolchain(), out);
    writer.Run();

    // TODO(brettw) I think we'll need to worry about backslashes here
    // depending if we're on actual Windows or Linux pretending to be Windows.
    const char expected_win[] =
        "build input1.out: copy ../../foo/input1.txt\n"
        "build input2.out: copy ../../foo/input2.txt\n"
        "\n"
        "build obj/foo/bar.stamp: stamp input1.out input2.out\n";
    std::string out_str = out.str();
#if defined(OS_WIN)
    std::replace(out_str.begin(), out_str.end(), '\\', '/');
#endif
    EXPECT_EQ(expected_win, out_str);
  }
}
