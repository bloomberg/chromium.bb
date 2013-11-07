// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/file_template.h"
#include "tools/gn/ninja_script_target_writer.h"
#include "tools/gn/test_with_scope.h"

TEST(NinjaScriptTargetWriter, WriteOutputFilesForBuildLine) {
  TestWithScope setup;
  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));
  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));

  target.script_values().outputs().push_back(
      SourceFile("//out/Debug/gen/a b{{source_name_part}}.h"));
  target.script_values().outputs().push_back(
      SourceFile("//out/Debug/gen/{{source_name_part}}.cc"));

  std::ostringstream out;
  NinjaScriptTargetWriter writer(&target, setup.toolchain(), out);

  FileTemplate output_template = writer.GetOutputTemplate();

  SourceFile source("//foo/bar.in");
  std::vector<OutputFile> output_files;
  writer.WriteOutputFilesForBuildLine(output_template, source, &output_files);

  std::string out_str = out.str();
#if defined(OS_WIN)
  std::replace(out_str.begin(), out_str.end(), '\\', '/');
#endif
  EXPECT_EQ(" gen/a$ bbar.h gen/bar.cc", out_str);
}

TEST(NinjaScriptTargetWriter, WriteOutputFilesForBuildLineWithDepfile) {
  TestWithScope setup;
  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));
  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));

  target.script_values().set_depfile(
      SourceFile("//out/Debug/gen/{{source_name_part}}.d"));
  target.script_values().outputs().push_back(
      SourceFile("//out/Debug/gen/{{source_name_part}}.h"));
  target.script_values().outputs().push_back(
      SourceFile("//out/Debug/gen/{{source_name_part}}.cc"));

  std::ostringstream out;
  NinjaScriptTargetWriter writer(&target, setup.toolchain(), out);

  FileTemplate output_template = writer.GetOutputTemplate();

  SourceFile source("//foo/bar.in");
  std::vector<OutputFile> output_files;
  writer.WriteOutputFilesForBuildLine(output_template, source, &output_files);

  std::string out_str = out.str();
#if defined(OS_WIN)
  std::replace(out_str.begin(), out_str.end(), '\\', '/');
#endif
  EXPECT_EQ(" gen/bar.d gen/bar.h gen/bar.cc", out_str);
}

TEST(NinjaScriptTargetWriter, WriteArgsSubstitutions) {
  TestWithScope setup;
  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));
  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));

  std::ostringstream out;
  NinjaScriptTargetWriter writer(&target, setup.toolchain(), out);

  std::vector<std::string> args;
  args.push_back("-i");
  args.push_back("{{source}}");
  args.push_back("--out=foo bar{{source_name_part}}.o");
  FileTemplate args_template(args);

  writer.WriteArgsSubstitutions(SourceFile("//foo/b ar.in"), args_template);

  std::string out_str = out.str();
#if defined(OS_WIN)
  std::replace(out_str.begin(), out_str.end(), '\\', '/');
#endif
  EXPECT_EQ("  source = ../../foo/b$ ar.in\n  source_name_part = b$ ar\n",
            out_str);
}

// Tests the "run script over multiple source files" mode.
TEST(NinjaScriptTargetWriter, InvokeOverSources) {
  TestWithScope setup;
  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));
  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::CUSTOM);

  target.sources().push_back(SourceFile("//foo/input1.txt"));
  target.sources().push_back(SourceFile("//foo/input2.txt"));

  target.script_values().set_script(SourceFile("//foo/script.py"));

  target.script_values().args().push_back("-i");
  target.script_values().args().push_back("{{source}}");
  target.script_values().args().push_back(
      "--out=foo bar{{source_name_part}}.o");

  target.script_values().outputs().push_back(
      SourceFile("//out/Debug/{{source_name_part}}.out"));

  target.source_prereqs().push_back(SourceFile("//foo/included.txt"));

  // Posix.
  {
    setup.settings()->set_target_os(Settings::LINUX);
    setup.build_settings()->set_python_path(base::FilePath(FILE_PATH_LITERAL(
        "/usr/bin/python")));

    std::ostringstream out;
    NinjaScriptTargetWriter writer(&target, setup.toolchain(), out);
    writer.Run();

    const char expected_linux[] =
        "rule __foo_bar___rule\n"
        "  command = /usr/bin/python ../../foo/script.py -i ${source} "
            "\"--out=foo$ bar${source_name_part}.o\"\n"
        "  description = CUSTOM //foo:bar()\n"
        "  restat = 1\n"
        "\n"
        "build input1.out: __foo_bar___rule ../../foo/input1.txt | "
            "../../foo/included.txt\n"
        "  source = ../../foo/input1.txt\n"
        "  source_name_part = input1\n"
        "build input2.out: __foo_bar___rule ../../foo/input2.txt | "
            "../../foo/included.txt\n"
        "  source = ../../foo/input2.txt\n"
        "  source_name_part = input2\n"
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
    // Note: we use forward slashes here so that the output will be the same on
    // Linux and Windows.
    setup.build_settings()->set_python_path(base::FilePath(FILE_PATH_LITERAL(
        "C:/python/python.exe")));
    setup.settings()->set_target_os(Settings::WIN);

    std::ostringstream out;
    NinjaScriptTargetWriter writer(&target, setup.toolchain(), out);
    writer.Run();

    // TODO(brettw) I think we'll need to worry about backslashes here
    // depending if we're on actual Windows or Linux pretending to be Windows.
    const char expected_win[] =
        "rule __foo_bar___rule\n"
        "  command = C:/python/python.exe gyp-win-tool action-wrapper "
            "environment.x86 __foo_bar___rule.$unique_name.rsp\n"
        "  description = CUSTOM //foo:bar()\n"
        "  restat = 1\n"
        "  rspfile = __foo_bar___rule.$unique_name.rsp\n"
        "  rspfile_content = C:/python/python.exe ../../foo/script.py -i "
            "${source} \"--out=foo$ bar${source_name_part}.o\"\n"
        "\n"
        "build input1.out: __foo_bar___rule ../../foo/input1.txt | "
            "../../foo/included.txt\n"
        "  unique_name = 0\n"
        "  source = ../../foo/input1.txt\n"
        "  source_name_part = input1\n"
        "build input2.out: __foo_bar___rule ../../foo/input2.txt | "
            "../../foo/included.txt\n"
        "  unique_name = 1\n"
        "  source = ../../foo/input2.txt\n"
        "  source_name_part = input2\n"
        "\n"
        "build obj/foo/bar.stamp: stamp input1.out input2.out\n";
    std::string out_str = out.str();
#if defined(OS_WIN)
    std::replace(out_str.begin(), out_str.end(), '\\', '/');
#endif
    EXPECT_EQ(expected_win, out_str);
  }
}

