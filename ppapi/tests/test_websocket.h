// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_WEBSOCKET_H_
#define PAPPI_TESTS_TEST_WEBSOCKET_H_

#include <string>

#include "ppapi/tests/test_case.h"

struct PPB_WebSocket_Dev;

class TestWebSocket : public TestCase {
 public:
  explicit TestWebSocket(TestingInstance* instance) : TestCase(instance) {}

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestCreate();
  std::string TestIsWebSocket();

  // Used by the tests that access the C API directly.
  const PPB_WebSocket_Dev* websocket_interface_;
};

#endif  // PAPPI_TESTS_TEST_WEBSOCKET_H_
