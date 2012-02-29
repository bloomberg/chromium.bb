// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_INSTANCE_H_
#define PPAPI_TESTS_TEST_INSTANCE_H_

#include <string>
#include <vector>

#include "ppapi/tests/test_case.h"

class TestInstance : public TestCase {
 public:
  TestInstance(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

  void set_string(const std::string& s) { string_ = s; }

 protected:
  // Test case protected overrides.
  virtual pp::deprecated::ScriptableObject* CreateTestObject();

 private:
  std::string TestExecuteScript();

  // Value written by set_string which is called by the ScriptableObject. This
  // allows us to keep track of what was called.
  std::string string_;
};

#endif  // PPAPI_TESTS_TEST_INSTANCE_H_
