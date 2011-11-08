// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_instance_deprecated.h"

#include "ppapi/cpp/module.h"
#include "ppapi/cpp/dev/scriptable_object_deprecated.h"
#include "ppapi/tests/testing_instance.h"

namespace {

static const char kSetValueFunction[] = "SetValue";
static const char kSetExceptionFunction[] = "SetException";
static const char kReturnValueFunction[] = "ReturnValue";

// ScriptableObject used by instance.
class InstanceSO : public pp::deprecated::ScriptableObject {
 public:
  InstanceSO(TestInstance* i) : test_instance_(i) {}

  // pp::deprecated::ScriptableObject overrides.
  bool HasMethod(const pp::Var& name, pp::Var* exception);
  pp::Var Call(const pp::Var& name,
               const std::vector<pp::Var>& args,
               pp::Var* exception);

 private:
  TestInstance* test_instance_;
};

bool InstanceSO::HasMethod(const pp::Var& name, pp::Var* exception) {
  if (!name.is_string())
    return false;
  return name.AsString() == kSetValueFunction ||
         name.AsString() == kSetExceptionFunction ||
         name.AsString() == kReturnValueFunction;
}

pp::Var InstanceSO::Call(const pp::Var& method_name,
                         const std::vector<pp::Var>& args,
                         pp::Var* exception) {
  if (!method_name.is_string())
    return false;
  std::string name = method_name.AsString();

  if (name == kSetValueFunction) {
    if (args.size() != 1 || !args[0].is_string())
      *exception = pp::Var("Bad argument to SetValue(<string>)");
    else
      test_instance_->set_string(args[0].AsString());
  } else if (name == kSetExceptionFunction) {
    if (args.size() != 1 || !args[0].is_string())
      *exception = pp::Var("Bad argument to SetException(<string>)");
    else
      *exception = args[0];
  } else if (name == kReturnValueFunction) {
    if (args.size() != 1)
      *exception = pp::Var("Need single arg to call ReturnValue");
    else
      return args[0];
  } else {
    *exception = pp::Var("Bad function call");
  }

  return pp::Var();
}

}  // namespace

REGISTER_TEST_CASE(Instance);

TestInstance::TestInstance(TestingInstance* instance) : TestCase(instance) {
}

bool TestInstance::Init() {
  return true;
}

void TestInstance::RunTests(const std::string& filter) {
  RUN_TEST(ExecuteScript, filter);
}

pp::deprecated::ScriptableObject* TestInstance::CreateTestObject() {
  return new InstanceSO(this);
}

std::string TestInstance::TestExecuteScript() {
  // Simple call back into the plugin.
  pp::Var exception;
  pp::Var ret = instance_->ExecuteScript(
      "document.getElementById('plugin').SetValue('hello, world');",
      &exception);
  ASSERT_TRUE(ret.is_undefined());
  ASSERT_TRUE(exception.is_undefined());
  ASSERT_TRUE(string_ == "hello, world");

  // Return values from the plugin should be returned.
  ret = instance_->ExecuteScript(
      "document.getElementById('plugin').ReturnValue('return value');",
      &exception);
  ASSERT_TRUE(ret.is_string() && ret.AsString() == "return value");
  ASSERT_TRUE(exception.is_undefined());

  // Exception thrown by the plugin should be caught.
  ret = instance_->ExecuteScript(
      "document.getElementById('plugin').SetException('plugin exception');",
      &exception);
  ASSERT_TRUE(ret.is_undefined());
  ASSERT_TRUE(exception.is_string());
  // TODO(brettw) bug 54011: The TryCatch isn't working properly and
  // doesn't actually pass the exception text up.
  //ASSERT_TRUE(exception.AsString() == "plugin exception");

  // Exception caused by string evaluation should be caught.
  exception = pp::Var();
  ret = instance_->ExecuteScript("document.doesntExist()", &exception);
  ASSERT_TRUE(ret.is_undefined());
  ASSERT_TRUE(exception.is_string());  // Don't know exactly what it will say.

  PASS();
}
