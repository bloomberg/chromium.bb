// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/web/cpp/fidl.h>

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "fuchsia/base/fit_adapter.h"
#include "fuchsia/base/frame_test_util.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/base/result_receiver.h"
#include "fuchsia/base/test_navigation_listener.h"
#include "fuchsia/engine/test/web_engine_browser_test.h"
#include "fuchsia/runners/cast/fake_queryable_data.h"
#include "fuchsia/runners/cast/named_message_port_connector.h"
#include "fuchsia/runners/cast/queryable_data_bindings.h"

namespace {

class QueryableDataBindingsTest : public cr_fuchsia::WebEngineBrowserTest {
 public:
  QueryableDataBindingsTest()
      : queryable_data_service_binding_(&queryable_data_service_) {
    set_test_server_root(base::FilePath("fuchsia/runners/cast/testdata"));
  }

  ~QueryableDataBindingsTest() override { connector_->Unregister("testQuery"); }

  void SetUpOnMainThread() override {
    cr_fuchsia::WebEngineBrowserTest::SetUpOnMainThread();
    base::ScopedAllowBlockingForTesting allow_blocking;
    frame_ = WebEngineBrowserTest::CreateFrame(&navigation_listener_);

    ASSERT_TRUE(embedded_test_server()->Start());
    test_url_ = embedded_test_server()->GetURL("/query_platform_value.html");

    navigation_listener_.SetBeforeAckHook(base::BindRepeating(
        &QueryableDataBindingsTest::OnBeforeAckHook, base::Unretained(this)));

    connector_ = std::make_unique<NamedMessagePortConnector>(frame_.get());
    connector_->Register(
        "testQuery",
        base::BindRepeating(&QueryableDataBindingsTest::ReceiveMessagePort,
                            base::Unretained(this)));
  }

  // Blocks test execution until the page has indicated that it's processed the
  // updates, which we achieve by setting the title to a new value and waiting
  // for the resulting navigation event.
  void SynchronizeWithPage() {
    std::string unique_title =
        base::StringPrintf("sync-%d", current_sync_id_++);
    frame_->ExecuteJavaScriptNoResult(
        {"*"},
        cr_fuchsia::MemBufferFromString(
            base::StringPrintf("document.title = '%s'", unique_title.c_str())),
        [](fuchsia::web::Frame_ExecuteJavaScriptNoResult_Result result) {
          ASSERT_TRUE(result.is_response());
        });

    navigation_listener_.RunUntilUrlAndTitleEquals(test_url_, unique_title);
  }

  // Communicates with the page to read an entry from its QueryableData store.
  std::string CallQueryPlatformValue(base::StringPiece key) {
    // Wait until the querying MessagePort is ready to use.
    if (!query_port_) {
      base::RunLoop run_loop;
      on_query_port_received_cb_ = run_loop.QuitClosure();
      run_loop.Run();
      if (!query_port_)
        return "";
    }

    // Send the request to the page.
    fuchsia::web::WebMessage message;
    message.set_data(cr_fuchsia::MemBufferFromString(key));
    query_port_->PostMessage(
        std::move(message),
        [](fuchsia::web::MessagePort_PostMessage_Result result) {
          ASSERT_TRUE(result.is_response());
        });

    // Return the response from the page.
    base::RunLoop response_loop;
    cr_fuchsia::ResultReceiver<fuchsia::web::WebMessage> response(
        response_loop.QuitClosure());
    query_port_->ReceiveMessage(
        cr_fuchsia::CallbackToFitFunction(response.GetReceiveCallback()));
    response_loop.Run();
    if (!response->has_data())
      return "";
    std::string response_string;
    if (!cr_fuchsia::StringFromMemBuffer(response->data(), &response_string))
      return "";
    return response_string;
  }

  void ReceiveMessagePort(
      fidl::InterfaceHandle<fuchsia::web::MessagePort> port) {
    query_port_ = port.Bind();
    if (on_query_port_received_cb_)
      std::move(on_query_port_received_cb_).Run();
  }

 protected:
  void OnBeforeAckHook(
      const fuchsia::web::NavigationState& change,
      fuchsia::web::NavigationEventListener::OnNavigationStateChangedCallback
          callback) {
    if (change.has_is_main_document_loaded() &&
        change.is_main_document_loaded())
      connector_->OnPageLoad();

    callback();
  }

  fuchsia::web::FramePtr frame_;

