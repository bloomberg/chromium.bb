// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_case.h"

#include <sstream>

#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

TestCase::TestCase(TestingInstance* instance)
    : instance_(instance),
      testing_interface_(NULL),
      force_async_(false) {
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
    if (so)
      test_object_ = pp::VarPrivate(instance_, so);  // Takes ownership.
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

std::string TestCase::CheckResourcesAndVars() {
  std::string errors;
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
    const uint32_t kVarsToPrint = 10;
    PP_Var vars[kVarsToPrint];
    uint32_t leaked_vars = testing_interface_->GetLiveVars(vars, kVarsToPrint);
    uint32_t tracked_vars = leaked_vars;
#if !(defined __native_client__)
    // Don't count test_object_ as a leak.
    if (test_object_.pp_var().type > PP_VARTYPE_DOUBLE)
      --leaked_vars;
#endif
    if (leaked_vars) {
      std::ostringstream output;
      output << "Test leaked " << leaked_vars << " vars (printing at most "
             << kVarsToPrint <<"):<p>";
      errors += output.str();
      for (uint32_t i = 0; i < std::min(tracked_vars, kVarsToPrint); ++i) {
        pp::Var leaked_var(pp::Var::PassRef(), vars[i]);
#if (defined __native_client__)
        errors += leaked_var.DebugString() + "<p>";
#else
        if (!(leaked_var == test_object_))
          errors += leaked_var.DebugString() + "<p>";
#endif
      }
    }
  }
  return errors;
}

