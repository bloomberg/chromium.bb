// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_create_bundle_target_writer.h"

#include <algorithm>
#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/target.h"
#include "tools/gn/test_with_scope.h"

namespace {

void SetupBundleDataDir(BundleData* bundle_data, const std::string& root_dir) {
  bundle_data->root_dir().assign(root_dir + "/bar.bundle");
  bundle_data->resources_dir().assign(bundle_data->root_dir() + "/Resources");
  bundle_data->executable_dir().assign(bundle_data->root_dir() + "/Executable");
  bundle_data->plugins_dir().assign(bundle_data->root_dir() + "/PlugIns");
}

}  // namespace

// Tests multiple files with an output pattern.
TEST(NinjaCreateBundleTargetWriter, Run) {
  TestWithScope setup;
  Err err;

  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));
  Target target(setup.settings(), Label(SourceDir("//baz/"), "bar"));
  target.set_output_type(Target::CREATE_BUNDLE);

  SetupBundleDataDir(&target.bundle_data(), "//out/Debug");

  std::vector<SourceFile> sources;
  sources.push_back(SourceFile("//foo/input1.txt"));
  sources.push_back(SourceFile("//foo/input2.txt"));
  target.bundle_data().file_rules().push_back(BundleFileRule(
      sources, SubstitutionPattern::MakeForTest(
                   "{{bundle_resources_dir}}/{{source_file_part}}")));

  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  std::ostringstream out;
  NinjaCreateBundleTargetWriter writer(&target, out);
  writer.Run();

  const char expected[] =
      "build bar.bundle/Resources/input1.txt: copy_bundle_data "
          "../../foo/input1.txt\n"
      "build bar.bundle/Resources/input2.txt: copy_bundle_data "
          "../../foo/input2.txt\n"
      "\n"
      "build obj/baz/bar.stamp: stamp "
          "bar.bundle/Resources/input1.txt "
          "bar.bundle/Resources/input2.txt\n"
      "build bar.bundle: phony obj/baz/bar.stamp\n";
  std::string out_str = out.str();
  EXPECT_EQ(expected, out_str);
}

// Tests multiple files from asset catalog.
TEST(NinjaCreateBundleTargetWriter, AssetCatalog) {
  TestWithScope setup;
  Err err;

  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));
  Target target(setup.settings(), Label(SourceDir("//baz/"), "bar"));
  target.set_output_type(Target::CREATE_BUNDLE);

  SetupBundleDataDir(&target.bundle_data(), "//out/Debug");

  std::vector<SourceFile>& asset_catalog_sources =
      target.bundle_data().asset_catalog_sources();
  asset_catalog_sources.push_back(
      SourceFile("//foo/Foo.xcassets/foo.imageset/Contents.json"));
  asset_catalog_sources.push_back(
      SourceFile("//foo/Foo.xcassets/foo.imageset/FooIcon-29.png"));
  asset_catalog_sources.push_back(
      SourceFile("//foo/Foo.xcassets/foo.imageset/FooIcon-29@2x.png"));
  asset_catalog_sources.push_back(
      SourceFile("//foo/Foo.xcassets/foo.imageset/FooIcon-29@3x.png"));

  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  std::ostringstream out;
  NinjaCreateBundleTargetWriter writer(&target, out);
  writer.Run();

  const char expected[] =
      "build bar.bundle/Resources/Assets.car: compile_xcassets "
          "../../foo/Foo.xcassets | "
          "../../foo/Foo.xcassets/foo.imageset/Contents.json "
          "../../foo/Foo.xcassets/foo.imageset/FooIcon-29.png "
          "../../foo/Foo.xcassets/foo.imageset/FooIcon-29@2x.png "
          "../../foo/Foo.xcassets/foo.imageset/FooIcon-29@3x.png\n"
      "\n"
      "build obj/baz/bar.stamp: stamp bar.bundle/Resources/Assets.car\n"
      "build bar.bundle: phony obj/baz/bar.stamp\n";
  std::string out_str = out.str();
  EXPECT_EQ(expected, out_str);
}

