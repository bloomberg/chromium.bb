// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_WEBSOCKET_H_
#define PAPPI_TESTS_TEST_WEBSOCKET_H_

#include <string>

#include "ppapi/tests/test_case.h"

struct PPB_Core;
struct PPB_Var;
struct PPB_WebSocket_Dev;

class TestWebSocket : public TestCase {
 public:
  explicit TestWebSocket(TestingInstance* instance) : TestCase(instance) {}

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  PP_Var CreateVar(const char* string);
  void ReleaseVar(const PP_Var& var);
  bool AreEqual(const PP_Var& var, const char* string);

  PP_Resource Connect();

  std::string TestIsWebSocket();
  std::string TestInvalidConnect();
  std::string TestValidConnect();
  std::string TestTextSendReceive();

  // Used by the tests that access the C API directly.
  const PPB_WebSocket_Dev* websocket_interface_;
  const PPB_Var* var_interface_;
  const PPB_Core* core_interface_;
};

#endif  // PAPPI_TESTS_TEST_WEBSOCKET_H_
