// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
  virtual ~TestInstance();

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

  void set_string(const std::string& s) { string_ = s; }

  // Leak a reference to the given var, but ignore the leak in the leak checking
  // that happens at shutdown. This allows us to test the "ForceFree" that
  // happens on instance shutdown.
  void LeakReferenceAndIgnore(const pp::Var& leaked);

 protected:
  // Test case protected overrides.
  virtual pp::deprecated::ScriptableObject* CreateTestObject();

 private:
  std::string TestExecuteScript();
  std::string TestRecursiveObjects();
  std::string TestLeakedObjectDestructors();
  std::string TestSetupExecuteScriptAtInstanceShutdown();
  std::string TestExecuteScriptAtInstanceShutdown();

  // Value written by set_string which is called by the ScriptableObject. This
  // allows us to keep track of what was called.
  std::string string_;
};

#endif  // PPAPI_TESTS_TEST_INSTANCE_H_
