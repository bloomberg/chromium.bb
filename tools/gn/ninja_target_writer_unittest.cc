// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/ninja_target_writer.h"
#include "tools/gn/target.h"
#include "tools/gn/test_with_scope.h"

namespace {

class TestingNinjaTargetWriter : public NinjaTargetWriter {
 public:
  TestingNinjaTargetWriter(const Target* target,
                           const Toolchain* toolchain,
                           std::ostream& out)
      : NinjaTargetWriter(target, out) {
  }

  virtual void Run() OVERRIDE {}

  // Make this public so the test can call it.
  std::string WriteInputDepsStampAndGetDep(
      const std::vector<const Target*>& extra_hard_deps) {
    return NinjaTargetWriter::WriteInputDepsStampAndGetDep(extra_hard_deps);
  }
};

}  // namespace

TEST(NinjaTargetWriter, WriteInputDepsStampAndGetDep) {
  TestWithScope setup;

  // Make a base target that's a hard dep (action).
  Target base_target(setup.settings(), Label(SourceDir("//foo/"), "base"));
  base_target.set_output_type(Target::ACTION);
  base_target.SetToolchain(setup.toolchain());
  base_target.action_values().set_script(SourceFile("//foo/script.py"));

  // Dependent target that also includes a source prerequisite (should get
  // included) and a source (should not be included).
  Target target(setup.settings(), Label(SourceDir("//foo/"), "target"));
  target.set_output_type(Target::EXECUTABLE);
  target.SetToolchain(setup.toolchain());
  target.inputs().push_back(SourceFile("//foo/input.txt"));
  target.sources().push_back(SourceFile("//foo/source.txt"));
  target.deps().push_back(LabelTargetPair(&base_target));

  // Dependent action to test that action sources will be treated the same as
  // inputs.
  Target action(setup.settings(), Label(SourceDir("//foo/"), "action"));
  action.set_output_type(Target::ACTION);
  action.SetToolchain(setup.toolchain());
  action.action_values().set_script(SourceFile("//foo/script.py"));
  action.sources().push_back(SourceFile("//foo/action_source.txt"));
  action.deps().push_back(LabelTargetPair(&target));

  base_target.OnResolved();
  target.OnResolved();
  action.OnResolved();

  // Input deps for the base (should be only the script itself).
  {
    std::ostringstream stream;
    TestingNinjaTargetWriter writer(&base_target, setup.toolchain(), stream);
    std::string dep =
        writer.WriteInputDepsStampAndGetDep(std::vector<const Target*>());

    EXPECT_EQ(" | obj/foo/base.inputdeps.stamp", dep);
    EXPECT_EQ("build obj/foo/base.inputdeps.stamp: stamp "
                  "../../foo/script.py\n",
              stream.str());
  }

  // Input deps for the target (should depend on the base).
  {
    std::ostringstream stream;
    TestingNinjaTargetWriter writer(&target, setup.toolchain(), stream);
    std::string dep =
        writer.WriteInputDepsStampAndGetDep(std::vector<const Target*>());

    EXPECT_EQ(" | obj/foo/target.inputdeps.stamp", dep);
    EXPECT_EQ("build obj/foo/target.inputdeps.stamp: stamp "
                  "../../foo/input.txt obj/foo/base.stamp\n",
              stream.str());
  }

  // Input deps for action which should depend on the base since its a hard dep
  // that is a (indirect) dependency, as well as the the action source.
  {
    std::ostringstream stream;
    TestingNinjaTargetWriter writer(&action, setup.toolchain(), stream);
    std::string dep =
        writer.WriteInputDepsStampAndGetDep(std::vector<const Target*>());

    EXPECT_EQ(" | obj/foo/action.inputdeps.stamp", dep);
    EXPECT_EQ("build obj/foo/action.inputdeps.stamp: stamp ../../foo/script.py "
                  "../../foo/action_source.txt obj/foo/base.stamp\n",
              stream.str());
  }
}

// Tests WriteInputDepsStampAndGetDep when toolchain deps are present.
TEST(NinjaTargetWriter, WriteInputDepsStampAndGetDepWithToolchainDeps) {
  TestWithScope setup;

  // Toolchain dependency. Here we make a target in the same toolchain for
  // simplicity, but in real life (using the Builder) this would be rejected
  // because it would be a circular dependency (the target depends on its
  // toolchain, and the toolchain depends on this target).
  Target toolchain_dep_target(setup.settings(),
                              Label(SourceDir("//foo/"), "setup"));
  toolchain_dep_target.set_output_type(Target::ACTION);
  toolchain_dep_target.SetToolchain(setup.toolchain());
  toolchain_dep_target.OnResolved();
  setup.toolchain()->deps().push_back(LabelTargetPair(&toolchain_dep_target));

  // Make a binary target
  Target target(setup.settings(), Label(SourceDir("//foo/"), "target"));
  target.set_output_type(Target::EXECUTABLE);
  target.SetToolchain(setup.toolchain());
  target.OnResolved();

  std::ostringstream stream;
  TestingNinjaTargetWriter writer(&target, setup.toolchain(), stream);
  std::string dep =
      writer.WriteInputDepsStampAndGetDep(std::vector<const Target*>());

  EXPECT_EQ(" | obj/foo/target.inputdeps.stamp", dep);
  EXPECT_EQ("build obj/foo/target.inputdeps.stamp: stamp "
                "obj/foo/setup.stamp\n",
            stream.str());
}
