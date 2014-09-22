// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/config.h"
#include "tools/gn/settings.h"
#include "tools/gn/target.h"
#include "tools/gn/test_with_scope.h"
#include "tools/gn/toolchain.h"

// Tests that lib[_dir]s are inherited across deps boundaries for static
// libraries but not executables.
TEST(Target, LibInheritance) {
  TestWithScope setup;
  Err err;

  const std::string lib("foo");
  const SourceDir libdir("/foo_dir/");

  // Leaf target with ldflags set.
  Target z(setup.settings(), Label(SourceDir("//foo/"), "z"));
  z.set_output_type(Target::STATIC_LIBRARY);
  z.config_values().libs().push_back(lib);
  z.config_values().lib_dirs().push_back(libdir);
  z.visibility().SetPublic();
  z.SetToolchain(setup.toolchain());
  ASSERT_TRUE(z.OnResolved(&err));

  // All lib[_dir]s should be set when target is resolved.
  ASSERT_EQ(1u, z.all_libs().size());
  EXPECT_EQ(lib, z.all_libs()[0]);
  ASSERT_EQ(1u, z.all_lib_dirs().size());
  EXPECT_EQ(libdir, z.all_lib_dirs()[0]);

  // Shared library target should inherit the libs from the static library
  // and its own. Its own flag should be before the inherited one.
  const std::string second_lib("bar");
  const SourceDir second_libdir("/bar_dir/");
  Target shared(setup.settings(), Label(SourceDir("//foo/"), "shared"));
  shared.set_output_type(Target::SHARED_LIBRARY);
  shared.config_values().libs().push_back(second_lib);
  shared.config_values().lib_dirs().push_back(second_libdir);
  shared.private_deps().push_back(LabelTargetPair(&z));
  shared.visibility().SetPublic();
  shared.SetToolchain(setup.toolchain());
  ASSERT_TRUE(shared.OnResolved(&err));

  ASSERT_EQ(2u, shared.all_libs().size());
  EXPECT_EQ(second_lib, shared.all_libs()[0]);
  EXPECT_EQ(lib, shared.all_libs()[1]);
  ASSERT_EQ(2u, shared.all_lib_dirs().size());
  EXPECT_EQ(second_libdir, shared.all_lib_dirs()[0]);
  EXPECT_EQ(libdir, shared.all_lib_dirs()[1]);

  // Executable target shouldn't get either by depending on shared.
  Target exec(setup.settings(), Label(SourceDir("//foo/"), "exec"));
  exec.set_output_type(Target::EXECUTABLE);
  exec.private_deps().push_back(LabelTargetPair(&shared));
  exec.SetToolchain(setup.toolchain());
  ASSERT_TRUE(exec.OnResolved(&err));
  EXPECT_EQ(0u, exec.all_libs().size());
  EXPECT_EQ(0u, exec.all_lib_dirs().size());
}

