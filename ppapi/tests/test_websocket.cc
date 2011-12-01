// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_websocket.h"

#include <string.h>

#include "base/logging.h"
#include "ppapi/c/dev/ppb_websocket_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

static const char kEchoServerURL[] =
    "ws://localhost:8880/websocket/tests/hybi/echo";

REGISTER_TEST_CASE(WebSocket);

bool TestWebSocket::Init() {
  websocket_interface_ = static_cast<const PPB_WebSocket_Dev*>(
      pp::Module::Get()->GetBrowserInterface(PPB_WEBSOCKET_DEV_INTERFACE));
  var_interface_ = static_cast<const PPB_Var*>(
      pp::Module::Get()->GetBrowserInterface(PPB_VAR_INTERFACE));
  core_interface_ = static_cast<const PPB_Core*>(
      pp::Module::Get()->GetBrowserInterface(PPB_CORE_INTERFACE));
  if (!websocket_interface_ || !var_interface_ || !core_interface_)
    return false;

  return true;
}

void TestWebSocket::RunTests(const std::string& filter) {
  RUN_TEST(IsWebSocket, filter);
  RUN_TEST(InvalidConnect, filter);
  RUN_TEST(ValidConnect, filter);
  RUN_TEST(TextSendReceive, filter);
}

PP_Var TestWebSocket::CreateVar(const char* string) {
  return var_interface_->VarFromUtf8(
      pp::Module::Get()->pp_module(), string, strlen(string));
}

void TestWebSocket::ReleaseVar(const PP_Var& var) {
  var_interface_->Release(var);
}

bool TestWebSocket::AreEqual(const PP_Var& var, const char* string) {
  if (var.type != PP_VARTYPE_STRING)
    return false;
  uint32_t utf8_length;
  const char* utf8 = var_interface_->VarToUtf8(var, &utf8_length);
  uint32_t string_length = strlen(string);
  if (utf8_length != string_length)
    return false;
  if (strncmp(utf8, string, utf8_length))
    return false;
  return true;
}

PP_Resource TestWebSocket::Connect() {
  PP_Var protocols[] = { PP_MakeUndefined() };
  PP_Resource ws = websocket_interface_->Create(instance_->pp_instance());
  if (!ws)
    return 0;
  PP_Var url = CreateVar(kEchoServerURL);
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  int32_t result = websocket_interface_->Connect(
      ws, url, protocols, 0,
      static_cast<pp::CompletionCallback>(callback).pp_completion_callback());
  ReleaseVar(url);
  if (force_async_ && result != PP_OK_COMPLETIONPENDING) {
    core_interface_->ReleaseResource(ws);
    return 0;
  }
  if (callback.WaitForResult() != PP_OK) {
    core_interface_->ReleaseResource(ws);
    return 0;
  }
  return ws;
}

std::string TestWebSocket::TestIsWebSocket() {
  // Test that a NULL resource isn't a websocket.
  pp::Resource null_resource;
  PP_Bool result =
      websocket_interface_->IsWebSocket(null_resource.pp_resource());
  ASSERT_FALSE(result);

  PP_Resource ws = websocket_interface_->Create(instance_->pp_instance());
  ASSERT_TRUE(ws);

  result = websocket_interface_->IsWebSocket(ws);
  ASSERT_TRUE(result);

  core_interface_->ReleaseResource(ws);

  PASS();
}

std::string TestWebSocket::TestInvalidConnect() {
  PP_Var protocols[] = { PP_MakeUndefined() };

  PP_Resource ws = websocket_interface_->Create(instance_->pp_instance());
  ASSERT_TRUE(ws);

  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  int32_t result = websocket_interface_->Connect(
      ws, PP_MakeUndefined(), protocols, 1,
      static_cast<pp::CompletionCallback>(callback).pp_completion_callback());
  ASSERT_EQ(PP_ERROR_BADARGUMENT, result);

  result = websocket_interface_->Connect(
      ws, PP_MakeUndefined(), protocols, 1,
      static_cast<pp::CompletionCallback>(callback).pp_completion_callback());
  ASSERT_EQ(PP_ERROR_INPROGRESS, result);

  core_interface_->ReleaseResource(ws);

  const char* invalid_urls[] = {
    "http://www.google.com/invalid_scheme",
    "ws://www.google.com/invalid#fragment",
    // TODO(toyoshim): Add URL which has invalid port like
    // ws://www.google.com:65535/invalid_port
    NULL
  };
  for (int i = 0; invalid_urls[i]; ++i) {
    ws = websocket_interface_->Create(instance_->pp_instance());
    ASSERT_TRUE(ws);
    PP_Var invalid_url = CreateVar(invalid_urls[i]);
    result = websocket_interface_->Connect(
        ws, invalid_url, protocols, 0,
        static_cast<pp::CompletionCallback>(
            callback).pp_completion_callback());
    ReleaseVar(invalid_url);
    core_interface_->ReleaseResource(ws);
    ASSERT_EQ(PP_ERROR_BADARGUMENT, result);
  }

  // TODO(toyoshim): Add invalid protocols tests

  PASS();
}


std::string TestWebSocket::TestValidConnect() {
  PP_Resource ws = websocket_interface_->Create(instance_->pp_instance());
  PP_Var url = CreateVar(kEchoServerURL);
  PP_Var protocols[] = { PP_MakeUndefined() };
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  int32_t result = websocket_interface_->Connect(
      ws, url, protocols, 0,
      static_cast<pp::CompletionCallback>(callback).pp_completion_callback());
  ReleaseVar(url);
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  result = callback.WaitForResult();
  ASSERT_EQ(PP_OK, result);
  core_interface_->ReleaseResource(ws);

  PASS();
}

// TODO(toyoshim): Add tests to call various interfaces before calling connect.

std::string TestWebSocket::TestTextSendReceive() {
  // Connect to test echo server.
  PP_Resource ws = Connect();
  ASSERT_TRUE(ws);

  // Send 'hello pepper' text message.
  const char* message = "hello pepper";
  PP_Var message_var = CreateVar(message);
  int32_t result = websocket_interface_->SendMessage(ws, message_var);
  ReleaseVar(message_var);
  ASSERT_EQ(PP_OK, result);

  // Receive echoed 'hello pepper'.
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  PP_Var received_message;
  result = websocket_interface_->ReceiveMessage(ws, &received_message,
      static_cast<pp::CompletionCallback>(callback).pp_completion_callback());
  ASSERT_FALSE(result != PP_OK && result != PP_OK_COMPLETIONPENDING);
  if (result == PP_OK_COMPLETIONPENDING)
    result = callback.WaitForResult();
  ASSERT_EQ(PP_OK, result);
  ASSERT_TRUE(AreEqual(received_message, message));
  ReleaseVar(received_message);
  core_interface_->ReleaseResource(ws);

  PASS();
}

// TODO(toyoshim): Add tests for GetBufferedAmount().
// For now, the function doesn't work fine because update callback in WebKit is
// not landed yet.

// TODO(toyoshim): Add other function tests.
