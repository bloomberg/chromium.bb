// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/functions.h"
#include "tools/gn/target.h"
#include "tools/gn/test_with_scope.h"

namespace {

class GetTargetOutputsTest : public testing::Test {
 public:
  GetTargetOutputsTest() {
    // Want consistent target names so explicitly set platform.
    setup_.settings()->set_target_os(Settings::LINUX);
    setup_.scope()->set_item_collector(&items_);
  }

  Value GetTargetOutputs(const std::string& name, Err* err) {
    FunctionCallNode function;
    std::vector<Value> args;
    args.push_back(Value(NULL, name));
    return functions::RunGetTargetOutputs(setup_.scope(), &function, args, err);
  }

  // Shortcut to get a label with the current toolchain.
  Label GetLabel(const std::string& dir, const std::string& name) {
    return Label(SourceDir(dir), name, setup_.toolchain()->label().dir(),
                 setup_.toolchain()->label().name());
  }

  // Asserts that the given list contains a single string with the given value.
  void AssertSingleStringEquals(const Value& list,
                                const std::string& expected) {
    ASSERT_TRUE(list.type() == Value::LIST);
    ASSERT_EQ(1u, list.list_value().size());
    ASSERT_TRUE(list.list_value()[0].type() == Value::STRING);
    ASSERT_EQ(expected, list.list_value()[0].string_value());
  }

  void AssertTwoStringsEqual(const Value& list,
                             const std::string& expected1,
                             const std::string& expected2) {
    ASSERT_TRUE(list.type() == Value::LIST);
    ASSERT_EQ(2u, list.list_value().size());
    ASSERT_TRUE(list.list_value()[0].type() == Value::STRING);
    ASSERT_EQ(expected1, list.list_value()[0].string_value());
    ASSERT_TRUE(list.list_value()[1].type() == Value::STRING);
    ASSERT_EQ(expected2, list.list_value()[1].string_value());
  }

 protected:
  TestWithScope setup_;

  Scope::ItemVector items_;
};

}  // namespace

TEST_F(GetTargetOutputsTest, Executable) {
  Target* exe = new Target(setup_.settings(), GetLabel("//foo/", "bar"));
  exe->set_output_type(Target::EXECUTABLE);
  items_.push_back(new scoped_ptr<Item>(exe));

  Err err;
  Value result = GetTargetOutputs("//foo:bar", &err);
  ASSERT_FALSE(err.has_error());
  // The TestWithScope declares that the build is Linux, so we'll have no
  // extension for the binaries.
  AssertSingleStringEquals(result, "//out/Debug/bar");
}

TEST_F(GetTargetOutputsTest, SourceSet) {
  Target* source_set = new Target(setup_.settings(), GetLabel("//foo/", "bar"));
  source_set->set_output_type(Target::SOURCE_SET);
  items_.push_back(new scoped_ptr<Item>(source_set));

  Err err;
  Value result = GetTargetOutputs("//foo:bar", &err);
  ASSERT_FALSE(err.has_error());
  AssertSingleStringEquals(result, "//out/Debug/obj/foo/bar.stamp");
}

TEST_F(GetTargetOutputsTest, Copy) {
  Target* action = new Target(setup_.settings(), GetLabel("//foo/", "bar"));
  action->set_output_type(Target::COPY_FILES);
  action->sources().push_back(SourceFile("//file.txt"));
  action->action_values().outputs().push_back(
      "//out/Debug/{{source_file_part}}.one");

  items_.push_back(new scoped_ptr<Item>(action));

  Err err;
  Value result = GetTargetOutputs("//foo:bar", &err);
  ASSERT_FALSE(err.has_error());
  AssertSingleStringEquals(result, "//out/Debug/file.txt.one");
}

TEST_F(GetTargetOutputsTest, Action) {
  Target* action = new Target(setup_.settings(), GetLabel("//foo/", "bar"));
  action->set_output_type(Target::ACTION);
  action->action_values().outputs().push_back("//output1.txt");
  action->action_values().outputs().push_back("//output2.txt");

  items_.push_back(new scoped_ptr<Item>(action));

  Err err;
  Value result = GetTargetOutputs("//foo:bar", &err);
  ASSERT_FALSE(err.has_error());
  AssertTwoStringsEqual(result, "//output1.txt", "//output2.txt");
}

TEST_F(GetTargetOutputsTest, ActionForeach) {
  Target* action = new Target(setup_.settings(), GetLabel("//foo/", "bar"));
  action->set_output_type(Target::ACTION_FOREACH);
  action->sources().push_back(SourceFile("//file.txt"));
  action->action_values().outputs().push_back(
      "//out/Debug/{{source_file_part}}.one");
  action->action_values().outputs().push_back(
      "//out/Debug/{{source_file_part}}.two");

  items_.push_back(new scoped_ptr<Item>(action));

  Err err;
  Value result = GetTargetOutputs("//foo:bar", &err);
  ASSERT_FALSE(err.has_error());
  AssertTwoStringsEqual(result, "//out/Debug/file.txt.one",
                        "//out/Debug/file.txt.two");
}
