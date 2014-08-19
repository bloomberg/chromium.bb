// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/ninja_copy_target_writer.h"
#include "tools/gn/target.h"
#include "tools/gn/test_with_scope.h"

// Tests mutliple files with an output pattern and no toolchain dependency.
TEST(NinjaCopyTargetWriter, Run) {
  TestWithScope setup;

  setup.settings()->set_target_os(Settings::LINUX);
  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));
  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::COPY_FILES);

  target.sources().push_back(SourceFile("//foo/input1.txt"));
  target.sources().push_back(SourceFile("//foo/input2.txt"));

  target.action_values().outputs() =
      SubstitutionList::MakeForTest("//out/Debug/{{source_name_part}}.out");

  target.SetToolchain(setup.toolchain());
  target.OnResolved();

  std::ostringstream out;
  NinjaCopyTargetWriter writer(&target, out);
  writer.Run();

  const char expected_linux[] =
      "build input1.out: copy ../../foo/input1.txt\n"
      "build input2.out: copy ../../foo/input2.txt\n"
      "\n"
      "build obj/foo/bar.stamp: stamp input1.out input2.out\n";
  std::string out_str = out.str();
  EXPECT_EQ(expected_linux, out_str);
}

// Tests a single file with no output pattern.
TEST(NinjaCopyTargetWriter, ToolchainDeps) {
  TestWithScope setup;

  setup.settings()->set_target_os(Settings::LINUX);
  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));
  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::COPY_FILES);

  target.sources().push_back(SourceFile("//foo/input1.txt"));

  target.action_values().outputs() =
      SubstitutionList::MakeForTest("//out/Debug/output.out");

  target.SetToolchain(setup.toolchain());
  target.OnResolved();

  std::ostringstream out;
  NinjaCopyTargetWriter writer(&target, out);
  writer.Run();

  const char expected_linux[] =
      "build output.out: copy ../../foo/input1.txt\n"
      "\n"
      "build obj/foo/bar.stamp: stamp output.out\n";
  std::string out_str = out.str();
  EXPECT_EQ(expected_linux, out_str);
}
