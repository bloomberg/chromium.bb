// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_WEBSOCKET_H_
#define PAPPI_TESTS_TEST_WEBSOCKET_H_

#include <string>

#include "ppapi/c/dev/ppb_var_array_buffer_dev.h"
#include "ppapi/c/dev/ppb_websocket_dev.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/tests/test_case.h"

class TestWebSocket : public TestCase {
 public:
  explicit TestWebSocket(TestingInstance* instance) : TestCase(instance) {}

 private:
  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

  PP_Var CreateVarString(const char* string);
  PP_Var CreateVarBinary(const uint8_t* data, uint32_t size);
  void ReleaseVar(const PP_Var& var);
  bool AreEqualWithString(const PP_Var& var, const char* string);
  bool AreEqualWithBinary(const PP_Var& var,
                          const uint8_t* data,
                          uint32_t size);

  PP_Resource Connect(const char* url, int32_t* result, const char* protocol);

  std::string TestIsWebSocket();
  std::string TestUninitializedPropertiesAccess();
  std::string TestInvalidConnect();
  std::string TestProtocols();
  std::string TestGetURL();
  std::string TestValidConnect();
  std::string TestInvalidClose();
  std::string TestValidClose();
  std::string TestGetProtocol();
  std::string TestTextSendReceive();
  std::string TestBinarySendReceive();
  std::string TestBufferedAmount();

  std::string TestCcInterfaces();

  // Used by the tests that access the C API directly.
  const PPB_WebSocket_Dev* websocket_interface_;
  const PPB_Var* var_interface_;
  const PPB_VarArrayBuffer_Dev* arraybuffer_interface_;
  const PPB_Core* core_interface_;
};

#endif  // PAPPI_TESTS_TEST_WEBSOCKET_H_
