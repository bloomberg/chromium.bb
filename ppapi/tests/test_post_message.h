// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_POST_MESSAGE_H_
#define PPAPI_TESTS_TEST_POST_MESSAGE_H_

#include <string>
#include <vector>

#include "ppapi/tests/test_case.h"

class TestPostMessage : public TestCase {
 public:
  explicit TestPostMessage(TestingInstance* instance) : TestCase(instance) {}

 private:
  // TestCase implementation.
  virtual bool Init();
  virtual void RunTest();

  // A handler for JS->Native calls to postMessage.  Simply pushes
  // the given value to the back of message_data_
  virtual void HandleMessage(const pp::Var& message_data);

  // Add a listener for message events which will echo back the given
  // JavaScript expression by passing it to postMessage. JavaScript Variables
  // available to the expression are:
  //  'plugin' - the DOM element for the test plugin.
  //  'message_event' - the message event parameter to the listener function.
  // This also adds the new listener to an array called 'eventListeners' on the
  // plugin's DOM element. This is used by ClearListeners().
  // Returns true on success, false on failure.
  bool AddEchoingListener(const std::string& expression);

  // Clear any listeners that have been added using AddEchoingListener by
  // calling removeEventListener for each.
  // Returns true on success, false on failure.
  bool ClearListeners();

  // Test some basic functionality;  make sure we can send data successfully
  // in both directions.
  std::string TestSendingData();

  // Test the MessageEvent object that JavaScript received to make sure it is
  // of the right type and has all the expected fields.
  std::string TestMessageEvent();

  // Test sending a message when no handler exists, make sure nothing happens.
  std::string TestNoHandler();

  // Test sending from JavaScript to the plugin with extra parameters, make sure
  // nothing happens.
  std::string TestExtraParam();

  // Test sending messages off of the main thread.
  std::string TestNonMainThread();

  typedef std::vector<pp::Var> VarVector;

  // This is used to store pp::Var objects we receive via a call to
  // HandleMessage.
  VarVector message_data_;
};

#endif  // PPAPI_TESTS_TEST_POST_MESSAGE_H_