// Test all_dependent_configs, public_config inheritance, and
// forward_dependent_configs_from
TEST(Target, DependentConfigs) {
  TestWithScope setup;
  Err err;

  // Set up a dependency chain of a -> b -> c
  Target a(setup.settings(), Label(SourceDir("//foo/"), "a"));
  a.set_output_type(Target::EXECUTABLE);
  a.visibility().SetPublic();
  a.SetToolchain(setup.toolchain());
  Target b(setup.settings(), Label(SourceDir("//foo/"), "b"));
  b.set_output_type(Target::STATIC_LIBRARY);
  b.visibility().SetPublic();
  b.SetToolchain(setup.toolchain());
  Target c(setup.settings(), Label(SourceDir("//foo/"), "c"));
  c.set_output_type(Target::STATIC_LIBRARY);
  c.visibility().SetPublic();
  c.SetToolchain(setup.toolchain());
  a.private_deps().push_back(LabelTargetPair(&b));
  b.private_deps().push_back(LabelTargetPair(&c));

  // Normal non-inherited config.
  Config config(setup.settings(), Label(SourceDir("//foo/"), "config"));
  c.configs().push_back(LabelConfigPair(&config));

  // All dependent config.
  Config all(setup.settings(), Label(SourceDir("//foo/"), "all"));
  c.all_dependent_configs().push_back(LabelConfigPair(&all));

  // Direct dependent config.
  Config direct(setup.settings(), Label(SourceDir("//foo/"), "direct"));
  c.public_configs().push_back(LabelConfigPair(&direct));

  ASSERT_TRUE(c.OnResolved(&err));
  ASSERT_TRUE(b.OnResolved(&err));
  ASSERT_TRUE(a.OnResolved(&err));

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
  Target a_fwd(setup.settings(), Label(SourceDir("//foo/"), "a_fwd"));
  a_fwd.set_output_type(Target::EXECUTABLE);
  a_fwd.visibility().SetPublic();
  a_fwd.SetToolchain(setup.toolchain());
  Target b_fwd(setup.settings(), Label(SourceDir("//foo/"), "b_fwd"));
  b_fwd.set_output_type(Target::STATIC_LIBRARY);
  b_fwd.SetToolchain(setup.toolchain());
  b_fwd.visibility().SetPublic();
  a_fwd.private_deps().push_back(LabelTargetPair(&b_fwd));
  b_fwd.private_deps().push_back(LabelTargetPair(&c));
  b_fwd.forward_dependent_configs().push_back(LabelTargetPair(&c));

  ASSERT_TRUE(b_fwd.OnResolved(&err));
  ASSERT_TRUE(a_fwd.OnResolved(&err));

  // A_fwd should now have both configs.
  ASSERT_EQ(2u, a_fwd.configs().size());
  EXPECT_EQ(&all, a_fwd.configs()[0].ptr);
  EXPECT_EQ(&direct, a_fwd.configs()[1].ptr);
  ASSERT_EQ(1u, a_fwd.all_dependent_configs().size());
  EXPECT_EQ(&all, a_fwd.all_dependent_configs()[0].ptr);
}

TEST(Target, InheritLibs) {
  TestWithScope setup;
  Err err;

  // Create a dependency chain:
  //   A (executable) -> B (shared lib) -> C (static lib) -> D (source set)
  Target a(setup.settings(), Label(SourceDir("//foo/"), "a"));
  a.set_output_type(Target::EXECUTABLE);
  a.visibility().SetPublic();
  a.SetToolchain(setup.toolchain());
  Target b(setup.settings(), Label(SourceDir("//foo/"), "b"));
  b.set_output_type(Target::SHARED_LIBRARY);
  b.visibility().SetPublic();
  b.SetToolchain(setup.toolchain());
  Target c(setup.settings(), Label(SourceDir("//foo/"), "c"));
  c.set_output_type(Target::STATIC_LIBRARY);
  c.visibility().SetPublic();
  c.SetToolchain(setup.toolchain());
  Target d(setup.settings(), Label(SourceDir("//foo/"), "d"));
  d.set_output_type(Target::SOURCE_SET);
  d.visibility().SetPublic();
  d.SetToolchain(setup.toolchain());
  a.private_deps().push_back(LabelTargetPair(&b));
  b.private_deps().push_back(LabelTargetPair(&c));
  c.private_deps().push_back(LabelTargetPair(&d));

  ASSERT_TRUE(d.OnResolved(&err));
  ASSERT_TRUE(c.OnResolved(&err));
  ASSERT_TRUE(b.OnResolved(&err));
  ASSERT_TRUE(a.OnResolved(&err));

  // C should have D in its inherited libs.
  const UniqueVector<const Target*>& c_inherited = c.inherited_libraries();
  EXPECT_EQ(1u, c_inherited.size());
  EXPECT_TRUE(c_inherited.IndexOf(&d) != static_cast<size_t>(-1));

  // B should have C and D in its inherited libs.
  const UniqueVector<const Target*>& b_inherited = b.inherited_libraries();
  EXPECT_EQ(2u, b_inherited.size());
  EXPECT_TRUE(b_inherited.IndexOf(&c) != static_cast<size_t>(-1));
  EXPECT_TRUE(b_inherited.IndexOf(&d) != static_cast<size_t>(-1));

  // A should have B in its inherited libs, but not any others (the shared
  // library will include the static library and source set).
  const UniqueVector<const Target*>& a_inherited = a.inherited_libraries();
  EXPECT_EQ(1u, a_inherited.size());
  EXPECT_TRUE(a_inherited.IndexOf(&b) != static_cast<size_t>(-1));
}

