// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TEST_TESTING_INSTANCE_H_
#define PPAPI_TEST_TESTING_INSTANCE_H_

#include <string>

#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"

class TestCase;

class TestingInstance : public pp::Instance {
 public:
  TestingInstance(PP_Instance instance);

  // pp::Instance override.
  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]);
  virtual void DidChangeView(const pp::Rect& position, const pp::Rect& clip);
  virtual pp::Var GetInstanceObject();

  // Outputs the information from one test run, using the format
  //   <test_name> [PASS|FAIL <error_message>]
  // If error_message is empty, we say the test passed and emit PASS. If
  // error_message is nonempty, the test failed with that message as the error
  // string.
  //
  // Intended usage:
  //   LogTest("Foo", FooTest());
  //
  // Where FooTest is defined as:
  //   std::string FooTest() {
  //     if (something_horrible_happened)
  //       return "Something horrible happened";
  //     return "";
  //   }
  void LogTest(const std::string& test_name, const std::string& error_message);

  // Appends an error message to the log.
  void AppendError(const std::string& message);

 private:
  void ExecuteTests(int32_t unused);

  // Creates a new TestCase for the give test name, or NULL if there is no such
  // test. Ownership is passed to the caller.
  TestCase* CaseForTestName(const char* name);

  // Appends a list of available tests to the console in the document.
  void LogAvailableTests();

  // Appends the given error test to the console in the document.
  void LogError(const std::string& text);

  // Appends the given HTML string to the console in the document.
  void LogHTML(const std::string& html);

  // Sets the given cookie in the current document.
  void SetCookie(const std::string& name, const std::string& value);

  pp::CompletionCallbackFactory<TestingInstance> callback_factory_;

  // Owning pointer to the current test case. Valid after Init has been called.
  TestCase* current_case_;

  // Set once the tests are run so we know not to re-run when the view is sized.
  bool executed_tests_;

  // Collects all errors to send the the browser. Empty indicates no error yet.
  std::string errors_;

  // True if running in Native Client.
  bool nacl_mode_;
};

#endif  // PPAPI_TEST_TESTING_INSTANCE_H_
