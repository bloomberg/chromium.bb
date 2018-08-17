// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/fidl/cpp/binding.h>

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"
#include "webrunner/browser/frame_impl.h"
#include "webrunner/browser/webrunner_browser_test.h"

namespace webrunner {
namespace {

using testing::_;
using testing::AllOf;
using testing::Field;

// Use a shorter name for NavigationStateChangeDetails, because it is
// referenced frequently in this file.
using NavigationDetails = chromium::web::NavigationStateChangeDetails;

const char kPage1Url[] = "/title1.html";
const char kPage2Url[] = "/title2.html";
const char kPage1Title[] = "title 1";
const char kPage2Title[] = "title 2";

MATCHER(IsSet, "Checks if an optional field is set.") {
  return !arg.is_null();
}

class ContextImplTest : public WebRunnerBrowserTest {
 public:
  ContextImplTest() = default;
  ~ContextImplTest() = default;

 protected:
  chromium::web::FramePtr CreateFrame() {
    chromium::web::FramePtr frame;
    context()->CreateFrame(frame.NewRequest());
    frame.events().OnNavigationStateChanged =
        fit::bind_member(this, &ContextImplTest::OnNavigationStateChanged);
    return frame;
  }

  MOCK_METHOD1(OnNavigationStateChanged,
               void(chromium::web::NavigationStateChangeDetails change));

 private:
  DISALLOW_COPY_AND_ASSIGN(ContextImplTest);
};

class WebContentsDeletionObserver : public content::WebContentsObserver {
 public:
  WebContentsDeletionObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  MOCK_METHOD1(RenderViewDeleted,
               void(content::RenderViewHost* render_view_host));
};

// Verifies that the browser will navigate and generate a navigation observer
// event when LoadUrl() is called.
IN_PROC_BROWSER_TEST_F(ContextImplTest, NavigateFrame) {
  chromium::web::FramePtr frame = CreateFrame();

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  base::RunLoop run_loop;
  EXPECT_CALL(*this, OnNavigationStateChanged(
                         Field(&NavigationDetails::title, url::kAboutBlankURL)))
      .WillOnce(testing::InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));

  controller->LoadUrl(url::kAboutBlankURL, nullptr);
  run_loop.Run();

  frame.Unbind();
}

IN_PROC_BROWSER_TEST_F(ContextImplTest, FrameDeletedBeforeContext) {
  chromium::web::FramePtr frame = CreateFrame();

  // Process the frame creation message.
  base::RunLoop().RunUntilIdle();

  FrameImpl* frame_impl = context_impl()->GetFrameImplForTest(&frame);
  WebContentsDeletionObserver deletion_observer(
      frame_impl->web_contents_for_test());
  base::RunLoop run_loop;
  EXPECT_CALL(deletion_observer, RenderViewDeleted(_))
      .WillOnce(testing::InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  controller->LoadUrl(url::kAboutBlankURL, nullptr);

  frame.Unbind();
  run_loop.Run();

  // Check that |context| remains bound after the frame is closed.
  EXPECT_TRUE(context());
}

IN_PROC_BROWSER_TEST_F(ContextImplTest, ContextDeletedBeforeFrame) {
  chromium::web::FramePtr frame = CreateFrame();
  EXPECT_TRUE(frame);

  base::RunLoop run_loop;
  frame.set_error_handler([&run_loop]() { run_loop.Quit(); });
  context().Unbind();
  run_loop.Run();
  EXPECT_FALSE(frame);
}

IN_PROC_BROWSER_TEST_F(ContextImplTest, GoBackAndForward) {
  chromium::web::FramePtr frame = CreateFrame();
  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title1(embedded_test_server()->GetURL(kPage1Url));
  GURL title2(embedded_test_server()->GetURL(kPage2Url));

  {
    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnNavigationStateChanged(testing::AllOf(
                           Field(&NavigationDetails::title, kPage1Title),
                           Field(&NavigationDetails::url, IsSet()))))
        .WillOnce(testing::InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));
    controller->LoadUrl(title1.spec(), nullptr);
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnNavigationStateChanged(testing::AllOf(
                           Field(&NavigationDetails::title, kPage2Title),
                           Field(&NavigationDetails::url, IsSet()))))
        .WillOnce(testing::InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));
    controller->LoadUrl(title2.spec(), nullptr);
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnNavigationStateChanged(testing::AllOf(
                           Field(&NavigationDetails::title, kPage1Title),
                           Field(&NavigationDetails::url, IsSet()))))
        .WillOnce(testing::InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));
    controller->GoBack();
    run_loop.Run();
  }

  // At the top of the navigation entry list; this should be a no-op.
  controller->GoBack();

  // Process the navigation request message.
  base::RunLoop().RunUntilIdle();

  {
    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnNavigationStateChanged(testing::AllOf(
                           Field(&NavigationDetails::title, kPage2Title),
                           Field(&NavigationDetails::url, IsSet()))))
        .WillOnce(testing::InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));
    controller->GoForward();
    run_loop.Run();
  }

  // At the end of the navigation entry list; this should be a no-op.
  controller->GoForward();

  // Process the navigation request message.
  base::RunLoop().RunUntilIdle();
}

}  // namespace
}  // namespace webrunner
