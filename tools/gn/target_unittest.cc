// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/config.h"
#include "tools/gn/settings.h"
#include "tools/gn/target.h"
#include "tools/gn/toolchain.h"

namespace {

class TargetTest : public testing::Test {
 public:
  TargetTest()
      : build_settings_(),
        settings_(&build_settings_, std::string()),
        toolchain_(&settings_, Label(SourceDir("//tc/"), "tc")) {
    settings_.set_toolchain_label(toolchain_.label());
  }
  virtual ~TargetTest() {
  }

 protected:
  BuildSettings build_settings_;
  Settings settings_;
  Toolchain toolchain_;
};

}  // namespace

// Tests that depending on a group is like depending directly on the group's
// deps.
TEST_F(TargetTest, GroupDeps) {
  // Two low-level targets.
  Target x(&settings_, Label(SourceDir("//component/"), "x"));
  Target y(&settings_, Label(SourceDir("//component/"), "y"));

  // Make a group for both x and y.
  Target g(&settings_, Label(SourceDir("//group/"), "g"));
  g.set_output_type(Target::GROUP);
  g.deps().push_back(LabelTargetPair(&x));
  g.deps().push_back(LabelTargetPair(&y));

  // Random placeholder target so we can see the group's deps get inserted at
  // the right place.
  Target b(&settings_, Label(SourceDir("//app/"), "b"));

  // Make a target depending on the group and "b". OnResolved will expand.
  Target a(&settings_, Label(SourceDir("//app/"), "a"));
  a.set_output_type(Target::EXECUTABLE);
  a.deps().push_back(LabelTargetPair(&g));
  a.deps().push_back(LabelTargetPair(&b));
  a.OnResolved();

  // The group's deps should be inserted after the group itself in the deps
  // list, so we should get "g, x, y, b"
  ASSERT_EQ(4u, a.deps().size());
  EXPECT_EQ(&g, a.deps()[0].ptr);
  EXPECT_EQ(&x, a.deps()[1].ptr);
  EXPECT_EQ(&y, a.deps()[2].ptr);
  EXPECT_EQ(&b, a.deps()[3].ptr);
}

// Tests that lib[_dir]s are inherited across deps boundaries for static
// libraries but not executables.
TEST_F(TargetTest, LibInheritance) {
  const std::string lib("foo");
  const SourceDir libdir("/foo_dir/");

  // Leaf target with ldflags set.
  Target z(&settings_, Label(SourceDir("//foo/"), "z"));
  z.set_output_type(Target::STATIC_LIBRARY);
  z.config_values().libs().push_back(lib);
  z.config_values().lib_dirs().push_back(libdir);
  z.OnResolved();

  // All lib[_dir]s should be set when target is resolved.
  ASSERT_EQ(1u, z.all_libs().size());
  EXPECT_EQ(lib, z.all_libs()[0]);
  ASSERT_EQ(1u, z.all_lib_dirs().size());
  EXPECT_EQ(libdir, z.all_lib_dirs()[0]);

  // Shared library target should inherit the libs from the static library
  // and its own. Its own flag should be before the inherited one.
  const std::string second_lib("bar");
  const SourceDir second_libdir("/bar_dir/");
  Target shared(&settings_, Label(SourceDir("//foo/"), "shared"));
  shared.set_output_type(Target::SHARED_LIBRARY);
  shared.config_values().libs().push_back(second_lib);
  shared.config_values().lib_dirs().push_back(second_libdir);
  shared.deps().push_back(LabelTargetPair(&z));
  shared.OnResolved();

  ASSERT_EQ(2u, shared.all_libs().size());
  EXPECT_EQ(second_lib, shared.all_libs()[0]);
  EXPECT_EQ(lib, shared.all_libs()[1]);
  ASSERT_EQ(2u, shared.all_lib_dirs().size());
  EXPECT_EQ(second_libdir, shared.all_lib_dirs()[0]);
  EXPECT_EQ(libdir, shared.all_lib_dirs()[1]);

  // Executable target shouldn't get either by depending on shared.
  Target exec(&settings_, Label(SourceDir("//foo/"), "exec"));
  exec.set_output_type(Target::EXECUTABLE);
  exec.deps().push_back(LabelTargetPair(&shared));
  exec.OnResolved();
  EXPECT_EQ(0u, exec.all_libs().size());
  EXPECT_EQ(0u, exec.all_lib_dirs().size());
}

