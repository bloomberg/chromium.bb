// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/fidl/cpp/binding.h>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "fuchsia/base/fit_adapter.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/base/result_receiver.h"
#include "fuchsia/base/test_navigation_listener.h"
#include "fuchsia/engine/test/web_engine_browser_test.h"
#include "fuchsia/runners/cast/cast_channel_bindings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

namespace {

class CastChannelBindingsTest : public cr_fuchsia::WebEngineBrowserTest,
                                public chromium::cast::CastChannel {
 public:
  CastChannelBindingsTest()
      : receiver_binding_(this),
        run_timeout_(TestTimeouts::action_timeout(),
                     base::MakeExpectedNotRunClosure(FROM_HERE)) {
    set_test_server_root(base::FilePath("fuchsia/runners/cast/testdata"));
  }

  ~CastChannelBindingsTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    cr_fuchsia::WebEngineBrowserTest::SetUpOnMainThread();
    base::ScopedAllowBlockingForTesting allow_blocking;
    frame_ = WebEngineBrowserTest::CreateFrame(&navigation_listener_);
    connector_ = std::make_unique<NamedMessagePortConnector>(frame_.get());
  }

  void Open(fidl::InterfaceHandle<fuchsia::web::MessagePort> channel,
            OpenCallback receive_next_channel_cb) override {
    connected_channel_ = channel.Bind();
    receive_next_channel_cb_ = std::move(receive_next_channel_cb);

    if (on_channel_connected_cb_)
      std::move(on_channel_connected_cb_).Run();
  }

  void WaitUntilCastChannelOpened() {
    if (!connected_channel_) {
      base::RunLoop run_loop;
      on_channel_connected_cb_ = run_loop.QuitClosure();
      run_loop.Run();
    }
    receive_next_channel_cb_();
  }

  void WaitUntilCastChannelClosed() {
    if (!connected_channel_)
      return;

    base::RunLoop run_loop;
    connected_channel_.set_error_handler(
        [quit_closure = run_loop.QuitClosure()](zx_status_t) {
          quit_closure.Run();
        });
    run_loop.Run();
  }

  std::string ReadStringFromChannel() {
    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<fuchsia::web::WebMessage> message(
        run_loop.QuitClosure());
    connected_channel_->ReceiveMessage(
        cr_fuchsia::CallbackToFitFunction(message.GetReceiveCallback()));
    run_loop.Run();

    std::string data;
    CHECK(cr_fuchsia::StringFromMemBuffer(message->data(), &data));
    return data;
  }

  void CheckLoadUrl(const GURL& url,
                    fuchsia::web::NavigationController* controller) {
    navigate_run_loop_ = std::make_unique<base::RunLoop>();
    cr_fuchsia::ResultReceiver<
        fuchsia::web::NavigationController_LoadUrl_Result>
        result;
    controller->LoadUrl(
        url.spec(), fuchsia::web::LoadUrlParams(),
        cr_fuchsia::CallbackToFitFunction(result.GetReceiveCallback()));
    navigation_listener_.RunUntilUrlEquals(url);
    EXPECT_TRUE(result->is_response());
  }

  std::unique_ptr<base::RunLoop> navigate_run_loop_;
  fuchsia::web::FramePtr frame_;
  std::unique_ptr<NamedMessagePortConnector> connector_;
  fidl::Binding<chromium::cast::CastChannel> receiver_binding_;
  cr_fuchsia::TestNavigationListener navigation_listener_;

  // The connected Cast Channel.
  fuchsia::web::MessagePortPtr connected_channel_;

  // A pending on-connect callback, to be invoked once a Cast Channel is
  // received.
  base::OnceClosure on_channel_connected_cb_;

 private:
  const base::RunLoop::ScopedRunTimeoutForTest run_timeout_;
  OpenCallback receive_next_channel_cb_;

  DISALLOW_COPY_AND_ASSIGN(CastChannelBindingsTest);
};

IN_PROC_BROWSER_TEST_F(CastChannelBindingsTest, CastChannelBufferedInput) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url(embedded_test_server()->GetURL("/cast_channel.html"));

  frame_->SetJavaScriptLogLevel(fuchsia::web::ConsoleLogLevel::INFO);
  fuchsia::web::NavigationControllerPtr controller;
  frame_->GetNavigationController(controller.NewRequest());

  CastChannelBindings cast_channel_instance(
      frame_.get(), connector_.get(), receiver_binding_.NewBinding().Bind());

  // Verify that CastChannelBindings can properly handle message, connect,
  // disconnect, and MessagePort disconnection events.
  CheckLoadUrl(test_url, controller.get());
  connector_->OnPageLoad();

  WaitUntilCastChannelOpened();

  auto expected_list = {"this", "is", "a", "test"};
  for (const std::string& expected : expected_list) {
    EXPECT_EQ(ReadStringFromChannel(), expected);
  }
}

IN_PROC_BROWSER_TEST_F(CastChannelBindingsTest, CastChannelReconnect) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url(embedded_test_server()->GetURL("/cast_channel_reconnect.html"));
  GURL empty_url(embedded_test_server()->GetURL("/defaultresponse"));

  frame_->SetJavaScriptLogLevel(fuchsia::web::ConsoleLogLevel::INFO);
  fuchsia::web::NavigationControllerPtr controller;
  frame_->GetNavigationController(controller.NewRequest());

  CastChannelBindings cast_channel_instance(
      frame_.get(), connector_.get(), receiver_binding_.NewBinding().Bind());

  // Verify that CastChannelBindings can properly handle message, connect,
  // disconnect, and MessagePort disconnection events.
  // Also verify that the cast channel is used across inter-page navigations.
  for (int i = 0; i < 5; ++i) {
    CheckLoadUrl(test_url, controller.get());
    connector_->OnPageLoad();

    WaitUntilCastChannelOpened();

    WaitUntilCastChannelClosed();

    WaitUntilCastChannelOpened();

    EXPECT_EQ("reconnected", ReadStringFromChannel());

    // Send a message to the channel.
    {
      fuchsia::web::WebMessage message;
      message.set_data(cr_fuchsia::MemBufferFromString("hello"));

      base::RunLoop run_loop;
      cr_fuchsia::ResultReceiver<fuchsia::web::MessagePort_PostMessage_Result>
          post_result(run_loop.QuitClosure());
      connected_channel_->PostMessage(
          std::move(message),
          cr_fuchsia::CallbackToFitFunction(post_result.GetReceiveCallback()));
      run_loop.Run();
      EXPECT_FALSE(post_result->is_err());
    }

    EXPECT_EQ("ack hello", ReadStringFromChannel());

    // Navigate away.
    CheckLoadUrl(empty_url, controller.get());
  }
}

}  // namespace
