// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/fidl/cpp/binding.h>

#include "base/barrier_closure.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"
#include "webrunner/common/mem_buffer_util.h"
#include "webrunner/common/named_message_port_connector.h"
#include "webrunner/common/test/run_with_timeout.h"
#include "webrunner/common/test/test_common.h"
#include "webrunner/common/test/webrunner_browser_test.h"
#include "webrunner/test/promise.h"

namespace webrunner {

// Use a shorter name for NavigationEvent, because it is
// referenced frequently in this file.
using NavigationDetails = chromium::web::NavigationEvent;

class NamedMessagePortConnectorTest
    : public webrunner::WebRunnerBrowserTest,
      public chromium::web::NavigationEventObserver {
 public:
  NamedMessagePortConnectorTest() {
    set_test_server_root(base::FilePath("webrunner/common/test/data"));
  }

  ~NamedMessagePortConnectorTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    webrunner::WebRunnerBrowserTest::SetUpOnMainThread();
    frame_ = WebRunnerBrowserTest::CreateFrame(this);
  }

  void OnNavigationStateChanged(
      chromium::web::NavigationEvent change,
      OnNavigationStateChangedCallback callback) override {
    connector_.NotifyPageLoad(frame_.get());
    if (navigate_run_loop_)
      navigate_run_loop_->Quit();
    callback();
  }

  void CheckLoadUrl(const std::string& url,
                    chromium::web::NavigationController* controller) {
    navigate_run_loop_ = std::make_unique<base::RunLoop>();
    controller->LoadUrl(url, nullptr);
    CheckRunWithTimeout(navigate_run_loop_.get());
    navigate_run_loop_.reset();
  }

  std::unique_ptr<base::RunLoop> navigate_run_loop_;
  chromium::web::FramePtr frame_;
  NamedMessagePortConnector connector_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NamedMessagePortConnectorTest);
};

IN_PROC_BROWSER_TEST_F(NamedMessagePortConnectorTest,
                       NamedMessagePortConnectorEndToEnd) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url(embedded_test_server()->GetURL("/connector.html"));

  chromium::web::NavigationControllerPtr controller;
  frame_->GetNavigationController(controller.NewRequest());

  base::RunLoop receive_port_run_loop;
  Promise<chromium::web::MessagePortPtr> message_port(
      receive_port_run_loop.QuitClosure());
  connector_.Register(
      "hello",
      base::BindRepeating(&Promise<chromium::web::MessagePortPtr>::ReceiveValue,
                          base::Unretained(&message_port)),
      frame_.get());
  CheckLoadUrl(test_url.spec(), controller.get());

  CheckRunWithTimeout(&receive_port_run_loop);

  base::RunLoop run_loop;
  chromium::web::WebMessage msg;
  msg.data = MemBufferFromString("ping");
  Promise<bool> post_result;
  (*message_port)
      ->PostMessage(std::move(msg),
                    ConvertToFitFunction(post_result.GetReceiveCallback()));

  std::vector<std::string> test_messages = {"early 1", "early 2", "ack ping"};
  for (std::string expected_msg : test_messages) {
    base::RunLoop run_loop;
    Promise<chromium::web::WebMessage> message_receiver(run_loop.QuitClosure());
    (*message_port)
        ->ReceiveMessage(
            ConvertToFitFunction(message_receiver.GetReceiveCallback()));
    CheckRunWithTimeout(&run_loop);
    EXPECT_EQ(StringFromMemBufferOrDie(message_receiver->data), expected_msg);
  }

  // Ensure that the MessagePort is dropped when navigating away.
  {
    base::RunLoop run_loop;
    (*message_port).set_error_handler([&run_loop](zx_status_t) {
      run_loop.Quit();
    });
    controller->LoadUrl("about:blank", nullptr);
    CheckRunWithTimeout(&run_loop);
  }

  connector_.Unregister(frame_.get(), "hello");
}

}  // namespace webrunner
