// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/web_view_impl.h"

#include <list>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"
#include "chrome/test/chromedriver/chrome/page_load_strategy.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"
#include "chrome/test/chromedriver/net/sync_websocket_factory.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeDevToolsClient : public DevToolsClient {
 public:
  FakeDevToolsClient() : id_("fake-id"), status_(kOk) {}
  ~FakeDevToolsClient() override {}

  void set_status(const Status& status) {
    status_ = status;
  }
  void set_result(const base::DictionaryValue& result) {
    result_.Clear();
    result_.MergeDictionary(&result);
  }

  // Overridden from DevToolsClient:
  const std::string& GetId() override { return id_; }
  bool WasCrashed() override { return false; }
  Status ConnectIfNecessary() override { return Status(kOk); }
  Status SendCommand(
      const std::string& method,
      const base::DictionaryValue& params) override {
    return SendCommandAndGetResult(method, params, nullptr);
  }
  Status SendCommandFromWebSocket(const std::string& method,
                                  const base::DictionaryValue& params,
                                  const int client_command_id) override {
    return SendCommandAndGetResult(method, params, nullptr);
  }
  Status SendCommandWithTimeout(
      const std::string& method,
      const base::DictionaryValue& params,
      const Timeout* timeout) override {
    return SendCommandAndGetResult(method, params, nullptr);
  }
  Status SendAsyncCommand(
      const std::string& method,
      const base::DictionaryValue& params) override {
    return SendCommandAndGetResult(method, params, nullptr);
  }
  Status SendCommandAndGetResult(
      const std::string& method,
      const base::DictionaryValue& params,
      std::unique_ptr<base::DictionaryValue>* result) override {
    if (status_.IsError())
      return status_;
    result->reset(result_.DeepCopy());
    return Status(kOk);
  }
  Status SendCommandAndGetResultWithTimeout(
      const std::string& method,
      const base::DictionaryValue& params,
      const Timeout* timeout,
      std::unique_ptr<base::DictionaryValue>* result) override {
    return SendCommandAndGetResult(method, params, result);
  }
  Status SendCommandAndIgnoreResponse(
      const std::string& method,
      const base::DictionaryValue& params) override {
    return SendCommandAndGetResult(method, params, nullptr);
  }
  void AddListener(DevToolsEventListener* listener) override {}
  Status HandleEventsUntil(const ConditionalFunc& conditional_func,
                           const Timeout& timeout) override {
    return Status(kOk);
  }
  Status HandleReceivedEvents() override { return Status(kOk); }
  void SetDetached() override {}
  void SetOwner(WebViewImpl* owner) override {}

 private:
  const std::string id_;
  Status status_;
  base::DictionaryValue result_;
};

void AssertEvalFails(const base::DictionaryValue& command_result) {
  std::unique_ptr<base::DictionaryValue> result;
  FakeDevToolsClient client;
  client.set_result(command_result);
  Status status = internal::EvaluateScript(
      &client, 0, std::string(), internal::ReturnByValue,
      base::TimeDelta::Max(), false, &result);
  ASSERT_EQ(kUnknownError, status.code());
  ASSERT_FALSE(result);
}

}  // namespace

TEST(EvaluateScript, CommandError) {
  std::unique_ptr<base::DictionaryValue> result;
  FakeDevToolsClient client;
  client.set_status(Status(kUnknownError));
  Status status = internal::EvaluateScript(
      &client, 0, std::string(), internal::ReturnByValue,
      base::TimeDelta::Max(), false, &result);
  ASSERT_EQ(kUnknownError, status.code());
  ASSERT_FALSE(result);
}

TEST(EvaluateScript, MissingResult) {
  base::DictionaryValue dict;
  ASSERT_NO_FATAL_FAILURE(AssertEvalFails(dict));
}

TEST(EvaluateScript, Throws) {
  base::DictionaryValue dict;
  dict.SetString("exceptionDetails.exception.className", "SyntaxError");
  dict.SetString("result.type", "object");
  ASSERT_NO_FATAL_FAILURE(AssertEvalFails(dict));
}

TEST(EvaluateScript, Ok) {
  std::unique_ptr<base::DictionaryValue> result;
  base::DictionaryValue dict;
  dict.SetInteger("result.key", 100);
  FakeDevToolsClient client;
  client.set_result(dict);
  ASSERT_TRUE(internal::EvaluateScript(&client, 0, std::string(),
                                       internal::ReturnByValue,
                                       base::TimeDelta::Max(), false, &result)
                  .IsOk());
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->HasKey("key"));
}

TEST(EvaluateScriptAndGetValue, MissingType) {
  std::unique_ptr<base::Value> result;
  FakeDevToolsClient client;
  base::DictionaryValue dict;
  dict.SetInteger("result.value", 1);
  client.set_result(dict);
  ASSERT_TRUE(internal::EvaluateScriptAndGetValue(&client, 0, std::string(),
                                                  base::TimeDelta::Max(), false,
                                                  &result)
                  .IsError());
}

