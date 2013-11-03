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

// Prevent horrible wrapping of calls below.
#define GPV(val) provider.GetProgrammaticValue(val)->string_value()

  // Test the default toolchain.
  {
    Toolchain toolchain(Label(SourceDir("//toolchain/"), "tc"));
    Settings settings(&build_settings, &toolchain, std::string());

    Scope scope(&settings);
    scope.set_source_dir(SourceDir("//source/"));
    ScopePerFileProvider provider(&scope);

    EXPECT_EQ("//toolchain:tc",         GPV(variables::kCurrentToolchain));
    EXPECT_EQ("//toolchain:default",    GPV(variables::kDefaultToolchain));
    EXPECT_EQ("//out/Debug",            GPV(variables::kRootBuildDir));
    EXPECT_EQ("//out/Debug/gen",        GPV(variables::kRootGenDir));
    EXPECT_EQ("//out/Debug",            GPV(variables::kRootOutDir));
    EXPECT_EQ("//out/Debug/gen/source", GPV(variables::kTargetGenDir));
    EXPECT_EQ("//out/Debug/obj/source", GPV(variables::kTargetOutDir));
  }

  // Test some with an alternate toolchain.
  {
    Toolchain toolchain(Label(SourceDir("//toolchain/"), "tc"));
    Settings settings(&build_settings, &toolchain, "tc");

    Scope scope(&settings);
    scope.set_source_dir(SourceDir("//source/"));
    ScopePerFileProvider provider(&scope);

    EXPECT_EQ("//out/Debug",               GPV(variables::kRootBuildDir));
    EXPECT_EQ("//out/Debug/tc/gen",        GPV(variables::kRootGenDir));
    EXPECT_EQ("//out/Debug/tc",            GPV(variables::kRootOutDir));
    EXPECT_EQ("//out/Debug/tc/gen/source", GPV(variables::kTargetGenDir));
    EXPECT_EQ("//out/Debug/tc/obj/source", GPV(variables::kTargetOutDir));
  }
}
