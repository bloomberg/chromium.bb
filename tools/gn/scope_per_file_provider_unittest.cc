// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/scope_per_file_provider.h"
#include "tools/gn/settings.h"
#include "tools/gn/toolchain.h"
#include "tools/gn/variables.h"

TEST(ScopePerFileProvider, Expected) {
  Err err;

  BuildSettings build_settings;
  build_settings.toolchain_manager().SetDefaultToolchainUnlocked(
      Label(SourceDir("//toolchain/"), "default", SourceDir(), ""),
      LocationRange(), &err);
  EXPECT_FALSE(err.has_error());

  build_settings.SetBuildDir(SourceDir("//out/Debug/"));

  Toolchain toolchain(Label(SourceDir("//toolchain/"), "tc", SourceDir(), ""));
  Settings settings(&build_settings, &toolchain, std::string());
  Scope scope(&settings);
  scope.set_source_dir(SourceDir("//source/"));

  ScopePerFileProvider provider(&scope);

  EXPECT_EQ("//toolchain:tc", provider.GetProgrammaticValue(
      variables::kCurrentToolchain)->string_value());
  EXPECT_EQ("//toolchain:default", provider.GetProgrammaticValue(
      variables::kDefaultToolchain)->string_value());
  EXPECT_EQ("../..",provider.GetProgrammaticValue(
      variables::kRelativeBuildToSourceRootDir)->string_value());
  EXPECT_EQ("../out/Debug", provider.GetProgrammaticValue(
      variables::kRelativeRootOutputDir)->string_value());
  EXPECT_EQ("../out/Debug/gen", provider.GetProgrammaticValue(
      variables::kRelativeRootGenDir)->string_value());
  EXPECT_EQ("..", provider.GetProgrammaticValue(
      variables::kRelativeSourceRootDir)->string_value());
  EXPECT_EQ("../out/Debug/obj/source", provider.GetProgrammaticValue(
      variables::kRelativeTargetOutputDir)->string_value());
  EXPECT_EQ("../out/Debug/gen/source", provider.GetProgrammaticValue(
      variables::kRelativeTargetGenDir)->string_value());
  EXPECT_EQ("//out/Debug/gen",provider.GetProgrammaticValue(
      variables::kRootGenDir)->string_value());
  EXPECT_EQ("//out/Debug/gen/source",provider.GetProgrammaticValue(
      variables::kTargetGenDir)->string_value());
}
