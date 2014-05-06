// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/ninja_target_writer.h"
#include "tools/gn/test_with_scope.h"

namespace {

class TestingNinjaTargetWriter : public NinjaTargetWriter {
 public:
  TestingNinjaTargetWriter(const Target* target,
                           const Toolchain* toolchain,
                           std::ostream& out)
      : NinjaTargetWriter(target, toolchain, out) {
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

  // Dependent target that also includes a source prerequisite (should get
  // included) and a source (should not be included).
  Target target(setup.settings(), Label(SourceDir("//foo/"), "target"));
  target.set_output_type(Target::EXECUTABLE);
  target.source_prereqs().push_back(SourceFile("//foo/input.txt"));
  target.sources().push_back(SourceFile("//foo/source.txt"));
  target.deps().push_back(LabelTargetPair(&base_target));

  // Dependent action to test that action sources will be treated the same as
  // source_prereqs.
  Target action(setup.settings(), Label(SourceDir("//foo/"), "action"));
  action.set_output_type(Target::ACTION);
  action.sources().push_back(SourceFile("//foo/action_source.txt"));
  action.deps().push_back(LabelTargetPair(&target));

  base_target.OnResolved();
  target.OnResolved();
  action.OnResolved();

  // Input deps for the base (should be nothing, it has no hard deps).
  {
    std::ostringstream stream;
    TestingNinjaTargetWriter writer(&base_target, setup.toolchain(), stream);
    std::string dep =
        writer.WriteInputDepsStampAndGetDep(std::vector<const Target*>());

    EXPECT_TRUE(dep.empty());
    EXPECT_TRUE(stream.str().empty());
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
    EXPECT_EQ("build obj/foo/action.inputdeps.stamp: stamp "
                  "../../foo/action_source.txt obj/foo/base.stamp\n",
              stream.str());
  }
}
