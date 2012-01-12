// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_websocket.h"

#include <string.h>
#include <vector>

#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/dev/ppb_var_array_buffer_dev.h"
#include "ppapi/c/dev/ppb_websocket_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/cpp/dev/websocket_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

const char kEchoServerURL[] =
    "ws://localhost:8880/websocket/tests/hybi/echo";

const char kCloseServerURL[] =
    "ws://localhost:8880/websocket/tests/hybi/close";

const char kProtocolTestServerURL[] =
    "ws://localhost:8880/websocket/tests/hybi/protocol-test?protocol=";

const char* const kInvalidURLs[] = {
  "http://www.google.com/invalid_scheme",
  "ws://www.google.com/invalid#fragment",
  "ws://www.google.com:65535/invalid_port",
  NULL
};

// Connection close code is defined in WebSocket protocol specification.
// The magic number 1000 means gracefull closure without any error.
// See section 7.4.1. of RFC 6455.
const uint16_t kCloseCodeNormalClosure = 1000U;

// Internal packet sizes.
const uint64_t kCloseFrameSize = 6;
const uint64_t kMessageFrameOverhead = 6;

REGISTER_TEST_CASE(WebSocket);

bool TestWebSocket::Init() {
  websocket_interface_ = static_cast<const PPB_WebSocket_Dev*>(
      pp::Module::Get()->GetBrowserInterface(PPB_WEBSOCKET_DEV_INTERFACE));
  var_interface_ = static_cast<const PPB_Var*>(
      pp::Module::Get()->GetBrowserInterface(PPB_VAR_INTERFACE));
  arraybuffer_interface_ = static_cast<const PPB_VarArrayBuffer_Dev*>(
      pp::Module::Get()->GetBrowserInterface(
          PPB_VAR_ARRAY_BUFFER_DEV_INTERFACE));
  core_interface_ = static_cast<const PPB_Core*>(
      pp::Module::Get()->GetBrowserInterface(PPB_CORE_INTERFACE));
  if (!websocket_interface_ || !var_interface_ || !arraybuffer_interface_ ||
      !core_interface_)
    return false;

  return CheckTestingInterface();
}

