// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "tools/gn/build_settings.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/ninja_helper.h"
#include "tools/gn/settings.h"
#include "tools/gn/target.h"
#include "tools/gn/toolchain.h"

namespace {

class HelperSetterUpper {
 public:
  HelperSetterUpper()
      : build_settings(),
        settings(&build_settings, std::string()),
        toolchain(&settings, Label(SourceDir("//"), "tc")),
        target(&settings, Label(SourceDir("//tools/gn/"), "name")) {
    settings.set_toolchain_label(toolchain.label());
    settings.set_target_os(Settings::WIN);

    // Output going to "out/Debug".
    build_settings.SetBuildDir(SourceDir("//out/Debug/"));

    // Our source target is in "tools/gn".
    target.set_output_type(Target::EXECUTABLE);
  }

  BuildSettings build_settings;
  Settings settings;
  Toolchain toolchain;
  Target target;
};

}  // namespace

TEST(NinjaHelper, GetNinjaFileForTarget) {
  HelperSetterUpper setup;
  NinjaHelper helper(&setup.build_settings);

  // Default toolchain.
  EXPECT_EQ(OutputFile("obj/tools/gn/name.ninja").value(),
            helper.GetNinjaFileForTarget(&setup.target).value());
}

TEST(NinjaHelper, GetOutputFileForSource) {
  HelperSetterUpper setup;
  NinjaHelper helper(&setup.build_settings);

  // On Windows, expect ".obj"
  EXPECT_EQ(OutputFile("obj/tools/gn/name.foo.obj").value(),
            helper.GetOutputFileForSource(&setup.target,
                                          SourceFile("//tools/gn/foo.cc"),
                                          SOURCE_CC).value());
}

TEST(NinjaHelper, GetOutputFileForObject) {
  HelperSetterUpper setup;
  NinjaHelper helper(&setup.build_settings);

  EXPECT_EQ(OutputFile("../../tools/gn/foo.o").value(),
            helper.GetOutputFileForSource(&setup.target,
                                          SourceFile("//tools/gn/foo.o"),
                                          SOURCE_O).value());

  EXPECT_EQ(OutputFile("../../tools/gn/foo.obj").value(),
            helper.GetOutputFileForSource(&setup.target,
                                          SourceFile("//tools/gn/foo.obj"),
                                          SOURCE_O).value());

  EXPECT_EQ(OutputFile("nested/foo.o").value(),
            helper.GetOutputFileForSource(
                &setup.target,
                SourceFile("//out/Debug/nested/foo.o"),
                SOURCE_O).value());

  EXPECT_EQ(OutputFile("/abs/rooted/foo.o").value(),
            helper.GetOutputFileForSource(&setup.target,
                                          SourceFile("/abs/rooted/foo.o"),
                                          SOURCE_O).value());
}

TEST(NinjaHelper, GetTargetOutputFile) {
  HelperSetterUpper setup;
  NinjaHelper helper(&setup.build_settings);
  EXPECT_EQ(OutputFile("name.exe"),
            helper.GetTargetOutputFile(&setup.target));

  // Static library on Windows goes alongside the object files.
  setup.target.set_output_type(Target::STATIC_LIBRARY);
  EXPECT_EQ(OutputFile("obj/tools/gn/name.lib"),
            helper.GetTargetOutputFile(&setup.target));

  // TODO(brettw) test output to library and other OS types.
}
