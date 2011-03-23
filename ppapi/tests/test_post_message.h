// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_POST_MESSAGE_H_
#define PPAPI_TESTS_TEST_POST_MESSAGE_H_

#include <string>
#include <vector>

#include "ppapi/tests/test_case.h"

struct PPB_Testing_Dev;

class TestPostMessage : public TestCase {
 public:
  explicit TestPostMessage(TestingInstance* instance)
      : TestCase(instance), testing_interface_(NULL) {}

 private:
  // TestCase implementation.
  virtual bool Init();
  virtual void RunTest();

  // A handler for JS->Native calls to postMessage.  Simply pushes
  // the given value to the back of message_data_
  virtual void HandleMessage(const pp::Var& message_data);

  // Set the JavaScript onmessage handler to echo back some expression based on
  // the message_event by passing it to postMessage.  Returns true on success,
  // false on failure.
  bool MakeOnMessageEcho(const std::string& expression);

  // Test some basic functionality;  make sure we can send data successfully
  // in both directions.
  std::string TestSendingData();

  // Test the MessageEvent object that JavaScript received to make sure it is
  // of the right type and has all the expected fields.
  std::string TestMessageEvent();

  // Test sending a message when no handler exists, make sure nothing happens.
  std::string TestNoHandler();

  const PPB_Testing_Dev* testing_interface_;

  // This is used to store pp::Var objects we receive via a call to
  // HandleMessage.
  std::vector<pp::Var> message_data_;
};

#endif  // PPAPI_TESTS_TEST_POST_MESSAGE_H_