TEST(Target, InheritCompleteStaticLib) {
  TestWithScope setup;
  Err err;

  // Create a dependency chain:
  //   A (executable) -> B (complete static lib) -> C (source set)
  Target a(setup.settings(), Label(SourceDir("//foo/"), "a"));
  a.set_output_type(Target::EXECUTABLE);
  a.visibility().SetPublic();
  a.SetToolchain(setup.toolchain());
  Target b(setup.settings(), Label(SourceDir("//foo/"), "b"));
  b.set_output_type(Target::STATIC_LIBRARY);
  b.visibility().SetPublic();
  b.set_complete_static_lib(true);
  b.SetToolchain(setup.toolchain());
  Target c(setup.settings(), Label(SourceDir("//foo/"), "c"));
  c.set_output_type(Target::SOURCE_SET);
  c.visibility().SetPublic();
  c.SetToolchain(setup.toolchain());
  a.public_deps().push_back(LabelTargetPair(&b));
  b.public_deps().push_back(LabelTargetPair(&c));

  ASSERT_TRUE(c.OnResolved(&err));
  ASSERT_TRUE(b.OnResolved(&err));
  ASSERT_TRUE(a.OnResolved(&err));

  // B should have C in its inherited libs.
  const UniqueVector<const Target*>& b_inherited = b.inherited_libraries();
  EXPECT_EQ(1u, b_inherited.size());
  EXPECT_TRUE(b_inherited.IndexOf(&c) != static_cast<size_t>(-1));

  // A should have B in its inherited libs, but not any others (the complete
  // static library will include the source set).
  const UniqueVector<const Target*>& a_inherited = a.inherited_libraries();
  EXPECT_EQ(1u, a_inherited.size());
  EXPECT_TRUE(a_inherited.IndexOf(&b) != static_cast<size_t>(-1));
}

TEST(Target, InheritCompleteStaticLibNoDirectStaticLibDeps) {
  TestWithScope setup;
  Err err;

  // Create a dependency chain:
  //   A (complete static lib) -> B (static lib)
  Target a(setup.settings(), Label(SourceDir("//foo/"), "a"));
  a.set_output_type(Target::STATIC_LIBRARY);
  a.visibility().SetPublic();
  a.set_complete_static_lib(true);
  a.SetToolchain(setup.toolchain());
  Target b(setup.settings(), Label(SourceDir("//foo/"), "b"));
  b.set_output_type(Target::STATIC_LIBRARY);
  b.visibility().SetPublic();
  b.SetToolchain(setup.toolchain());

  a.public_deps().push_back(LabelTargetPair(&b));
  ASSERT_TRUE(b.OnResolved(&err));
  ASSERT_FALSE(a.OnResolved(&err));
}