void TestWebSocket::RunTests(const std::string& filter) {
  RUN_TEST_WITH_REFERENCE_CHECK(IsWebSocket, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(UninitializedPropertiesAccess, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(InvalidConnect, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(Protocols, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(GetURL, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(ValidConnect, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(InvalidClose, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(ValidClose, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(GetProtocol, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(TextSendReceive, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(BinarySendReceive, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(BufferedAmount, filter);

  RUN_TEST_WITH_REFERENCE_CHECK(CcInterfaces, filter);
}

PP_Var TestWebSocket::CreateVarString(const char* string) {
  return var_interface_->VarFromUtf8(string, strlen(string));
}

PP_Var TestWebSocket::CreateVarBinary(const uint8_t* data, uint32_t size) {
  PP_Var var = arraybuffer_interface_->Create(size);
  void* var_data = arraybuffer_interface_->Map(var);
  memcpy(var_data, data, size);
  return var;
}

void TestWebSocket::ReleaseVar(const PP_Var& var) {
  var_interface_->Release(var);
}

bool TestWebSocket::AreEqualWithString(const PP_Var& var, const char* string) {
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

bool TestWebSocket::AreEqualWithBinary(const PP_Var& var,
                                       const uint8_t* data,
                                       uint32_t size) {
  if (arraybuffer_interface_->ByteLength(var) != size)
    return false;
  if (memcmp(arraybuffer_interface_->Map(var), data, size))
    return false;
  return true;
}

PP_Resource TestWebSocket::Connect(
    const char* url, int32_t* result, const char* protocol) {
  PP_Var protocols[] = { PP_MakeUndefined() };
  PP_Resource ws = websocket_interface_->Create(instance_->pp_instance());
  if (!ws)
    return 0;
  PP_Var url_var = CreateVarString(url);
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  uint32_t protocol_count = 0U;
  if (protocol) {
    protocols[0] = CreateVarString(protocol);
    protocol_count = 1U;
  }
  websocket_interface_->SetBinaryType(
      ws, PP_WEBSOCKETBINARYTYPE_ARRAYBUFFER_DEV);
  *result = websocket_interface_->Connect(
      ws, url_var, protocols, protocol_count,
      static_cast<pp::CompletionCallback>(callback).pp_completion_callback());
  ReleaseVar(url_var);
  if (protocol)
    ReleaseVar(protocols[0]);
  if (*result == PP_OK_COMPLETIONPENDING)
    *result = callback.WaitForResult();
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

std::string TestWebSocket::TestUninitializedPropertiesAccess() {
  PP_Resource ws = websocket_interface_->Create(instance_->pp_instance());
  ASSERT_TRUE(ws);

  uint64_t bufferedAmount = websocket_interface_->GetBufferedAmount(ws);
  ASSERT_EQ(0U, bufferedAmount);

  uint16_t close_code = websocket_interface_->GetCloseCode(ws);
  ASSERT_EQ(0U, close_code);

  PP_Var close_reason = websocket_interface_->GetCloseReason(ws);
  ASSERT_TRUE(AreEqualWithString(close_reason, ""));
  ReleaseVar(close_reason);

  PP_Bool close_was_clean = websocket_interface_->GetCloseWasClean(ws);
  ASSERT_EQ(PP_FALSE, close_was_clean);

  PP_Var extensions = websocket_interface_->GetExtensions(ws);
  ASSERT_TRUE(AreEqualWithString(extensions, ""));
  ReleaseVar(extensions);

  PP_Var protocol = websocket_interface_->GetProtocol(ws);
  ASSERT_TRUE(AreEqualWithString(protocol, ""));
  ReleaseVar(protocol);

  PP_WebSocketReadyState_Dev ready_state =
      websocket_interface_->GetReadyState(ws);
  ASSERT_EQ(PP_WEBSOCKETREADYSTATE_INVALID_DEV, ready_state);

  PP_Var url = websocket_interface_->GetURL(ws);
  ASSERT_TRUE(AreEqualWithString(url, ""));
  ReleaseVar(url);

  core_interface_->ReleaseResource(ws);

  PASS();
}

std::string TestWebSocket::TestInvalidConnect() {
  PP_Var protocols[] = { PP_MakeUndefined() };

  PP_Resource ws = websocket_interface_->Create(instance_->pp_instance());
  ASSERT_TRUE(ws);

  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  int32_t result = websocket_interface_->Connect(
      ws, PP_MakeUndefined(), protocols, 1U,
      static_cast<pp::CompletionCallback>(callback).pp_completion_callback());
  ASSERT_EQ(PP_ERROR_BADARGUMENT, result);

  result = websocket_interface_->Connect(
      ws, PP_MakeUndefined(), protocols, 1U,
      static_cast<pp::CompletionCallback>(callback).pp_completion_callback());
  ASSERT_EQ(PP_ERROR_INPROGRESS, result);

  core_interface_->ReleaseResource(ws);

  for (int i = 0; kInvalidURLs[i]; ++i) {
    ws = Connect(kInvalidURLs[i], &result, NULL);
    ASSERT_TRUE(ws);
    ASSERT_EQ(PP_ERROR_BADARGUMENT, result);

    core_interface_->ReleaseResource(ws);
  }

  PASS();
}

std::string TestWebSocket::TestProtocols() {
  PP_Var url = CreateVarString(kEchoServerURL);
  PP_Var bad_protocols[] = {
    CreateVarString("x-test"),
    CreateVarString("x-test")
  };
  PP_Var good_protocols[] = {
    CreateVarString("x-test"),
    CreateVarString("x-yatest")
  };

  PP_Resource ws = websocket_interface_->Create(instance_->pp_instance());
  ASSERT_TRUE(ws);
  int32_t result = websocket_interface_->Connect(
      ws, url, bad_protocols, 2U, PP_BlockUntilComplete());
  ASSERT_EQ(PP_ERROR_BADARGUMENT, result);
  core_interface_->ReleaseResource(ws);

  ws = websocket_interface_->Create(instance_->pp_instance());
  ASSERT_TRUE(ws);
  result = websocket_interface_->Connect(
      ws, url, good_protocols, 2U, PP_BlockUntilComplete());
  ASSERT_EQ(PP_ERROR_BLOCKS_MAIN_THREAD, result);
  core_interface_->ReleaseResource(ws);

  ReleaseVar(url);
  for (int i = 0; i < 2; ++i) {
    ReleaseVar(bad_protocols[i]);
    ReleaseVar(good_protocols[i]);
  }
  core_interface_->ReleaseResource(ws);

  PASS();
}

std::string TestWebSocket::TestGetURL() {
  for (int i = 0; kInvalidURLs[i]; ++i) {
    int32_t result;
    PP_Resource ws = Connect(kInvalidURLs[i], &result, NULL);
    ASSERT_TRUE(ws);
    PP_Var url = websocket_interface_->GetURL(ws);
    ASSERT_TRUE(AreEqualWithString(url, kInvalidURLs[i]));
    ASSERT_EQ(PP_ERROR_BADARGUMENT, result);

    ReleaseVar(url);
    core_interface_->ReleaseResource(ws);
  }

  PASS();
}

std::string TestWebSocket::TestValidConnect() {
  int32_t result;
  PP_Resource ws = Connect(kEchoServerURL, &result, NULL);
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, result);
  core_interface_->ReleaseResource(ws);

  PASS();
}

std::string TestWebSocket::TestInvalidClose() {
  PP_Var reason = CreateVarString("close for test");
  TestCompletionCallback callback(instance_->pp_instance());

  // Close before connect.
  PP_Resource ws = websocket_interface_->Create(instance_->pp_instance());
  int32_t result = websocket_interface_->Close(
      ws, kCloseCodeNormalClosure, reason,
      static_cast<pp::CompletionCallback>(callback).pp_completion_callback());
  ASSERT_EQ(PP_ERROR_FAILED, result);
  core_interface_->ReleaseResource(ws);

  // Close with bad arguments.
  ws = Connect(kEchoServerURL, &result, NULL);
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, result);
  result = websocket_interface_->Close(ws, 1U, reason,
      static_cast<pp::CompletionCallback>(callback).pp_completion_callback());
  ASSERT_EQ(PP_ERROR_NOACCESS, result);
  core_interface_->ReleaseResource(ws);

  ReleaseVar(reason);

  PASS();
}

std::string TestWebSocket::TestValidClose() {
  PP_Var reason = CreateVarString("close for test");
  PP_Var url = CreateVarString(kEchoServerURL);
  PP_Var protocols[] = { PP_MakeUndefined() };
  TestCompletionCallback callback(instance_->pp_instance());
  TestCompletionCallback another_callback(instance_->pp_instance());

  // Close.
  int32_t result;
  PP_Resource ws = Connect(kEchoServerURL, &result, NULL);
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, result);
  result = websocket_interface_->Close(ws, kCloseCodeNormalClosure, reason,
      static_cast<pp::CompletionCallback>(callback).pp_completion_callback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  result = callback.WaitForResult();
  ASSERT_EQ(PP_OK, result);
  core_interface_->ReleaseResource(ws);

  // Close in connecting.
  // The ongoing connect failed with PP_ERROR_ABORTED, then the close is done
  // successfully.
  ws = websocket_interface_->Create(instance_->pp_instance());
  result = websocket_interface_->Connect(ws, url, protocols, 0U,
      static_cast<pp::CompletionCallback>(callback).pp_completion_callback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  result = websocket_interface_->Close(ws, kCloseCodeNormalClosure, reason,
      static_cast<pp::CompletionCallback>(
          another_callback).pp_completion_callback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  result = callback.WaitForResult();
  ASSERT_EQ(PP_ERROR_ABORTED, result);
  result = another_callback.WaitForResult();
  ASSERT_EQ(PP_OK, result);
  core_interface_->ReleaseResource(ws);

  // Close in closing.
  // The first close will be done successfully, then the second one failed with
  // with PP_ERROR_INPROGRESS immediately.
  ws = Connect(kEchoServerURL, &result, NULL);
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, result);
  result = websocket_interface_->Close(ws, kCloseCodeNormalClosure, reason,
      static_cast<pp::CompletionCallback>(callback).pp_completion_callback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  result = websocket_interface_->Close(ws, kCloseCodeNormalClosure, reason,
      static_cast<pp::CompletionCallback>(
          another_callback).pp_completion_callback());
  ASSERT_EQ(PP_ERROR_INPROGRESS, result);
  result = callback.WaitForResult();
  ASSERT_EQ(PP_OK, result);
  core_interface_->ReleaseResource(ws);

  // Close with ongoing receive message.
  ws = Connect(kEchoServerURL, &result, NULL);
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, result);
  PP_Var receive_message_var;
  result = websocket_interface_->ReceiveMessage(ws, &receive_message_var,
      static_cast<pp::CompletionCallback>(callback).pp_completion_callback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  result = websocket_interface_->Close(ws, kCloseCodeNormalClosure, reason,
      static_cast<pp::CompletionCallback>(
          another_callback).pp_completion_callback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  result = callback.WaitForResult();
  ASSERT_EQ(PP_ERROR_ABORTED, result);
  result = another_callback.WaitForResult();
  ASSERT_EQ(PP_OK, result);
  core_interface_->ReleaseResource(ws);

  ReleaseVar(reason);
  ReleaseVar(url);

  PASS();
}

std::string TestWebSocket::TestGetProtocol() {
  const char* expected_protocols[] = {
    "x-chat",
    "hoehoe",
    NULL
  };
  for (int i = 0; expected_protocols[i]; ++i) {
    std::string url(kProtocolTestServerURL);
    url += expected_protocols[i];
    int32_t result;
    PP_Resource ws = Connect(url.c_str(), &result, expected_protocols[i]);
    ASSERT_TRUE(ws);
    ASSERT_EQ(PP_OK, result);

    PP_Var protocol = websocket_interface_->GetProtocol(ws);
    ASSERT_TRUE(AreEqualWithString(protocol, expected_protocols[i]));

    ReleaseVar(protocol);
    core_interface_->ReleaseResource(ws);
  }

  PASS();
}

std::string TestWebSocket::TestTextSendReceive() {
  // Connect to test echo server.
  int32_t connect_result;
  PP_Resource ws = Connect(kEchoServerURL, &connect_result, NULL);
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, connect_result);

  // Send 'hello pepper' text message.
  const char* message = "hello pepper";
  PP_Var message_var = CreateVarString(message);
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
  ASSERT_TRUE(AreEqualWithString(received_message, message));
  ReleaseVar(received_message);
  core_interface_->ReleaseResource(ws);

  PASS();
}

std::string TestWebSocket::TestBinarySendReceive() {
  // Connect to test echo server.
  int32_t connect_result;
  PP_Resource ws = Connect(kEchoServerURL, &connect_result, NULL);
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, connect_result);

  // Send binary message.
  uint32_t len = 256;
  uint8_t data[256];
  for (uint32_t i = 0; i < len; ++i)
    data[i] = i;
  PP_Var message_var = CreateVarBinary(data, len);
  int32_t result = websocket_interface_->SendMessage(ws, message_var);
  ReleaseVar(message_var);
  ASSERT_EQ(PP_OK, result);

  // Receive echoed binary.
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  PP_Var received_message;
  result = websocket_interface_->ReceiveMessage(ws, &received_message,
      static_cast<pp::CompletionCallback>(callback).pp_completion_callback());
  ASSERT_TRUE(result == PP_OK || result == PP_OK_COMPLETIONPENDING);
  if (result == PP_OK_COMPLETIONPENDING)
    result = callback.WaitForResult();
  ASSERT_EQ(PP_OK, result);
  ASSERT_TRUE(AreEqualWithBinary(received_message, data, len));
  ReleaseVar(received_message);
  core_interface_->ReleaseResource(ws);

  PASS();
}

std::string TestWebSocket::TestBufferedAmount() {
  // Connect to test echo server.
  int32_t connect_result;
  PP_Resource ws = Connect(kEchoServerURL, &connect_result, NULL);
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, connect_result);

  // Prepare a large message that is not aligned with the internal buffer
  // sizes.
  char message[8194];
  memset(message, 'x', 8193);
  message[8193] = 0;
  PP_Var message_var = CreateVarString(message);

  uint64_t buffered_amount = 0;
  int32_t result;
  for (int i = 0; i < 100; i++) {
    result = websocket_interface_->SendMessage(ws, message_var);
    ASSERT_EQ(PP_OK, result);
    buffered_amount = websocket_interface_->GetBufferedAmount(ws);
    // Buffered amount size 262144 is too big for the internal buffer size.
    if (buffered_amount > 262144)
      break;
  }

  // Close connection.
  std::string reason_str = "close while busy";
  PP_Var reason = CreateVarString(reason_str.c_str());
  TestCompletionCallback callback(instance_->pp_instance());
  result = websocket_interface_->Close(ws, kCloseCodeNormalClosure, reason,
      static_cast<pp::CompletionCallback>(callback).pp_completion_callback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  ASSERT_EQ(PP_WEBSOCKETREADYSTATE_CLOSING_DEV,
      websocket_interface_->GetReadyState(ws));

  result = callback.WaitForResult();
  ASSERT_EQ(PP_OK, result);
  ASSERT_EQ(PP_WEBSOCKETREADYSTATE_CLOSED_DEV,
      websocket_interface_->GetReadyState(ws));

  uint64_t base_buffered_amount = websocket_interface_->GetBufferedAmount(ws);

  // After connection closure, all sending requests fail and just increase
  // the bufferedAmount property.
  PP_Var empty_string = CreateVarString("");
  result = websocket_interface_->SendMessage(ws, empty_string);
  ASSERT_EQ(PP_ERROR_FAILED, result);
  buffered_amount = websocket_interface_->GetBufferedAmount(ws);
  ASSERT_EQ(base_buffered_amount + kMessageFrameOverhead, buffered_amount);
  base_buffered_amount = buffered_amount;

  result = websocket_interface_->SendMessage(ws, reason);
  ASSERT_EQ(PP_ERROR_FAILED, result);
  buffered_amount = websocket_interface_->GetBufferedAmount(ws);
  uint64_t reason_frame_size = kMessageFrameOverhead + reason_str.length();
  ASSERT_EQ(base_buffered_amount + reason_frame_size, buffered_amount);

  ReleaseVar(message_var);
  ReleaseVar(reason);
  ReleaseVar(empty_string);
  core_interface_->ReleaseResource(ws);

  PASS();
}

std::string TestWebSocket::TestCcInterfaces() {
  // C++ bindings is simple straightforward, then just verifies interfaces work
  // as a interface bridge fine.
  pp::WebSocket_Dev ws(instance_);

  // Check uninitialized properties access.
  ASSERT_EQ(0, ws.GetBufferedAmount());
  ASSERT_EQ(0, ws.GetCloseCode());
  ASSERT_TRUE(AreEqualWithString(ws.GetCloseReason().pp_var(), ""));
  ASSERT_EQ(false, ws.GetCloseWasClean());
  ASSERT_TRUE(AreEqualWithString(ws.GetExtensions().pp_var(), ""));
  ASSERT_TRUE(AreEqualWithString(ws.GetProtocol().pp_var(), ""));
  ASSERT_EQ(PP_WEBSOCKETREADYSTATE_INVALID_DEV, ws.GetReadyState());
  ASSERT_TRUE(AreEqualWithString(ws.GetURL().pp_var(), ""));

  // Check communication interfaces (connect, send, receive, and close).
  TestCompletionCallback connect_callback(instance_->pp_instance());
  int32_t result = ws.Connect(pp::Var(std::string(kCloseServerURL)), NULL, 0U,
      connect_callback);
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  result = connect_callback.WaitForResult();
  ASSERT_EQ(PP_OK, result);

  std::string text_message("hello C++");
  result = ws.SendMessage(pp::Var(text_message));
  ASSERT_EQ(PP_OK, result);

  uint32_t binary_length = 256;
  uint8_t binary_message[256];
  for (uint32_t i = 0; i < binary_length; ++i)
    binary_message[i] = i;
  result = ws.SendMessage(pp::Var(
      pp::Var::PassRef(), CreateVarBinary(binary_message, binary_length)));
  ASSERT_EQ(PP_OK, result);

  pp::Var text_receive_var;
  TestCompletionCallback text_receive_callback(instance_->pp_instance());
  result = ws.ReceiveMessage(&text_receive_var, text_receive_callback);
  if (result == PP_OK_COMPLETIONPENDING)
    result = text_receive_callback.WaitForResult();
  ASSERT_EQ(PP_OK, result);
  ASSERT_TRUE(
      AreEqualWithString(text_receive_var.pp_var(), text_message.c_str()));

  pp::Var binary_receive_var;
  TestCompletionCallback binary_receive_callback(instance_->pp_instance());
  result = ws.ReceiveMessage(&binary_receive_var, binary_receive_callback);
  if (result == PP_OK_COMPLETIONPENDING)
    result = binary_receive_callback.WaitForResult();
  ASSERT_EQ(PP_OK, result);
  ASSERT_TRUE(AreEqualWithBinary(
      binary_receive_var.pp_var(), binary_message, binary_length));

  TestCompletionCallback close_callback(instance_->pp_instance());
  std::string reason("bye");
  result = ws.Close(kCloseCodeNormalClosure, pp::Var(reason), close_callback);
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  result = close_callback.WaitForResult();
  ASSERT_EQ(PP_OK, result);

  // Check initialized properties access.
  ASSERT_EQ(0, ws.GetBufferedAmount());
  ASSERT_EQ(kCloseCodeNormalClosure, ws.GetCloseCode());
  ASSERT_TRUE(AreEqualWithString(ws.GetCloseReason().pp_var(), reason.c_str()));
  ASSERT_EQ(true, ws.GetCloseWasClean());
  ASSERT_TRUE(AreEqualWithString(ws.GetExtensions().pp_var(), ""));
  ASSERT_TRUE(AreEqualWithString(ws.GetProtocol().pp_var(), ""));
  ASSERT_EQ(PP_WEBSOCKETREADYSTATE_CLOSED_DEV, ws.GetReadyState());
  ASSERT_TRUE(AreEqualWithString(ws.GetURL().pp_var(), kCloseServerURL));

  PASS();
}