  GURL test_url_;
  std::unique_ptr<NamedMessagePortConnector> connector_;
  FakeQueryableData queryable_data_service_;
  cr_fuchsia::TestNavigationListener navigation_listener_;
  fidl::Binding<chromium::cast::QueryableData> queryable_data_service_binding_;
  base::OnceClosure on_query_port_received_cb_;
  base::OnceClosure on_navigate_cb_;
  fuchsia::web::MessagePortPtr query_port_;
  int current_sync_id_ = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(QueryableDataBindingsTest);
};

IN_PROC_BROWSER_TEST_F(QueryableDataBindingsTest, VariousTypes) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::DictionaryValue dict_value;
  dict_value.SetString("key", "val");
  queryable_data_service_.SendChanges({{"string", base::Value("foo")},
                                       {"number", base::Value(123)},
                                       {"null", base::Value()},
                                       {"dict", std::move(dict_value)}});

  QueryableDataBindings bindings(
      frame_.get(), queryable_data_service_binding_.NewBinding().Bind());

  fuchsia::web::NavigationControllerPtr controller;
  frame_->GetNavigationController(controller.NewRequest());
  frame_->SetJavaScriptLogLevel(fuchsia::web::ConsoleLogLevel::INFO);
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), test_url_.spec()));
  navigation_listener_.RunUntilUrlEquals(test_url_);

  EXPECT_EQ(CallQueryPlatformValue("string"), "\"foo\"");
  EXPECT_EQ(CallQueryPlatformValue("number"), "123");
  EXPECT_EQ(CallQueryPlatformValue("null"), "null");
  EXPECT_EQ(CallQueryPlatformValue("dict"), "{\"key\":\"val\"}");
}

IN_PROC_BROWSER_TEST_F(QueryableDataBindingsTest, NoValues) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  QueryableDataBindings bindings(
      frame_.get(), queryable_data_service_binding_.NewBinding().Bind());

  fuchsia::web::NavigationControllerPtr controller;
  frame_->GetNavigationController(controller.NewRequest());
  frame_->SetJavaScriptLogLevel(fuchsia::web::ConsoleLogLevel::INFO);
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), test_url_.spec()));
  navigation_listener_.RunUntilUrlEquals(test_url_);

  EXPECT_EQ(CallQueryPlatformValue("string"), "null");
}

IN_PROC_BROWSER_TEST_F(QueryableDataBindingsTest, AtPageRuntime) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  queryable_data_service_.SendChanges({{"key1", base::Value(1)},
                                       {"key2", base::Value(2)},
                                       {"key3", base::Value(3)}});

  QueryableDataBindings bindings(
      frame_.get(), queryable_data_service_binding_.NewBinding().Bind());

  fuchsia::web::NavigationControllerPtr controller;
  frame_->GetNavigationController(controller.NewRequest());
  frame_->SetJavaScriptLogLevel(fuchsia::web::ConsoleLogLevel::INFO);
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), test_url_.spec()));
  navigation_listener_.RunUntilUrlEquals(test_url_);

  SynchronizeWithPage();

  EXPECT_EQ(CallQueryPlatformValue("key1"), "1");
  EXPECT_EQ(CallQueryPlatformValue("key2"), "2");
  EXPECT_EQ(CallQueryPlatformValue("key3"), "3");

  queryable_data_service_.SendChanges(
      {{"key1", base::Value(10)}, {"key2", base::Value(20)}});

  SynchronizeWithPage();

  // Verify that the changes are immediately available.
  EXPECT_EQ(CallQueryPlatformValue("key1"), "10");
  EXPECT_EQ(CallQueryPlatformValue("key2"), "20");
  EXPECT_EQ(CallQueryPlatformValue("key3"), "3");
}

// Sends updates to the Frame before the Frame has created a renderer.
IN_PROC_BROWSER_TEST_F(QueryableDataBindingsTest, AtPageLoad) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  queryable_data_service_.SendChanges({{"key1", base::Value(1)},
                                       {"key2", base::Value(2)},
                                       {"key3", base::Value(3)}});

  queryable_data_service_.SendChanges(
      {{"key1", base::Value(10)}, {"key2", base::Value(20)}});

  QueryableDataBindings bindings(
      frame_.get(), queryable_data_service_binding_.NewBinding().Bind());

  fuchsia::web::NavigationControllerPtr controller;
  frame_->GetNavigationController(controller.NewRequest());
  frame_->SetJavaScriptLogLevel(fuchsia::web::ConsoleLogLevel::INFO);
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), test_url_.spec()));
  navigation_listener_.RunUntilUrlEquals(test_url_);

  SynchronizeWithPage();

  EXPECT_EQ(CallQueryPlatformValue("key1"), "10");
  EXPECT_EQ(CallQueryPlatformValue("key2"), "20");
  EXPECT_EQ(CallQueryPlatformValue("key3"), "3");
}

}  // namespace