TEST(Target, InheritCompleteStaticLibNoIheritedStaticLibDeps) {
  TestWithScope setup;
  Err err;

  // Create a dependency chain:
  //   A (complete static lib) -> B (source set) -> C (static lib)
  Target a(setup.settings(), Label(SourceDir("//foo/"), "a"));
  a.set_output_type(Target::STATIC_LIBRARY);
  a.visibility().SetPublic();
  a.set_complete_static_lib(true);
  a.SetToolchain(setup.toolchain());
  Target b(setup.settings(), Label(SourceDir("//foo/"), "b"));
  b.set_output_type(Target::SOURCE_SET);
  b.visibility().SetPublic();
  b.SetToolchain(setup.toolchain());
  Target c(setup.settings(), Label(SourceDir("//foo/"), "c"));
  c.set_output_type(Target::STATIC_LIBRARY);
  c.visibility().SetPublic();
  c.SetToolchain(setup.toolchain());

  a.public_deps().push_back(LabelTargetPair(&b));
  b.public_deps().push_back(LabelTargetPair(&c));

  ASSERT_TRUE(c.OnResolved(&err));
  ASSERT_TRUE(b.OnResolved(&err));
  ASSERT_FALSE(a.OnResolved(&err));
}

TEST(Target, GetComputedOutputName) {
  TestWithScope setup;
  Err err;

  // Basic target with no prefix (executable type tool in the TestWithScope has
  // no prefix) or output name.
  Target basic(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  basic.set_output_type(Target::EXECUTABLE);
  basic.SetToolchain(setup.toolchain());
  ASSERT_TRUE(basic.OnResolved(&err));
  EXPECT_EQ("bar", basic.GetComputedOutputName(false));
  EXPECT_EQ("bar", basic.GetComputedOutputName(true));

  // Target with no prefix but an output name.
  Target with_name(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  with_name.set_output_type(Target::EXECUTABLE);
  with_name.set_output_name("myoutput");
  with_name.SetToolchain(setup.toolchain());
  ASSERT_TRUE(with_name.OnResolved(&err));
  EXPECT_EQ("myoutput", with_name.GetComputedOutputName(false));
  EXPECT_EQ("myoutput", with_name.GetComputedOutputName(true));

  // Target with a "lib" prefix (the static library tool in the TestWithScope
  // should specify a "lib" output prefix).
  Target with_prefix(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  with_prefix.set_output_type(Target::STATIC_LIBRARY);
  with_prefix.SetToolchain(setup.toolchain());
  ASSERT_TRUE(with_prefix.OnResolved(&err));
  EXPECT_EQ("bar", with_prefix.GetComputedOutputName(false));
  EXPECT_EQ("libbar", with_prefix.GetComputedOutputName(true));

  // Target with a "lib" prefix that already has it applied. The prefix should
  // not duplicate something already in the target name.
  Target dup_prefix(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  dup_prefix.set_output_type(Target::STATIC_LIBRARY);
  dup_prefix.set_output_name("libbar");
  dup_prefix.SetToolchain(setup.toolchain());
  ASSERT_TRUE(dup_prefix.OnResolved(&err));
  EXPECT_EQ("libbar", dup_prefix.GetComputedOutputName(false));
  EXPECT_EQ("libbar", dup_prefix.GetComputedOutputName(true));
}

// Test visibility failure case.
TEST(Target, VisibilityFails) {
  TestWithScope setup;
  Err err;

  Target b(setup.settings(), Label(SourceDir("//private/"), "b"));
  b.set_output_type(Target::STATIC_LIBRARY);
  b.SetToolchain(setup.toolchain());
  b.visibility().SetPrivate(b.label().dir());
  ASSERT_TRUE(b.OnResolved(&err));

  // Make a target depending on "b". The dependency must have an origin to mark
  // it as user-set so we check visibility. This check should fail.
  Target a(setup.settings(), Label(SourceDir("//app/"), "a"));
  a.set_output_type(Target::EXECUTABLE);
  a.private_deps().push_back(LabelTargetPair(&b));
  IdentifierNode origin;  // Dummy origin.
  a.private_deps()[0].origin = &origin;
  a.SetToolchain(setup.toolchain());
  ASSERT_FALSE(a.OnResolved(&err));
}

// Test visibility with a single data_dep.
TEST(Target, VisibilityDatadeps) {
  TestWithScope setup;
  Err err;

  Target b(setup.settings(), Label(SourceDir("//public/"), "b"));
  b.set_output_type(Target::STATIC_LIBRARY);
  b.SetToolchain(setup.toolchain());
  b.visibility().SetPublic();
  ASSERT_TRUE(b.OnResolved(&err));

  // Make a target depending on "b". The dependency must have an origin to mark
  // it as user-set so we check visibility. This check should fail.
  Target a(setup.settings(), Label(SourceDir("//app/"), "a"));
  a.set_output_type(Target::EXECUTABLE);
  a.data_deps().push_back(LabelTargetPair(&b));
  IdentifierNode origin;  // Dummy origin.
  a.data_deps()[0].origin = &origin;
  a.SetToolchain(setup.toolchain());
  ASSERT_TRUE(a.OnResolved(&err)) << err.help_text();
}

// Tests that A -> Group -> B where the group is visible from A but B isn't,
// passes visibility even though the group's deps get expanded into A.
TEST(Target, VisibilityGroup) {
  TestWithScope setup;
  Err err;

  IdentifierNode origin;  // Dummy origin.

  // B has private visibility. This lets the group see it since the group is in
  // the same directory.
  Target b(setup.settings(), Label(SourceDir("//private/"), "b"));
  b.set_output_type(Target::STATIC_LIBRARY);
  b.SetToolchain(setup.toolchain());
  b.visibility().SetPrivate(b.label().dir());
  ASSERT_TRUE(b.OnResolved(&err));

  // The group has public visibility and depends on b.
  Target g(setup.settings(), Label(SourceDir("//private/"), "g"));
  g.set_output_type(Target::GROUP);
  g.SetToolchain(setup.toolchain());
  g.private_deps().push_back(LabelTargetPair(&b));
  g.private_deps()[0].origin = &origin;
  g.visibility().SetPublic();
  ASSERT_TRUE(b.OnResolved(&err));

  // Make a target depending on "g". This should succeed.
  Target a(setup.settings(), Label(SourceDir("//app/"), "a"));
  a.set_output_type(Target::EXECUTABLE);
  a.private_deps().push_back(LabelTargetPair(&g));
  a.private_deps()[0].origin = &origin;
  a.SetToolchain(setup.toolchain());
  ASSERT_TRUE(a.OnResolved(&err));
}

// Verifies that only testonly targets can depend on other testonly targets.
// Many of the above dependency checking cases covered the non-testonly
// case.
TEST(Target, Testonly) {
  TestWithScope setup;
  Err err;

  // "testlib" is a test-only library.
  Target testlib(setup.settings(), Label(SourceDir("//test/"), "testlib"));
  testlib.set_testonly(true);
  testlib.set_output_type(Target::STATIC_LIBRARY);
  testlib.visibility().SetPublic();
  testlib.SetToolchain(setup.toolchain());
  ASSERT_TRUE(testlib.OnResolved(&err));

  // "test" is a test-only executable depending on testlib, this is OK.
  Target test(setup.settings(), Label(SourceDir("//test/"), "test"));
  test.set_testonly(true);
  test.set_output_type(Target::EXECUTABLE);
  test.private_deps().push_back(LabelTargetPair(&testlib));
  test.SetToolchain(setup.toolchain());
  ASSERT_TRUE(test.OnResolved(&err));

  // "product" is a non-test depending on testlib. This should fail.
  Target product(setup.settings(), Label(SourceDir("//app/"), "product"));
  product.set_testonly(false);
  product.set_output_type(Target::EXECUTABLE);
  product.private_deps().push_back(LabelTargetPair(&testlib));
  product.SetToolchain(setup.toolchain());
  ASSERT_FALSE(product.OnResolved(&err));
}

TEST(Target, PublicConfigs) {
  TestWithScope setup;
  Err err;

  Label pub_config_label(SourceDir("//a/"), "pubconfig");
  Config pub_config(setup.settings(), pub_config_label);

  // This is the destination target that has a public config.
  Target dest(setup.settings(), Label(SourceDir("//a/"), "a"));
  dest.set_output_type(Target::SOURCE_SET);
  dest.visibility().SetPublic();
  dest.SetToolchain(setup.toolchain());
  dest.public_configs().push_back(LabelConfigPair(&pub_config));
  ASSERT_TRUE(dest.OnResolved(&err));

  // This target has a public dependency on dest.
  Target pub(setup.settings(), Label(SourceDir("//a/"), "pub"));
  pub.set_output_type(Target::SOURCE_SET);
  pub.visibility().SetPublic();
  pub.SetToolchain(setup.toolchain());
  pub.public_deps().push_back(LabelTargetPair(&dest));
  ASSERT_TRUE(pub.OnResolved(&err));

  // Depending on the target with the public dependency should forward dest's
  // to the current target.
  Target dep_on_pub(setup.settings(), Label(SourceDir("//a/"), "dop"));
  dep_on_pub.set_output_type(Target::SOURCE_SET);
  dep_on_pub.visibility().SetPublic();
  dep_on_pub.SetToolchain(setup.toolchain());
  dep_on_pub.private_deps().push_back(LabelTargetPair(&pub));
  ASSERT_TRUE(dep_on_pub.OnResolved(&err));
  ASSERT_EQ(1u, dep_on_pub.configs().size());
  EXPECT_EQ(&pub_config, dep_on_pub.configs()[0].ptr);

  // This target has a private dependency on dest for forwards configs.
  Target forward(setup.settings(), Label(SourceDir("//a/"), "f"));
  forward.set_output_type(Target::SOURCE_SET);
  forward.visibility().SetPublic();
  forward.SetToolchain(setup.toolchain());
  forward.private_deps().push_back(LabelTargetPair(&dest));
  forward.forward_dependent_configs().push_back(LabelTargetPair(&dest));
  ASSERT_TRUE(forward.OnResolved(&err));

  // Depending on the forward target should apply the config.
  Target dep_on_forward(setup.settings(), Label(SourceDir("//a/"), "dof"));
  dep_on_forward.set_output_type(Target::SOURCE_SET);
  dep_on_forward.visibility().SetPublic();
  dep_on_forward.SetToolchain(setup.toolchain());
  dep_on_forward.private_deps().push_back(LabelTargetPair(&forward));
  ASSERT_TRUE(dep_on_forward.OnResolved(&err));
  ASSERT_EQ(1u, dep_on_forward.configs().size());
  EXPECT_EQ(&pub_config, dep_on_forward.configs()[0].ptr);
}

// Tests that different link/depend outputs work for solink tools.
TEST(Target, LinkAndDepOutputs) {
  TestWithScope setup;
  Err err;

  Toolchain toolchain(setup.settings(), Label(SourceDir("//tc/"), "tc"));

  scoped_ptr<Tool> solink_tool(new Tool());
  solink_tool->set_output_prefix("lib");
  solink_tool->set_default_output_extension(".so");

  const char kLinkPattern[] =
      "{{root_out_dir}}/{{target_output_name}}{{output_extension}}";
  SubstitutionPattern link_output = SubstitutionPattern::MakeForTest(
      kLinkPattern);
  solink_tool->set_link_output(link_output);

  const char kDependPattern[] =
      "{{root_out_dir}}/{{target_output_name}}{{output_extension}}.TOC";
  SubstitutionPattern depend_output = SubstitutionPattern::MakeForTest(
      kDependPattern);
  solink_tool->set_depend_output(depend_output);

  solink_tool->set_outputs(SubstitutionList::MakeForTest(
      kLinkPattern, kDependPattern));

  toolchain.SetTool(Toolchain::TYPE_SOLINK, solink_tool.Pass());

  Target target(setup.settings(), Label(SourceDir("//a/"), "a"));
  target.set_output_type(Target::SHARED_LIBRARY);
  target.SetToolchain(&toolchain);
  ASSERT_TRUE(target.OnResolved(&err));

  EXPECT_EQ("./liba.so", target.link_output_file().value());
  EXPECT_EQ("./liba.so.TOC", target.dependency_output_file().value());
}