// Tests the "run script over multiple source files" mode, with a depfile.
TEST(NinjaScriptTargetWriter, InvokeOverSourcesWithDepfile) {
  TestWithScope setup;
  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));
  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::CUSTOM);

  target.sources().push_back(SourceFile("//foo/input1.txt"));
  target.sources().push_back(SourceFile("//foo/input2.txt"));

  target.script_values().set_script(SourceFile("//foo/script.py"));
  target.script_values().set_depfile(
      SourceFile("//out/Debug/gen/{{source_name_part}}.d"));

  target.script_values().args().push_back("-i");
  target.script_values().args().push_back("{{source}}");
  target.script_values().args().push_back(
      "--out=foo bar{{source_name_part}}.o");

  target.script_values().outputs().push_back(
      SourceFile("//out/Debug/{{source_name_part}}.out"));

  target.source_prereqs().push_back(SourceFile("//foo/included.txt"));

  // Posix.
  {
    setup.settings()->set_target_os(Settings::LINUX);
    setup.build_settings()->set_python_path(base::FilePath(FILE_PATH_LITERAL(
        "/usr/bin/python")));

    std::ostringstream out;
    NinjaScriptTargetWriter writer(&target, setup.toolchain(), out);
    writer.Run();

    const char expected_linux[] =
        "rule __foo_bar___rule\n"
        "  command = /usr/bin/python ../../foo/script.py -i ${source} "
            "\"--out=foo$ bar${source_name_part}.o\"\n"
        "  description = CUSTOM //foo:bar()\n"
        "  restat = 1\n"
        "\n"
        "build gen/input1.d input1.out: __foo_bar___rule ../../foo/input1.txt"
            " | ../../foo/included.txt\n"
        "  source = ../../foo/input1.txt\n"
        "  source_name_part = input1\n"
        "  depfile = gen/input1.d\n"
        "build gen/input2.d input2.out: __foo_bar___rule ../../foo/input2.txt"
            " | ../../foo/included.txt\n"
        "  source = ../../foo/input2.txt\n"
        "  source_name_part = input2\n"
        "  depfile = gen/input2.d\n"
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
    // Note: we use forward slashes here so that the output will be the same on
    // Linux and Windows.
    setup.build_settings()->set_python_path(base::FilePath(FILE_PATH_LITERAL(
        "C:/python/python.exe")));
    setup.settings()->set_target_os(Settings::WIN);

    std::ostringstream out;
    NinjaScriptTargetWriter writer(&target, setup.toolchain(), out);
    writer.Run();

    // TODO(brettw) I think we'll need to worry about backslashes here
    // depending if we're on actual Windows or Linux pretending to be Windows.
    const char expected_win[] =
        "rule __foo_bar___rule\n"
        "  command = C:/python/python.exe gyp-win-tool action-wrapper "
            "environment.x86 __foo_bar___rule.$unique_name.rsp\n"
        "  description = CUSTOM //foo:bar()\n"
        "  restat = 1\n"
        "  rspfile = __foo_bar___rule.$unique_name.rsp\n"
        "  rspfile_content = C:/python/python.exe ../../foo/script.py -i "
            "${source} \"--out=foo$ bar${source_name_part}.o\"\n"
        "\n"
        "build gen/input1.d input1.out: __foo_bar___rule ../../foo/input1.txt"
            " | ../../foo/included.txt\n"
        "  unique_name = 0\n"
        "  source = ../../foo/input1.txt\n"
        "  source_name_part = input1\n"
        "  depfile = gen/input1.d\n"
        "build gen/input2.d input2.out: __foo_bar___rule ../../foo/input2.txt"
            " | ../../foo/included.txt\n"
        "  unique_name = 1\n"
        "  source = ../../foo/input2.txt\n"
        "  source_name_part = input2\n"
        "  depfile = gen/input2.d\n"
        "\n"
        "build obj/foo/bar.stamp: stamp input1.out input2.out\n";
    std::string out_str = out.str();
#if defined(OS_WIN)
    std::replace(out_str.begin(), out_str.end(), '\\', '/');
#endif
    EXPECT_EQ(expected_win, out_str);
  }
}