TEST(EvaluateScriptAndGetValue, Undefined) {
  std::unique_ptr<base::Value> result;
  FakeDevToolsClient client;
  base::DictionaryValue dict;
  dict.SetString("result.type", "undefined");
  client.set_result(dict);
  Status status = internal::EvaluateScriptAndGetValue(
      &client, 0, std::string(), base::TimeDelta::Max(), false, &result);
  ASSERT_EQ(kOk, status.code());
  ASSERT_TRUE(result && result->is_none());
}

TEST(EvaluateScriptAndGetValue, Ok) {
  std::unique_ptr<base::Value> result;
  FakeDevToolsClient client;
  base::DictionaryValue dict;
  dict.SetString("result.type", "integer");
  dict.SetInteger("result.value", 1);
  client.set_result(dict);
  Status status = internal::EvaluateScriptAndGetValue(
      &client, 0, std::string(), base::TimeDelta::Max(), false, &result);
  ASSERT_EQ(kOk, status.code());
  int value;
  ASSERT_TRUE(result && result->GetAsInteger(&value));
  ASSERT_EQ(1, value);
}

TEST(EvaluateScriptAndGetObject, NoObject) {
  FakeDevToolsClient client;
  base::DictionaryValue dict;
  dict.SetString("result.type", "integer");
  client.set_result(dict);
  bool got_object;
  std::string object_id;
  ASSERT_TRUE(internal::EvaluateScriptAndGetObject(
                  &client, 0, std::string(), base::TimeDelta::Max(), false,
                  &got_object, &object_id)
                  .IsOk());
  ASSERT_FALSE(got_object);
  ASSERT_TRUE(object_id.empty());
}

TEST(EvaluateScriptAndGetObject, Ok) {
  FakeDevToolsClient client;
  base::DictionaryValue dict;
  dict.SetString("result.objectId", "id");
  client.set_result(dict);
  bool got_object;
  std::string object_id;
  ASSERT_TRUE(internal::EvaluateScriptAndGetObject(
                  &client, 0, std::string(), base::TimeDelta::Max(), false,
                  &got_object, &object_id)
                  .IsOk());
  ASSERT_TRUE(got_object);
  ASSERT_STREQ("id", object_id.c_str());
}

TEST(ParseCallFunctionResult, NotDict) {
  std::unique_ptr<base::Value> result;
  base::Value value(1);
  ASSERT_NE(kOk, internal::ParseCallFunctionResult(value, &result).code());
}

TEST(ParseCallFunctionResult, Ok) {
  std::unique_ptr<base::Value> result;
  base::DictionaryValue dict;
  dict.SetInteger("status", 0);
  dict.SetInteger("value", 1);
  Status status = internal::ParseCallFunctionResult(dict, &result);
  ASSERT_EQ(kOk, status.code());
  int value;
  ASSERT_TRUE(result && result->GetAsInteger(&value));
  ASSERT_EQ(1, value);
}

TEST(ParseCallFunctionResult, ScriptError) {
  std::unique_ptr<base::Value> result;
  base::DictionaryValue dict;
  dict.SetInteger("status", 1);
  dict.SetInteger("value", 1);
  Status status = internal::ParseCallFunctionResult(dict, &result);
  ASSERT_EQ(1, status.code());
  ASSERT_FALSE(result);
}

namespace {

class MockSyncWebSocket : public SyncWebSocket {
 public:
  explicit MockSyncWebSocket(SyncWebSocket::StatusCode next_status)
      : connected_(true),
        id_(-1),
        queued_messages_(1),
        next_status_(next_status) {}
  ~MockSyncWebSocket() override {}

  bool IsConnected() override { return connected_; }

  bool Connect(const GURL& url) override {
    EXPECT_STREQ("http://url/", url.possibly_invalid_spec().c_str());
    connected_ = true;
    return true;
  }

  bool Send(const std::string& message) override { return false; }

  SyncWebSocket::StatusCode ReceiveNextMessage(
      std::string* message,
      const Timeout& timeout) override {
    return next_status_;
  }

  bool HasNextMessage() override { return queued_messages_ > 0; }

 protected:
  bool connected_;
  int id_;
  int queued_messages_;
  SyncWebSocket::StatusCode next_status_;
};

std::unique_ptr<SyncWebSocket> CreateMockSyncWebSocket(
    SyncWebSocket::StatusCode next_status) {
  return std::make_unique<MockSyncWebSocket>(next_status);
}

}  // namespace

TEST(CreateChild, MultiLevel) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket, SyncWebSocket::kOk);
  // CreateChild relies on client_ being a DevToolsClientImpl, so no mocking
  std::unique_ptr<DevToolsClientImpl> client_uptr =
      std::make_unique<DevToolsClientImpl>(factory, "http://url", "id");
  DevToolsClientImpl* client_ptr = client_uptr.get();
  BrowserInfo browser_info;
  WebViewImpl level1(client_ptr->GetId(), true, nullptr, &browser_info,
                     std::move(client_uptr), nullptr, PageLoadStrategy::kEager);
  std::string sessionid = "2";
  std::unique_ptr<WebViewImpl> level2 =
      std::unique_ptr<WebViewImpl>(level1.CreateChild(sessionid, "1234"));
  sessionid = "3";
  std::unique_ptr<WebViewImpl> level3 =
      std::unique_ptr<WebViewImpl>(level2->CreateChild(sessionid, "3456"));
  sessionid = "4";
  std::unique_ptr<WebViewImpl> level4 =
      std::unique_ptr<WebViewImpl>(level3->CreateChild(sessionid, "5678"));
}