// Tests that the phony target for the top-level bundle directory is generated
// correctly.
TEST(NinjaCreateBundleTargetWriter, BundleRootDirOutput) {
  TestWithScope setup;
  Err err;

  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));
  Target target(setup.settings(), Label(SourceDir("//baz/"), "bar"));
  target.set_output_type(Target::CREATE_BUNDLE);

  const std::string bundle_root_dir("//out/Debug/bar.bundle/Contents");
  target.bundle_data().root_dir().assign(bundle_root_dir);
  target.bundle_data().resources_dir().assign(bundle_root_dir + "/Resources");
  target.bundle_data().executable_dir().assign(bundle_root_dir + "/MacOS");
  target.bundle_data().plugins_dir().assign(bundle_root_dir + "/Plug Ins");

  std::vector<SourceFile> sources;
  sources.push_back(SourceFile("//foo/input1.txt"));
  sources.push_back(SourceFile("//foo/input2.txt"));
  target.bundle_data().file_rules().push_back(BundleFileRule(
      sources, SubstitutionPattern::MakeForTest(
                   "{{bundle_resources_dir}}/{{source_file_part}}")));

  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  std::ostringstream out;
  NinjaCreateBundleTargetWriter writer(&target, out);
  writer.Run();

  const char expected[] =
      "build bar.bundle/Contents/Resources/input1.txt: copy_bundle_data "
          "../../foo/input1.txt\n"
      "build bar.bundle/Contents/Resources/input2.txt: copy_bundle_data "
          "../../foo/input2.txt\n"
      "\n"
      "build obj/baz/bar.stamp: stamp "
          "bar.bundle/Contents/Resources/input1.txt "
          "bar.bundle/Contents/Resources/input2.txt\n"
      "build bar.bundle: phony obj/baz/bar.stamp\n";
  std::string out_str = out.str();
  EXPECT_EQ(expected, out_str);
}

// Tests complex target with multiple bundle_data sources, including
// some asset catalog.
TEST(NinjaCreateBundleTargetWriter, OrderOnlyDeps) {
  TestWithScope setup;
  Err err;

  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));
  Target target(setup.settings(), Label(SourceDir("//baz/"), "bar"));
  target.set_output_type(Target::CREATE_BUNDLE);

  SetupBundleDataDir(&target.bundle_data(), "//out/Debug");

  std::vector<SourceFile> sources1;
  sources1.push_back(SourceFile("//foo/input1.txt"));
  sources1.push_back(SourceFile("//foo/input2.txt"));
  target.bundle_data().file_rules().push_back(BundleFileRule(
      sources1, SubstitutionPattern::MakeForTest(
                    "{{bundle_resources_dir}}/{{source_file_part}}")));

  std::vector<SourceFile> sources2;
  sources2.push_back(SourceFile("//qux/Info.plist"));
  target.bundle_data().file_rules().push_back(BundleFileRule(
      sources2,
      SubstitutionPattern::MakeForTest("{{bundle_root_dir}}/Info.plist")));

  std::vector<SourceFile> empty_source;
  target.bundle_data().file_rules().push_back(BundleFileRule(
      empty_source, SubstitutionPattern::MakeForTest(
                        "{{bundle_plugins_dir}}/{{source_file_part}}")));

  std::vector<SourceFile>& asset_catalog_sources =
      target.bundle_data().asset_catalog_sources();
  asset_catalog_sources.push_back(
      SourceFile("//foo/Foo.xcassets/foo.imageset/Contents.json"));
  asset_catalog_sources.push_back(
      SourceFile("//foo/Foo.xcassets/foo.imageset/FooIcon-29.png"));
  asset_catalog_sources.push_back(
      SourceFile("//foo/Foo.xcassets/foo.imageset/FooIcon-29@2x.png"));
  asset_catalog_sources.push_back(
      SourceFile("//foo/Foo.xcassets/foo.imageset/FooIcon-29@3x.png"));

  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  std::ostringstream out;
  NinjaCreateBundleTargetWriter writer(&target, out);
  writer.Run();

  const char expected[] =
      "build bar.bundle/Resources/input1.txt: copy_bundle_data "
          "../../foo/input1.txt\n"
      "build bar.bundle/Resources/input2.txt: copy_bundle_data "
          "../../foo/input2.txt\n"
      "build bar.bundle/Info.plist: copy_bundle_data ../../qux/Info.plist\n"
      "build bar.bundle/Resources/Assets.car: compile_xcassets "
          "../../foo/Foo.xcassets | "
          "../../foo/Foo.xcassets/foo.imageset/Contents.json "
          "../../foo/Foo.xcassets/foo.imageset/FooIcon-29.png "
          "../../foo/Foo.xcassets/foo.imageset/FooIcon-29@2x.png "
          "../../foo/Foo.xcassets/foo.imageset/FooIcon-29@3x.png\n"
      "\n"
      "build obj/baz/bar.stamp: stamp "
          "bar.bundle/Resources/input1.txt "
          "bar.bundle/Resources/input2.txt "
          "bar.bundle/Info.plist "
          "bar.bundle/Resources/Assets.car\n"
      "build bar.bundle: phony obj/baz/bar.stamp\n";
  std::string out_str = out.str();
  EXPECT_EQ(expected, out_str);
}
