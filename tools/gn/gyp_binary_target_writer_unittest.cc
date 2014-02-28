// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/builder_record.h"
#include "tools/gn/gyp_binary_target_writer.h"
#include "tools/gn/test_with_scope.h"

TEST(GypBinaryTargetWriter, ProductExtension) {
  TestWithScope setup;
  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));

  // A shared library w/ the product_extension set to a custom value.
  scoped_ptr<Target> target(
      new Target(setup.settings(), Label(SourceDir("//foo/"), "bar")));
  target->set_output_type(Target::SHARED_LIBRARY);
  target->set_output_extension(std::string("so.6"));
  target->sources().push_back(SourceFile("//foo/input1.cc"));
  target->sources().push_back(SourceFile("//foo/input2.cc"));

  BuilderRecord record(BuilderRecord::ITEM_TARGET, target->label());
  record.set_item(target.PassAs<Item>());
  GypTargetWriter::TargetGroup group;
  group.debug = &record;
  group.release = &record;

  setup.settings()->set_target_os(Settings::LINUX);

  std::ostringstream out;
  GypBinaryTargetWriter writer(group, setup.toolchain(),
                               SourceDir("//out/gn_gyp/"), out);
  writer.Run();

  const char expected[] =
      "    {\n"
      "      'target_name': 'bar',\n"
      "      'product_name': 'bar',\n"
      "      'product_extension': 'so.6',\n"
      "      'type': 'shared_library',\n"
      "      'target_conditions': [\n"
      "        ['_toolset == \"target\"', {\n"
      "          'configurations': {\n"
      "            'Debug': {\n"
      "              'ldflags': [ ],\n"
      "            },\n"
      "            'Release': {\n"
      "              'ldflags': [ ],\n"
      "            },\n"
      "          },\n"
      "          'sources': [\n"
      "            '<(DEPTH)/foo/input1.cc',\n"
      "            '<(DEPTH)/foo/input2.cc',\n"
      "          ],\n"
      "        },],\n"
      "      ],\n"
      "    },\n";
  std::string out_str = out.str();
  EXPECT_EQ(expected, out_str);
}

TEST(GypBinaryTargetWriter, EmptyProductExtension) {
  TestWithScope setup;
  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));

  // This test is the same as ProductExtension, except that
  // we call set_output_extension("") and ensure that we still get the default.
  scoped_ptr<Target> target(
      new Target(setup.settings(), Label(SourceDir("//foo/"), "bar")));
  target->set_output_type(Target::SHARED_LIBRARY);

  target->sources().push_back(SourceFile("//foo/input1.cc"));
  target->sources().push_back(SourceFile("//foo/input2.cc"));
  target->set_output_extension(std::string());

  BuilderRecord record(BuilderRecord::ITEM_TARGET, target->label());
  record.set_item(target.PassAs<Item>());
  GypTargetWriter::TargetGroup group;
  group.debug = &record;
  group.release = &record;

  setup.settings()->set_target_os(Settings::LINUX);

  std::ostringstream out;
  GypBinaryTargetWriter writer(group, setup.toolchain(),
                               SourceDir("//out/gn_gyp/"), out);
  writer.Run();

  const char expected[] =
      "    {\n"
      "      'target_name': 'bar',\n"
      "      'product_name': 'bar',\n"
      "      'type': 'shared_library',\n"
      "      'target_conditions': [\n"
      "        ['_toolset == \"target\"', {\n"
      "          'configurations': {\n"
      "            'Debug': {\n"
      "              'ldflags': [ ],\n"
      "            },\n"
      "            'Release': {\n"
      "              'ldflags': [ ],\n"
      "            },\n"
      "          },\n"
      "          'sources': [\n"
      "            '<(DEPTH)/foo/input1.cc',\n"
      "            '<(DEPTH)/foo/input2.cc',\n"
      "          ],\n"
      "        },],\n"
      "      ],\n"
      "    },\n";
  std::string out_str = out.str();
  EXPECT_EQ(expected, out_str);
}
