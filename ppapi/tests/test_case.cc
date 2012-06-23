// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_case.h"

#include <sstream>

#include "ppapi/tests/pp_thread.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

TestCase::TestCase(TestingInstance* instance)
    : instance_(instance),
      testing_interface_(NULL),
      callback_type_(PP_REQUIRED) {
  // Get the testing_interface_ if it is available, so that we can do Resource
  // and Var checks on shutdown (see CheckResourcesAndVars). If it is not
  // available, testing_interface_ will be NULL. Some tests do not require it.
  testing_interface_ = GetTestingInterface();
}

TestCase::~TestCase() {
}

bool TestCase::Init() {
  return true;
}

// static
std::string TestCase::MakeFailureMessage(const char* file,
                                         int line,
                                         const char* cmd) {
  // The mere presence of this local variable works around a gcc-4.2.4
  // compiler bug in official Chrome Linux builds.  If you remove it,
  // confirm this compile command still works:
  // GYP_DEFINES='branding=Chrome buildtype=Official target_arch=x64'
  //     gclient runhooks
  // make -k -j4 BUILDTYPE=Release ppapi_tests
  std::string s;

  std::ostringstream output;
  output << "Failure in " << file << "(" << line << "): " << cmd;
  return output.str();
}

#if !(defined __native_client__)
pp::VarPrivate TestCase::GetTestObject() {
  if (test_object_.is_undefined()) {
    pp::deprecated::ScriptableObject* so = CreateTestObject();
    if (so) {
      test_object_ = pp::VarPrivate(instance_, so);  // Takes ownership.
      // CheckResourcesAndVars runs and looks for leaks before we've actually
      // completely shut down. Ignore the instance object, since it's not a real
      // leak.
      IgnoreLeakedVar(test_object_.pp_var().value.as_id);
    }
  }
  return test_object_;
}
#endif

bool TestCase::CheckTestingInterface() {
  testing_interface_ = GetTestingInterface();
  if (!testing_interface_) {
    // Give a more helpful error message for the testing interface being gone
    // since that needs special enabling in Chrome.
    instance_->AppendError("This test needs the testing interface, which is "
                           "not currently available. In Chrome, use "
                           "--enable-pepper-testing when launching.");
    return false;
  }

  return true;
}

void TestCase::HandleMessage(const pp::Var& message_data) {
}

void TestCase::DidChangeView(const pp::View& view) {
}

bool TestCase::HandleInputEvent(const pp::InputEvent& event) {
  return false;
}

void TestCase::IgnoreLeakedVar(int64_t id) {
  ignored_leaked_vars_.insert(id);
}

#if !(defined __native_client__)
pp::deprecated::ScriptableObject* TestCase::CreateTestObject() {
  return NULL;
}
#endif

bool TestCase::EnsureRunningOverHTTP() {
  if (instance_->protocol() != "http:") {
    instance_->AppendError("This test needs to be run over HTTP.");
    return false;
  }

  return true;
}

bool TestCase::MatchesFilter(const std::string& test_name,
                             const std::string& filter) {
  return filter.empty() || (test_name == filter);
}

std::string TestCase::CheckResourcesAndVars(std::string errors) {
  if (!errors.empty())
    return errors;

  if (testing_interface_) {
    // TODO(dmichael): Fix tests that leak resources and enable the following:
    /*
    uint32_t leaked_resources =
        testing_interface_->GetLiveObjectsForInstance(instance_->pp_instance());
    if (leaked_resources) {
      std::ostringstream output;
      output << "FAILED: Test leaked " << leaked_resources << " resources.\n";
      errors += output.str();
    }
    */
    const int kVarsToPrint = 100;
    PP_Var vars[kVarsToPrint];
    int found_vars = testing_interface_->GetLiveVars(vars, kVarsToPrint);
    // This will undercount if we are told to ignore a Var which is then *not*
    // leaked. Worst case, we just won't print the little "Test leaked" message,
    // but we'll still print any non-ignored leaked vars we found.
    int leaked_vars =
        found_vars - static_cast<int>(ignored_leaked_vars_.size());
    if (leaked_vars > 0) {
      std::ostringstream output;
      output << "Test leaked " << leaked_vars << " vars (printing at most "
             << kVarsToPrint <<"):<p>";
      errors += output.str();
    }
    for (int i = 0; i < std::min(found_vars, kVarsToPrint); ++i) {
      pp::Var leaked_var(pp::PASS_REF, vars[i]);
      if (ignored_leaked_vars_.count(leaked_var.pp_var().value.as_id) == 0)
        errors += leaked_var.DebugString() + "<p>";
    }
  }
  return errors;
}

// static
void TestCase::QuitMainMessageLoop(PP_Instance instance) {
  PP_Instance* heap_instance = new PP_Instance(instance);
  pp::CompletionCallback callback(&DoQuitMainMessageLoop, heap_instance);
  pp::Module::Get()->core()->CallOnMainThread(0, callback);
}

// static
void TestCase::DoQuitMainMessageLoop(void* pp_instance, int32_t result) {
  PP_Instance* instance = static_cast<PP_Instance*>(pp_instance);
  GetTestingInterface()->QuitMessageLoop(*instance);
  delete instance;
}

void TestCase::RunOnThreadInternal(void (*thread_func)(void*),
                                   void* thread_param,
                                   const PPB_Testing_Dev* testing_interface) {
    PP_ThreadType thread;
    PP_CreateThread(&thread, thread_func, thread_param);
    // Run a message loop so pepper calls can be dispatched. The background
    // thread will set result_ and make us Quit when it's done.
    testing_interface->RunMessageLoop(instance_->pp_instance());
    PP_JoinThread(thread);
}
