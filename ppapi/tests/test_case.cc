// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_case.h"

#include <sstream>

#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

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

pp::Var TestCase::GetTestObject() {
  if (test_object_.is_undefined()) {
    pp::deprecated::ScriptableObject* so = CreateTestObject();
    if (so)
      test_object_ = pp::Var(so);  // Takes ownership.
  }
  return test_object_;
}

pp::deprecated::ScriptableObject* TestCase::CreateTestObject() {
  return NULL;
}

bool TestCase::InitTestingInterface() {
  if (!GetTestingInterface()) {
    // Give a more helpful error message for the testing interface being gone
    // since that needs special enabling in Chrome.
    instance_->AppendError("This test needs the testing interface, which is "
                           "not currently available. In Chrome, use "
                           "--enable-pepper-testing when launching.");
    return false;
  }

  return true;
}

bool TestCase::EnsureRunningOverHTTP() {
  pp::Var window = instance_->GetWindowObject();
  pp::Var location = window.GetProperty("location");
  pp::Var protocol = location.GetProperty("protocol");
  if (!protocol.is_string() || protocol.AsString() != "http:") {
    instance_->AppendError("This test needs to be run over HTTP.");
    return false;
  }

  return true;
}