TEST(CreateChild, IsNonBlocking_NoErrors) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket, SyncWebSocket::kOk);
  // CreateChild relies on client_ being a DevToolsClientImpl, so no mocking
  std::unique_ptr<DevToolsClientImpl> client_uptr =
      std::make_unique<DevToolsClientImpl>(factory, "http://url", "id");
  DevToolsClientImpl* client_ptr = client_uptr.get();
  BrowserInfo browser_info;
  WebViewImpl parent_view(client_ptr->GetId(), true, nullptr, &browser_info,
                          std::move(client_uptr), nullptr,
                          PageLoadStrategy::kEager);
  ASSERT_FALSE(parent_view.IsNonBlocking());

  std::string sessionid = "2";
  std::unique_ptr<WebViewImpl> child_view =
      std::unique_ptr<WebViewImpl>(parent_view.CreateChild(sessionid, "1234"));
  ASSERT_NO_FATAL_FAILURE(child_view->IsNonBlocking());
  ASSERT_FALSE(child_view->IsNonBlocking());
}

TEST(CreateChild, Load_NoErrors) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket, SyncWebSocket::kOk);
  // CreateChild relies on client_ being a DevToolsClientImpl, so no mocking
  std::unique_ptr<DevToolsClientImpl> client_uptr =
      std::make_unique<DevToolsClientImpl>(factory, "http://url", "id");
  DevToolsClientImpl* client_ptr = client_uptr.get();
  BrowserInfo browser_info;
  WebViewImpl parent_view(client_ptr->GetId(), true, nullptr, &browser_info,
                          std::move(client_uptr), nullptr,
                          PageLoadStrategy::kNone);
  std::string sessionid = "2";
  std::unique_ptr<WebViewImpl> child_view =
      std::unique_ptr<WebViewImpl>(parent_view.CreateChild(sessionid, "1234"));

  ASSERT_NO_FATAL_FAILURE(child_view->Load("chrome://version", nullptr));
}

TEST(CreateChild, WaitForPendingNavigations_NoErrors) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket, SyncWebSocket::kTimeout);
  // CreateChild relies on client_ being a DevToolsClientImpl, so no mocking
  std::unique_ptr<DevToolsClientImpl> client_uptr =
      std::make_unique<DevToolsClientImpl>(factory, "http://url", "id");
  DevToolsClientImpl* client_ptr = client_uptr.get();
  BrowserInfo browser_info;
  WebViewImpl parent_view(client_ptr->GetId(), true, nullptr, &browser_info,
                          std::move(client_uptr), nullptr,
                          PageLoadStrategy::kNone);
  std::string sessionid = "2";
  std::unique_ptr<WebViewImpl> child_view =
      std::unique_ptr<WebViewImpl>(parent_view.CreateChild(sessionid, "1234"));

  // child_view gets no socket...
  ASSERT_NO_FATAL_FAILURE(child_view->WaitForPendingNavigations(
      "1234", Timeout(base::TimeDelta::FromMilliseconds(10)), true));
}

TEST(CreateChild, IsPendingNavigation_NoErrors) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket, SyncWebSocket::kOk);
  // CreateChild relies on client_ being a DevToolsClientImpl, so no mocking
  std::unique_ptr<DevToolsClientImpl> client_uptr =
      std::make_unique<DevToolsClientImpl>(factory, "http://url", "id");
  DevToolsClientImpl* client_ptr = client_uptr.get();
  BrowserInfo browser_info;
  WebViewImpl parent_view(client_ptr->GetId(), true, nullptr, &browser_info,
                          std::move(client_uptr), nullptr,
                          PageLoadStrategy::kNormal);
  std::string sessionid = "2";
  std::unique_ptr<WebViewImpl> child_view =
      std::unique_ptr<WebViewImpl>(parent_view.CreateChild(sessionid, "1234"));

  Timeout timeout(base::TimeDelta::FromMilliseconds(10));
  bool result;
  ASSERT_NO_FATAL_FAILURE(
      child_view->IsPendingNavigation("1234", &timeout, &result));
}

TEST(ManageCookies, AddCookie_SameSiteTrue) {
  std::unique_ptr<FakeDevToolsClient> client_uptr =
      std::make_unique<FakeDevToolsClient>();
  FakeDevToolsClient* client_ptr = client_uptr.get();
  BrowserInfo browser_info;
  WebViewImpl view(client_ptr->GetId(), true, nullptr, &browser_info,
                   std::move(client_uptr), nullptr, PageLoadStrategy::kEager);
  std::string samesite = "Strict";
  base::DictionaryValue dict;
  dict.SetBoolean("success", true);
  client_ptr->set_result(dict);
  Status status = view.AddCookie("utest", "chrome://version", "value", "domain",
                                 "path", samesite, true, true, 123456789);
  ASSERT_EQ(kOk, status.code());
}