// Test all/direct_dependent_configs inheritance, and
// forward_dependent_configs_from
TEST_F(TargetTest, DependentConfigs) {
  // Set up a dependency chain of a -> b -> c
  Target a(&settings_, Label(SourceDir("//foo/"), "a"));
  a.set_output_type(Target::EXECUTABLE);
  Target b(&settings_, Label(SourceDir("//foo/"), "b"));
  b.set_output_type(Target::STATIC_LIBRARY);
  Target c(&settings_, Label(SourceDir("//foo/"), "c"));
  c.set_output_type(Target::STATIC_LIBRARY);
  a.deps().push_back(LabelTargetPair(&b));
  b.deps().push_back(LabelTargetPair(&c));

  // Normal non-inherited config.
  Config config(&settings_, Label(SourceDir("//foo/"), "config"));
  c.configs().push_back(LabelConfigPair(&config));

  // All dependent config.
  Config all(&settings_, Label(SourceDir("//foo/"), "all"));
  c.all_dependent_configs().push_back(LabelConfigPair(&all));

  // Direct dependent config.
  Config direct(&settings_, Label(SourceDir("//foo/"), "direct"));
  c.direct_dependent_configs().push_back(LabelConfigPair(&direct));

  c.OnResolved();
  b.OnResolved();
  a.OnResolved();

  // B should have gotten both dependent configs from C.
  ASSERT_EQ(2u, b.configs().size());
  EXPECT_EQ(&all, b.configs()[0].ptr);
  EXPECT_EQ(&direct, b.configs()[1].ptr);
  ASSERT_EQ(1u, b.all_dependent_configs().size());
  EXPECT_EQ(&all, b.all_dependent_configs()[0].ptr);

  // A should have just gotten the "all" dependent config from C.
  ASSERT_EQ(1u, a.configs().size());
  EXPECT_EQ(&all, a.configs()[0].ptr);
  EXPECT_EQ(&all, a.all_dependent_configs()[0].ptr);

  // Making an an alternate A and B with B forwarding the direct dependents.
  Target a_fwd(&settings_, Label(SourceDir("//foo/"), "a_fwd"));
  a_fwd.set_output_type(Target::EXECUTABLE);
  Target b_fwd(&settings_, Label(SourceDir("//foo/"), "b_fwd"));
  b_fwd.set_output_type(Target::STATIC_LIBRARY);
  a_fwd.deps().push_back(LabelTargetPair(&b_fwd));
  b_fwd.deps().push_back(LabelTargetPair(&c));
  b_fwd.forward_dependent_configs().push_back(LabelTargetPair(&c));

  b_fwd.OnResolved();
  a_fwd.OnResolved();

  // A_fwd should now have both configs.
  ASSERT_EQ(2u, a_fwd.configs().size());
  EXPECT_EQ(&all, a_fwd.configs()[0].ptr);
  EXPECT_EQ(&direct, a_fwd.configs()[1].ptr);
  ASSERT_EQ(1u, a_fwd.all_dependent_configs().size());
  EXPECT_EQ(&all, a_fwd.all_dependent_configs()[0].ptr);
}

// Tests that forward_dependent_configs_from works for groups, forwarding the
// group's deps' dependent configs.
TEST_F(TargetTest, ForwardDependentConfigsFromGroups) {
  Target a(&settings_, Label(SourceDir("//foo/"), "a"));
  a.set_output_type(Target::EXECUTABLE);
  Target b(&settings_, Label(SourceDir("//foo/"), "b"));
  b.set_output_type(Target::GROUP);
  Target c(&settings_, Label(SourceDir("//foo/"), "c"));
  c.set_output_type(Target::STATIC_LIBRARY);
  a.deps().push_back(LabelTargetPair(&b));
  b.deps().push_back(LabelTargetPair(&c));

  // Direct dependent config on C.
  Config direct(&settings_, Label(SourceDir("//foo/"), "direct"));
  c.direct_dependent_configs().push_back(LabelConfigPair(&direct));

  // A forwards the dependent configs from B.
  a.forward_dependent_configs().push_back(LabelTargetPair(&b));

  c.OnResolved();
  b.OnResolved();
  a.OnResolved();

  // The config should now be on A, and in A's direct dependent configs.
  ASSERT_EQ(1u, a.configs().size());
  ASSERT_EQ(&direct, a.configs()[0].ptr);
  ASSERT_EQ(1u, a.direct_dependent_configs().size());
  ASSERT_EQ(&direct, a.direct_dependent_configs()[0].ptr);
}

TEST_F(TargetTest, InheritLibs) {
  // Create a dependency chain:
  //   A (executable) -> B (shared lib) -> C (static lib) -> D (source set)
  Target a(&settings_, Label(SourceDir("//foo/"), "a"));
  a.set_output_type(Target::EXECUTABLE);
  Target b(&settings_, Label(SourceDir("//foo/"), "b"));
  b.set_output_type(Target::SHARED_LIBRARY);
  Target c(&settings_, Label(SourceDir("//foo/"), "c"));
  c.set_output_type(Target::STATIC_LIBRARY);
  Target d(&settings_, Label(SourceDir("//foo/"), "d"));
  d.set_output_type(Target::SOURCE_SET);
  a.deps().push_back(LabelTargetPair(&b));
  b.deps().push_back(LabelTargetPair(&c));
  c.deps().push_back(LabelTargetPair(&d));

  d.OnResolved();
  c.OnResolved();
  b.OnResolved();
  a.OnResolved();

  // C should have D in its inherited libs.
  const std::set<const Target*>& c_inherited = c.inherited_libraries();
  EXPECT_EQ(1u, c_inherited.size());
  EXPECT_TRUE(c_inherited.find(&d) != c_inherited.end());

  // B should have C and D in its inherited libs.
  const std::set<const Target*>& b_inherited = b.inherited_libraries();
  EXPECT_EQ(2u, b_inherited.size());
  EXPECT_TRUE(b_inherited.find(&c) != b_inherited.end());
  EXPECT_TRUE(b_inherited.find(&d) != b_inherited.end());

  // A should have B in its inherited libs, but not any others (the shared
  // library will include the static library and source set).
  const std::set<const Target*>& a_inherited = a.inherited_libraries();
  EXPECT_EQ(1u, a_inherited.size());
  EXPECT_TRUE(a_inherited.find(&b) != a_inherited.end());
}
