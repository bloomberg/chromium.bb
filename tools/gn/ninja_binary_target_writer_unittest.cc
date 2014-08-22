// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/ninja_binary_target_writer.h"
#include "tools/gn/target.h"
#include "tools/gn/test_with_scope.h"

TEST(NinjaBinaryTargetWriter, SourceSet) {
  TestWithScope setup;
  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));
  setup.settings()->set_target_os(Settings::WIN);

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::SOURCE_SET);
  target.sources().push_back(SourceFile("//foo/input1.cc"));
  target.sources().push_back(SourceFile("//foo/input2.cc"));
  // Also test object files, which should be just passed through to the
  // dependents to link.
  target.sources().push_back(SourceFile("//foo/input3.o"));
  target.sources().push_back(SourceFile("//foo/input4.obj"));
  target.SetToolchain(setup.toolchain());
  target.OnResolved();

  // Source set itself.
  {
    std::ostringstream out;
    NinjaBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "defines =\n"
        "include_dirs =\n"
        "cflags =\n"
        "cflags_c =\n"
        "cflags_cc =\n"
        "cflags_objc =\n"
        "cflags_objcc =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build obj/foo/bar.input1.o: cxx ../../foo/input1.cc\n"
        "build obj/foo/bar.input2.o: cxx ../../foo/input2.cc\n"
        "\n"
        "build obj/foo/bar.stamp: stamp obj/foo/bar.input1.o "
            "obj/foo/bar.input2.o ../../foo/input3.o ../../foo/input4.obj\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str);
  }

  // A shared library that depends on the source set.
  Target shlib_target(setup.settings(), Label(SourceDir("//foo/"), "shlib"));
  shlib_target.set_output_type(Target::SHARED_LIBRARY);
  shlib_target.deps().push_back(LabelTargetPair(&target));
  shlib_target.SetToolchain(setup.toolchain());
  shlib_target.OnResolved();

  {
    std::ostringstream out;
    NinjaBinaryTargetWriter writer(&shlib_target, out);
    writer.Run();

    const char expected[] =
        "defines =\n"
        "include_dirs =\n"
        "cflags =\n"
        "cflags_c =\n"
        "cflags_cc =\n"
        "cflags_objc =\n"
        "cflags_objcc =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = libshlib\n"
        "\n"
        "\n"
        // Ordering of the obj files here should come out in the order
        // specified, with the target's first, followed by the source set's, in
        // order.
        "build ./libshlib.so: solink obj/foo/bar.input1.o "
            "obj/foo/bar.input2.o ../../foo/input3.o ../../foo/input4.obj\n"
        "  ldflags =\n"
        "  libs =\n"
        "  output_extension = .so\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str);
  }

  // A static library that depends on the source set (should not link it).
  Target stlib_target(setup.settings(), Label(SourceDir("//foo/"), "stlib"));
  stlib_target.set_output_type(Target::STATIC_LIBRARY);
  stlib_target.deps().push_back(LabelTargetPair(&target));
  stlib_target.SetToolchain(setup.toolchain());
  stlib_target.OnResolved();

  {
    std::ostringstream out;
    NinjaBinaryTargetWriter writer(&stlib_target, out);
    writer.Run();

    const char expected[] =
        "defines =\n"
        "include_dirs =\n"
        "cflags =\n"
        "cflags_c =\n"
        "cflags_cc =\n"
        "cflags_objc =\n"
        "cflags_objcc =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = libstlib\n"
        "\n"
        "\n"
        // There are no sources so there are no params to alink. (In practice
        // this will probably fail in the archive tool.)
        "build obj/foo/libstlib.a: alink\n"
        "  ldflags =\n"
        "  libs =\n"
        "  output_extension = \n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str);
  }
}

TEST(NinjaBinaryTargetWriter, ProductExtension) {
  TestWithScope setup;
  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));
  setup.settings()->set_target_os(Settings::LINUX);

  // A shared library w/ the product_extension set to a custom value.
  Target target(setup.settings(), Label(SourceDir("//foo/"), "shlib"));
  target.set_output_type(Target::SHARED_LIBRARY);
  target.set_output_extension(std::string("so.6"));
  target.sources().push_back(SourceFile("//foo/input1.cc"));
  target.sources().push_back(SourceFile("//foo/input2.cc"));
  target.SetToolchain(setup.toolchain());
  target.OnResolved();

  std::ostringstream out;
  NinjaBinaryTargetWriter writer(&target, out);
  writer.Run();

  const char expected[] =
      "defines =\n"
      "include_dirs =\n"
      "cflags =\n"
      "cflags_c =\n"
      "cflags_cc =\n"
      "cflags_objc =\n"
      "cflags_objcc =\n"
      "root_out_dir = .\n"
      "target_out_dir = obj/foo\n"
      "target_output_name = libshlib\n"
      "\n"
      "build obj/foo/libshlib.input1.o: cxx ../../foo/input1.cc\n"
      "build obj/foo/libshlib.input2.o: cxx ../../foo/input2.cc\n"
      "\n"
      "build ./libshlib.so.6: solink obj/foo/libshlib.input1.o "
          "obj/foo/libshlib.input2.o\n"
      "  ldflags =\n"
      "  libs =\n"
      "  output_extension = .so.6\n";

  std::string out_str = out.str();
  EXPECT_EQ(expected, out_str);
}

TEST(NinjaBinaryTargetWriter, EmptyProductExtension) {
  TestWithScope setup;
  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));
  setup.settings()->set_target_os(Settings::LINUX);

  // This test is the same as ProductExtension, except that
  // we call set_output_extension("") and ensure that we still get the default.
  Target target(setup.settings(), Label(SourceDir("//foo/"), "shlib"));
  target.set_output_type(Target::SHARED_LIBRARY);
  target.set_output_extension(std::string());
  target.sources().push_back(SourceFile("//foo/input1.cc"));
  target.sources().push_back(SourceFile("//foo/input2.cc"));

  target.SetToolchain(setup.toolchain());
  target.OnResolved();

  std::ostringstream out;
  NinjaBinaryTargetWriter writer(&target, out);
  writer.Run();

  const char expected[] =
      "defines =\n"
      "include_dirs =\n"
      "cflags =\n"
      "cflags_c =\n"
      "cflags_cc =\n"
      "cflags_objc =\n"
      "cflags_objcc =\n"
      "root_out_dir = .\n"
      "target_out_dir = obj/foo\n"
      "target_output_name = libshlib\n"
      "\n"
      "build obj/foo/libshlib.input1.o: cxx ../../foo/input1.cc\n"
      "build obj/foo/libshlib.input2.o: cxx ../../foo/input2.cc\n"
      "\n"
      "build ./libshlib.so: solink obj/foo/libshlib.input1.o "
          "obj/foo/libshlib.input2.o\n"
      "  ldflags =\n"
      "  libs =\n"
      "  output_extension = .so\n";

  std::string out_str = out.str();
  EXPECT_EQ(expected, out_str);
}
