// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/builder_record.h"
#include "tools/gn/gyp_script_target_writer.h"
#include "tools/gn/test_with_scope.h"

TEST(GypScriptTargetWriter, Run) {
  TestWithScope setup;
  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));
  scoped_ptr<Target> target(
      new Target(setup.settings(), Label(SourceDir("//foo/"), "bar")));
  target->set_output_type(Target::CUSTOM);

  target->sources().push_back(SourceFile("//foo/input1.txt"));
  target->sources().push_back(SourceFile("//foo/input2.txt"));

  target->script_values().outputs().push_back(
      SourceFile("//out/Debug/{{source_file_part}}.out"));

  BuilderRecord record(BuilderRecord::ITEM_TARGET, target->label());
  record.set_item(target.PassAs<Item>());
  GypTargetWriter::TargetGroup group;
  group.debug = &record;

  setup.settings()->set_target_os(Settings::WIN);

  std::ostringstream out;
  GypScriptTargetWriter writer(group, setup.toolchain(),
                               SourceDir("//out/gn_gyp/"), out);
  writer.Run();

  const char expected[] =
      "    {\n"
      "      'target_name': 'bar',\n"
      "      'type': 'none',\n"
      "      'actions': [{\n"
      "        'action_name': 'bar action',\n"
      "        'action': [\n"
      "          'ninja',\n"
      "          '-C', '../../out/Debug/obj/foo/bar_ninja',\n"
      "          'bar',\n"
      "        ],\n"
      "        'inputs': [\n"
      "          '../../foo/input1.txt',\n"
      "          '../../foo/input2.txt',\n"
      "        ],\n"
      "        'outputs': [\n"
      "          '../../out/Debug/input1.txt.out',\n"
      "          '../../out/Debug/input2.txt.out',\n"
      "        ],\n"
      "      }],\n"
      "    },\n";
  std::string out_str = out.str();
  EXPECT_EQ(expected, out_str);
}
