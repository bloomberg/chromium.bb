// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_instance_deprecated.h"

#include <assert.h>

#include "ppapi/c/ppb_var.h"
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
  InstanceSO(TestInstance* i);
  virtual ~InstanceSO();

  // pp::deprecated::ScriptableObject overrides.
  bool HasMethod(const pp::Var& name, pp::Var* exception);
  pp::Var Call(const pp::Var& name,
               const std::vector<pp::Var>& args,
               pp::Var* exception);

 private:
  TestInstance* test_instance_;
};

InstanceSO::InstanceSO(TestInstance* i) : test_instance_(i) {
  // Set up a post-condition for the test so that we can ensure our destructor
  // is called. This only works in-process right now. Rather than disable the
  // whole test, we only do this check when running in-process.
  // TODO(dmichael): Figure out if we want this to work out-of-process, and if
  //                 so, fix it. Note that it might just be failing because the
  //                 ReleaseObject and Deallocate messages are asynchronous.
  if (i->testing_interface() &&
      i->testing_interface()->IsOutOfProcess() == PP_FALSE) {
    i->instance()->AddPostCondition(
        "window.document.getElementById('container').instance_object_destroyed"
        );
  }
}

InstanceSO::~InstanceSO() {
  pp::Var exception;
  pp::Var ret = test_instance_->instance()->ExecuteScript(
      "document.getElementById('container').instance_object_destroyed=true;");
}

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
  RUN_TEST(RecursiveObjects, filter);
  RUN_TEST(LeakedObjectDestructors, filter);
}

void TestInstance::LeakReferenceAndIgnore(const pp::Var& leaked) {
  static const PPB_Var* var_interface = static_cast<const PPB_Var*>(
        pp::Module::Get()->GetBrowserInterface(PPB_VAR_INTERFACE));
  var_interface->AddRef(leaked.pp_var());
  IgnoreLeakedVar(leaked.pp_var().value.as_id);
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
  // Due to a limitation in the implementation of TryCatch, it doesn't actually
  // pass the strings up. Since this is a trusted only interface, we've decided
  // not to bother fixing this for now.

  // Exception caused by string evaluation should be caught.
  exception = pp::Var();
  ret = instance_->ExecuteScript("document.doesntExist()", &exception);
  ASSERT_TRUE(ret.is_undefined());
  ASSERT_TRUE(exception.is_string());  // Don't know exactly what it will say.

  PASS();
}

// A scriptable object that contains other scriptable objects recursively. This
// is used to help verify that our scriptable object clean-up code works
// properly.
class ObjectWithChildren : public pp::deprecated::ScriptableObject {
 public:
  ObjectWithChildren(TestInstance* i, int num_descendents) {
    if (num_descendents > 0) {
      child_ = pp::VarPrivate(i->instance(),
                              new ObjectWithChildren(i, num_descendents - 1));
    }
  }
  struct IgnoreLeaks {};
  ObjectWithChildren(TestInstance* i, int num_descendents, IgnoreLeaks) {
    if (num_descendents > 0) {
      child_ = pp::VarPrivate(i->instance(),
                              new ObjectWithChildren(i, num_descendents - 1,
                                                     IgnoreLeaks()));
      i->IgnoreLeakedVar(child_.pp_var().value.as_id);
    }
  }
 private:
  pp::VarPrivate child_;
};

std::string TestInstance::TestRecursiveObjects() {
  // These should be deleted when we exit scope, so should not leak.
  pp::VarPrivate not_leaked(instance(), new ObjectWithChildren(this, 50));

  // Leak some, but tell TestCase to ignore the leaks. This test is run and then
  // reloaded (see ppapi_uitest.cc). If these aren't cleaned up when the first
  // run is torn down, they will show up as leaks in the second run.
  // NOTE: The ScriptableObjects are actually leaked, but they should be removed
  //       from the tracker. See below for a test that verifies that the
  //       destructor is not run.
  pp::VarPrivate leaked(
      instance(),
      new ObjectWithChildren(this, 50, ObjectWithChildren::IgnoreLeaks()));
  // Now leak a reference to the root object. This should force the root and
  // all its descendents to stay in the tracker.
  LeakReferenceAndIgnore(leaked);

  PASS();
}

// A scriptable object that should cause a crash if its destructor is run. We
// don't run the destructor for objects which the plugin leaks. This is to
// prevent them doing dangerous things at cleanup time, such as executing script
// or creating new objects.
class BadDestructorObject : public pp::deprecated::ScriptableObject {
 public:
  BadDestructorObject() {}
  ~BadDestructorObject() {
    assert(false);
  }
};

std::string TestInstance::TestLeakedObjectDestructors() {
  pp::VarPrivate leaked(instance(), new BadDestructorObject());
  // Leak a reference so it gets deleted on instance shutdown.
  LeakReferenceAndIgnore(leaked);
  PASS();
}

