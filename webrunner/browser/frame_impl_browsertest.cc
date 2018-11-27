// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/fidl/cpp/binding.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/macros.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/url_request/url_request_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"
#include "webrunner/browser/frame_impl.h"
#include "webrunner/browser/run_with_timeout.h"
#include "webrunner/browser/test_common.h"
#include "webrunner/browser/webrunner_browser_test.h"
#include "webrunner/service/common.h"

namespace webrunner {

using testing::_;
using testing::AllOf;
using testing::Field;
using testing::InvokeWithoutArgs;
using testing::Mock;

// Use a shorter name for NavigationEvent, because it is
// referenced frequently in this file.
using NavigationDetails = chromium::web::NavigationEvent;

const char kPage1Path[] = "/title1.html";
const char kPage2Path[] = "/title2.html";
const char kDynamicTitlePath[] = "/dynamic_title.html";
const char kPage1Title[] = "title 1";
const char kPage2Title[] = "title 2";
const char kDataUrl[] =
    "data:text/html;base64,PGI+SGVsbG8sIHdvcmxkLi4uPC9iPg==";

MATCHER(IsSet, "Checks if an optional field is set.") {
  return !arg.is_null();
}

// Defines a suite of tests that exercise Frame-level functionality, such as
// navigation commands and page events.
class FrameImplTest : public WebRunnerBrowserTest {
 public:
  FrameImplTest() = default;
  ~FrameImplTest() = default;

  MOCK_METHOD1(OnServeHttpRequest,
               void(const net::test_server::HttpRequest& request));

 protected:
  // Creates a Frame with |navigation_observer_| attached.
  chromium::web::FramePtr CreateFrame() {
    return WebRunnerBrowserTest::CreateFrame(&navigation_observer_);
  }

  // Navigates a |controller| to |url|, blocking until navigation is complete.
  void CheckLoadUrl(const std::string& url,
                    const std::string& expected_title,
                    chromium::web::NavigationController* controller) {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_,
                MockableOnNavigationStateChanged(testing::AllOf(
                    Field(&NavigationDetails::title, expected_title),
                    Field(&NavigationDetails::url, url))))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->LoadUrl(url, nullptr);
    CheckRunWithTimeout(&run_loop);
    Mock::VerifyAndClearExpectations(this);
    navigation_observer_.Acknowledge();
  }

  testing::StrictMock<MockNavigationObserver> navigation_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameImplTest);
};

class WebContentsDeletionObserver : public content::WebContentsObserver {
 public:
  explicit WebContentsDeletionObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  MOCK_METHOD1(RenderViewDeleted,
               void(content::RenderViewHost* render_view_host));
};

// Verifies that the browser will navigate and generate a navigation observer
// event when LoadUrl() is called.
IN_PROC_BROWSER_TEST_F(FrameImplTest, NavigateFrame) {
  chromium::web::FramePtr frame = CreateFrame();

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  CheckLoadUrl(url::kAboutBlankURL, url::kAboutBlankURL, controller.get());

  frame.Unbind();
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, NavigateDataFrame) {
  chromium::web::FramePtr frame = CreateFrame();

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  CheckLoadUrl(kDataUrl, kDataUrl, controller.get());

  frame.Unbind();
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, FrameDeletedBeforeContext) {
  chromium::web::FramePtr frame = CreateFrame();

  // Process the frame creation message.
  base::RunLoop().RunUntilIdle();

  FrameImpl* frame_impl = context_impl()->GetFrameImplForTest(&frame);
  WebContentsDeletionObserver deletion_observer(
      frame_impl->web_contents_for_test());
  base::RunLoop run_loop;
  EXPECT_CALL(deletion_observer, RenderViewDeleted(_))
      .WillOnce(InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  controller->LoadUrl(url::kAboutBlankURL, nullptr);

  frame.Unbind();
  run_loop.Run();

  // Check that |context| remains bound after the frame is closed.
  EXPECT_TRUE(context());
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, ContextDeletedBeforeFrame) {
  chromium::web::FramePtr frame = CreateFrame();
  EXPECT_TRUE(frame);

  base::RunLoop run_loop;
  frame.set_error_handler([&run_loop](zx_status_t status) { run_loop.Quit(); });
  context().Unbind();
  run_loop.Run();
  EXPECT_FALSE(frame);
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, GoBackAndForward) {
  chromium::web::FramePtr frame = CreateFrame();
  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  CheckLoadUrl(title1.spec(), kPage1Title, controller.get());
  CheckLoadUrl(title2.spec(), kPage2Title, controller.get());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_,
                MockableOnNavigationStateChanged(testing::AllOf(
                    Field(&NavigationDetails::title, kPage1Title),
                    Field(&NavigationDetails::url, IsSet()))))
        .WillOnce(InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));
    controller->GoBack();
    run_loop.Run();
    navigation_observer_.Acknowledge();
  }

  // At the top of the navigation entry list; this should be a no-op.
  controller->GoBack();

  // Process the navigation request message.
  base::RunLoop().RunUntilIdle();

  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_,
                MockableOnNavigationStateChanged(testing::AllOf(
                    Field(&NavigationDetails::title, kPage2Title),
                    Field(&NavigationDetails::url, IsSet()))))
        .WillOnce(InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));
    controller->GoForward();
    run_loop.Run();
    navigation_observer_.Acknowledge();
  }

  // At the end of the navigation entry list; this should be a no-op.
  controller->GoForward();

  // Process the navigation request message.
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, ReloadFrame) {
  chromium::web::FramePtr frame = CreateFrame();
  chromium::web::NavigationControllerPtr navigation_controller;
  frame->GetNavigationController(navigation_controller.NewRequest());

  embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
      &FrameImplTest::OnServeHttpRequest, base::Unretained(this)));

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kPage1Path));

  EXPECT_CALL(*this, OnServeHttpRequest(_));
  CheckLoadUrl(url.spec(), kPage1Title, navigation_controller.get());

  navigation_observer_.Observe(
      context_impl()->GetFrameImplForTest(&frame)->web_contents_.get());

  // Reload with NO_CACHE.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnServeHttpRequest(_));
    EXPECT_CALL(navigation_observer_, DidFinishLoad(_, url))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    navigation_controller->Reload(chromium::web::ReloadType::NO_CACHE);
    run_loop.Run();
    Mock::VerifyAndClearExpectations(this);
    navigation_observer_.Acknowledge();
  }
  // Reload with PARTIAL_CACHE.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnServeHttpRequest(_));
    EXPECT_CALL(navigation_observer_, DidFinishLoad(_, url))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    navigation_controller->Reload(chromium::web::ReloadType::PARTIAL_CACHE);
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, GetVisibleEntry) {
  chromium::web::FramePtr frame = CreateFrame();

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  // Verify that a Frame returns a null NavigationEntry prior to receiving any
  // LoadUrl() calls.
  {
    base::RunLoop run_loop;
    controller->GetVisibleEntry(
        [&run_loop](std::unique_ptr<chromium::web::NavigationEntry> details) {
          EXPECT_EQ(nullptr, details.get());
          run_loop.Quit();
        });
    run_loop.Run();
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  // Navigate to a page.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_,
                MockableOnNavigationStateChanged(testing::AllOf(
                    Field(&NavigationDetails::title, kPage1Title),
                    Field(&NavigationDetails::url, IsSet()))))
        .WillOnce(testing::InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));
    controller->LoadUrl(title1.spec(), nullptr);
    run_loop.Run();
    navigation_observer_.Acknowledge();
  }

  // Verify that GetVisibleEntry() reflects the new Frame navigation state.
  {
    base::RunLoop run_loop;
    controller->GetVisibleEntry(
        [&run_loop,
         &title1](std::unique_ptr<chromium::web::NavigationEntry> details) {
          EXPECT_TRUE(details);
          EXPECT_EQ(details->url, title1.spec());
          EXPECT_EQ(details->title, kPage1Title);
          run_loop.Quit();
        });
    run_loop.Run();
  }

  // Navigate to another page.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_,
                MockableOnNavigationStateChanged(testing::AllOf(
                    Field(&NavigationDetails::title, kPage2Title),
                    Field(&NavigationDetails::url, IsSet()))))
        .WillOnce(testing::InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));
    controller->LoadUrl(title2.spec(), nullptr);
    run_loop.Run();
    navigation_observer_.Acknowledge();
  }

  // Verify the navigation with GetVisibleEntry().
  {
    base::RunLoop run_loop;
    controller->GetVisibleEntry(
        [&run_loop,
         &title2](std::unique_ptr<chromium::web::NavigationEntry> details) {
          EXPECT_TRUE(details);
          EXPECT_EQ(details->url, title2.spec());
          EXPECT_EQ(details->title, kPage2Title);
          run_loop.Quit();
        });
    run_loop.Run();
  }

  // Navigate back to the first page.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_,
                MockableOnNavigationStateChanged(testing::AllOf(
                    Field(&NavigationDetails::title, kPage1Title),
                    Field(&NavigationDetails::url, IsSet()))))
        .WillOnce(testing::InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));
    controller->GoBack();
    run_loop.Run();
    navigation_observer_.Acknowledge();
  }

  // Verify the navigation with GetVisibleEntry().
  {
    base::RunLoop run_loop;
    controller->GetVisibleEntry(
        [&run_loop,
         &title1](std::unique_ptr<chromium::web::NavigationEntry> details) {
          EXPECT_TRUE(details);
          EXPECT_EQ(details->url, title1.spec());
          EXPECT_EQ(details->title, kPage1Title);
          run_loop.Quit();
        });
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, NoNavigationObserverAttached) {
  chromium::web::FramePtr frame;
  context()->CreateFrame(frame.NewRequest());
  base::RunLoop().RunUntilIdle();

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  navigation_observer_.Observe(
      context_impl()->GetFrameImplForTest(&frame)->web_contents_.get());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_, DidFinishLoad(_, title1))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->LoadUrl(title1.spec(), nullptr);
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_, DidFinishLoad(_, title2))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->LoadUrl(title2.spec(), nullptr);
    run_loop.Run();
  }
}

fuchsia::mem::Buffer CreateBuffer(base::StringPiece data) {
  fuchsia::mem::Buffer output;
  output.size = data.size();

  zx_status_t status = zx::vmo::create(data.size(), 0, &output.vmo);
  ZX_CHECK(status == ZX_OK, ZX_OK) << "zx_vmo_create";

  status = output.vmo.write(data.data(), 0, data.size());
  ZX_CHECK(status == ZX_OK, status) << "zx_vmo_write";

  return output;
}

// Test JS injection by using Javascript to trigger document navigation.
IN_PROC_BROWSER_TEST_F(FrameImplTest, ExecuteJavaScriptImmediate) {
  chromium::web::FramePtr frame = CreateFrame();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  CheckLoadUrl(title1.spec(), kPage1Title, controller.get());
  fidl::VectorPtr<fidl::StringPtr> origins =
      fidl::VectorPtr<fidl::StringPtr>::New(0);
  origins.push_back(title1.GetOrigin().spec());

  frame->ExecuteJavaScript(
      std::move(origins),
      CreateBuffer("window.location.href = \"" + title2.spec() + "\";"),
      chromium::web::ExecuteMode::IMMEDIATE_ONCE,
      [](bool success) { EXPECT_TRUE(success); });

  base::RunLoop run_loop;
  EXPECT_CALL(navigation_observer_,
              MockableOnNavigationStateChanged(
                  testing::AllOf(Field(&NavigationDetails::title, kPage2Title),
                                 Field(&NavigationDetails::url, IsSet()))))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  CheckRunWithTimeout(&run_loop);

  frame.Unbind();
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, ExecuteJavaScriptOnLoad) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  chromium::web::FramePtr frame = CreateFrame();

  fidl::VectorPtr<fidl::StringPtr> origins =
      fidl::VectorPtr<fidl::StringPtr>::New(0);
  origins.push_back(url.GetOrigin().spec());

  frame->ExecuteJavaScript(std::move(origins),
                           CreateBuffer("stashed_title = 'hello';"),
                           chromium::web::ExecuteMode::ON_PAGE_LOAD,
                           [](bool success) { EXPECT_TRUE(success); });

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  CheckLoadUrl(url.spec(), "hello", controller.get());

  frame.Unbind();
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, ExecuteJavascriptOnLoadWrongOrigin) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  chromium::web::FramePtr frame = CreateFrame();

  fidl::VectorPtr<fidl::StringPtr> origins =
      fidl::VectorPtr<fidl::StringPtr>::New(0);
  origins.push_back("http://example.com");

  frame->ExecuteJavaScript(std::move(origins),
                           CreateBuffer("stashed_title = 'hello';"),
                           chromium::web::ExecuteMode::ON_PAGE_LOAD,
                           [](bool success) { EXPECT_TRUE(success); });

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  // Expect that the original HTML title is used, because we didn't inject a
  // script with a replacement title.
  CheckLoadUrl(url.spec(), "Welcome to Stan the Offline Dino's Homepage",
               controller.get());

  frame.Unbind();
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, ExecuteJavaScriptOnLoadWildcardOrigin) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  chromium::web::FramePtr frame = CreateFrame();

  fidl::VectorPtr<fidl::StringPtr> origins =
      fidl::VectorPtr<fidl::StringPtr>::New(0);
  origins.push_back("*");

  frame->ExecuteJavaScript(std::move(origins),
                           CreateBuffer("stashed_title = 'hello';"),
                           chromium::web::ExecuteMode::ON_PAGE_LOAD,
                           [](bool success) { EXPECT_TRUE(success); });

  // Test script injection for the origin 127.0.0.1.
  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  CheckLoadUrl(url.spec(), "hello", controller.get());

  CheckLoadUrl(url::kAboutBlankURL, url::kAboutBlankURL, controller.get());

  // Test script injection using a different origin ("localhost"), which should
  // still be picked up by the wildcard.
  GURL alt_url = embedded_test_server()->GetURL("localhost", kDynamicTitlePath);
  CheckLoadUrl(alt_url.spec(), "hello", controller.get());

  frame.Unbind();
}

// Test that consecutive scripts are executed in order by computing a cumulative
// result.
IN_PROC_BROWSER_TEST_F(FrameImplTest, ExecuteMultipleJavaScriptsOnLoad) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  chromium::web::FramePtr frame = CreateFrame();

  fidl::VectorPtr<fidl::StringPtr> origins =
      fidl::VectorPtr<fidl::StringPtr>::New(0);
  origins.push_back(url.GetOrigin().spec());
  frame->ExecuteJavaScript(origins.Clone(),
                           CreateBuffer("stashed_title = 'hello';"),
                           chromium::web::ExecuteMode::ON_PAGE_LOAD,
                           [](bool success) { EXPECT_TRUE(success); });
  frame->ExecuteJavaScript(std::move(origins),
                           CreateBuffer("stashed_title += ' there';"),
                           chromium::web::ExecuteMode::ON_PAGE_LOAD,
                           [](bool success) { EXPECT_TRUE(success); });

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  CheckLoadUrl(url.spec(), "hello there", controller.get());

  frame.Unbind();
}

// Test that we can inject scripts before and after RenderFrame creation.
IN_PROC_BROWSER_TEST_F(FrameImplTest, ExecuteOnLoadEarlyAndLateRegistrations) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  chromium::web::FramePtr frame = CreateFrame();

  fidl::VectorPtr<fidl::StringPtr> origins =
      fidl::VectorPtr<fidl::StringPtr>::New(0);
  origins.push_back(url.GetOrigin().spec());

  frame->ExecuteJavaScript(origins.Clone(),
                           CreateBuffer("stashed_title = 'hello';"),
                           chromium::web::ExecuteMode::ON_PAGE_LOAD,
                           [](bool success) { EXPECT_TRUE(success); });

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  CheckLoadUrl(url.spec(), "hello", controller.get());

  frame->ExecuteJavaScript(std::move(origins),
                           CreateBuffer("stashed_title += ' there';"),
                           chromium::web::ExecuteMode::ON_PAGE_LOAD,
                           [](bool success) { EXPECT_TRUE(success); });

  // Navigate away to clean the slate.
  CheckLoadUrl(url::kAboutBlankURL, url::kAboutBlankURL, controller.get());

  // Navigate back and see if both scripts are working.
  CheckLoadUrl(url.spec(), "hello there", controller.get());

  frame.Unbind();
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, ExecuteJavaScriptBadEncoding) {
  chromium::web::FramePtr frame = CreateFrame();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kPage1Path));

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  CheckLoadUrl(url.spec(), kPage1Title, controller.get());

  base::RunLoop run_loop;

  // 0xFE is an illegal UTF-8 byte; it should cause UTF-8 conversion to fail.
  fidl::VectorPtr<fidl::StringPtr> origins =
      fidl::VectorPtr<fidl::StringPtr>::New(0);
  origins.push_back(url.host());
  frame->ExecuteJavaScript(std::move(origins), CreateBuffer("true;\xfe"),
                           chromium::web::ExecuteMode::IMMEDIATE_ONCE,
                           [&run_loop](bool success) {
                             EXPECT_FALSE(success);
                             run_loop.Quit();
                           });
  CheckRunWithTimeout(&run_loop);

  frame.Unbind();
}

// Verifies that a Frame will handle navigation observer disconnection events
// gracefully.
IN_PROC_BROWSER_TEST_F(FrameImplTest, NavigationObserverDisconnected) {
  chromium::web::FramePtr frame = CreateFrame();

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  navigation_observer_.Observe(
      context_impl()->GetFrameImplForTest(&frame)->web_contents_.get());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_, DidFinishLoad(_, title1));
    EXPECT_CALL(navigation_observer_,
                MockableOnNavigationStateChanged(testing::AllOf(
                    Field(&NavigationDetails::title, kPage1Title),
                    Field(&NavigationDetails::url, IsSet()))))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->LoadUrl(title1.spec(), nullptr);
    run_loop.Run();
  }

  // Disconnect the observer & spin the runloop to propagate the disconnection
  // event over IPC.
  navigation_observer_bindings().CloseAll();
  base::RunLoop().RunUntilIdle();

  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_, DidFinishLoad(_, title2))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->LoadUrl(title2.spec(), nullptr);
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, DelayedNavigationEventAck) {
  chromium::web::FramePtr frame = CreateFrame();

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  // Expect an navigation event here, but deliberately postpone acknowledgement
  // until the end of the test.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_,
                MockableOnNavigationStateChanged(testing::AllOf(
                    Field(&NavigationDetails::title, kPage1Title),
                    Field(&NavigationDetails::url, IsSet()))))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->LoadUrl(title1.spec(), nullptr);
    run_loop.Run();
    Mock::VerifyAndClearExpectations(this);
  }

  // Since we have blocked NavigationEventObserver's flow, we must observe the
  // WebContents events directly via a test-only seam.
  navigation_observer_.Observe(
      context_impl()->GetFrameImplForTest(&frame)->web_contents_.get());

  // Navigate to a second page.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_, DidFinishLoad(_, title2))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->LoadUrl(title2.spec(), nullptr);
    run_loop.Run();
    Mock::VerifyAndClearExpectations(this);
  }

  // Navigate to the first page.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_, DidFinishLoad(_, title1))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->LoadUrl(title1.spec(), nullptr);
    run_loop.Run();
    Mock::VerifyAndClearExpectations(this);
  }

  // Since there was no observable change in navigation state since the last
  // ack, there should be no more NavigationEvents generated.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_,
                MockableOnNavigationStateChanged(testing::AllOf(
                    Field(&NavigationDetails::title, kPage1Title),
                    Field(&NavigationDetails::url, IsSet()))))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    navigation_observer_.Acknowledge();
    run_loop.Run();
  }
}

// Observes events specific to the Stop() test case.
struct WebContentsObserverForStop : public content::WebContentsObserver {
  using content::WebContentsObserver::Observe;
  MOCK_METHOD1(DidStartNavigation, void(content::NavigationHandle*));
  MOCK_METHOD0(NavigationStopped, void());
};

IN_PROC_BROWSER_TEST_F(FrameImplTest, Stop) {
  chromium::web::FramePtr frame = CreateFrame();

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  ASSERT_TRUE(embedded_test_server()->Start());

  // Use a request handler that will accept the connection and stall
  // indefinitely.
  GURL hung_url(embedded_test_server()->GetURL("/hung"));

  WebContentsObserverForStop observer;
  observer.Observe(
      context_impl()->GetFrameImplForTest(&frame)->web_contents_.get());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer, DidStartNavigation(_))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->LoadUrl(hung_url.spec(), nullptr);
    run_loop.Run();
    Mock::VerifyAndClearExpectations(this);
  }

  EXPECT_TRUE(
      context_impl()->GetFrameImplForTest(&frame)->web_contents_->IsLoading());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer, NavigationStopped())
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->Stop();
    run_loop.Run();
    Mock::VerifyAndClearExpectations(this);
  }

  EXPECT_FALSE(
      context_impl()->GetFrameImplForTest(&frame)->web_contents_->IsLoading());
}

fuchsia::mem::Buffer MemBufferFromString(const std::string& data) {
  fuchsia::mem::Buffer buffer;

  zx_status_t status =
      zx::vmo::create(data.size(), ZX_VMO_NON_RESIZABLE, &buffer.vmo);
  ZX_CHECK(status == ZX_OK, status) << "zx_vmo_create";

  status = buffer.vmo.write(data.data(), 0, data.size());
  ZX_CHECK(status == ZX_OK, status) << "zx_vmo_write";

  buffer.size = data.size();
  return buffer;
}

// Reads a UTF-8 string from |buffer|.
std::string ReadFromBufferOrDie(const fuchsia::mem::Buffer& buffer) {
  std::string output;
  output.resize(buffer.size);
  zx_status_t status = buffer.vmo.read(&output[0], 0, buffer.size);
  ZX_CHECK(status == ZX_OK, status) << "zx_vmo_read";
  return output;
}

// Stores an asynchronously generated value for later retrieval.
template <typename T>
class AsyncValueReceiver {
 public:
  explicit AsyncValueReceiver(
      base::RepeatingClosure on_capture = base::DoNothing())
      : on_capture_(std::move(on_capture)) {}
  ~AsyncValueReceiver() = default;

  fit::function<void(T)> GetReceiveClosure() {
    return [this](T value) {
      captured_ = std::move(value);
      on_capture_.Run();
    };
  }

  T& captured() { return captured_; }

 private:
  T captured_;
  base::RepeatingClosure on_capture_;

  DISALLOW_COPY_AND_ASSIGN(AsyncValueReceiver<T>);
};

IN_PROC_BROWSER_TEST_F(FrameImplTest, PostMessage) {
  chromium::web::FramePtr frame = CreateFrame();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL post_message_url(
      embedded_test_server()->GetURL("/window_post_message.html"));

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  CheckLoadUrl(post_message_url.spec(), "postmessage", controller.get());

  chromium::web::WebMessage message;
  message.data = MemBufferFromString(kPage1Path);
  AsyncValueReceiver<bool> post_result;
  frame->PostMessage(std::move(message), post_message_url.GetOrigin().spec(),
                     post_result.GetReceiveClosure());
  base::RunLoop run_loop;
  EXPECT_CALL(navigation_observer_,
              MockableOnNavigationStateChanged(
                  testing::AllOf(Field(&NavigationDetails::title, kPage1Title),
                                 Field(&NavigationDetails::url, IsSet()))))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  CheckRunWithTimeout(&run_loop);
  EXPECT_TRUE(post_result.captured());

  frame.Unbind();
}

// Send a MessagePort to the content, then perform bidirectional messaging
// through the port.
IN_PROC_BROWSER_TEST_F(FrameImplTest, PostMessagePassMessagePort) {
  chromium::web::FramePtr frame = CreateFrame();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL post_message_url(embedded_test_server()->GetURL("/message_port.html"));

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  CheckLoadUrl(post_message_url.spec(), "messageport", controller.get());

  chromium::web::MessagePortPtr message_port;
  chromium::web::WebMessage msg;
  {
    msg.outgoing_transfer =
        std::make_unique<chromium::web::OutgoingTransferable>();
    msg.outgoing_transfer->set_message_port(message_port.NewRequest());
    msg.data = MemBufferFromString("hi");
    AsyncValueReceiver<bool> post_result;
    frame->PostMessage(std::move(msg), post_message_url.GetOrigin().spec(),
                       post_result.GetReceiveClosure());

    base::RunLoop run_loop;
    AsyncValueReceiver<chromium::web::WebMessage> receiver(
        run_loop.QuitClosure());
    message_port->ReceiveMessage(receiver.GetReceiveClosure());
    CheckRunWithTimeout(&run_loop);
    EXPECT_EQ("got_port", ReadFromBufferOrDie(receiver.captured().data));
  }

  {
    msg.data = MemBufferFromString("ping");
    AsyncValueReceiver<bool> post_result;
    message_port->PostMessage(std::move(msg), post_result.GetReceiveClosure());
    base::RunLoop run_loop;
    AsyncValueReceiver<chromium::web::WebMessage> receiver(
        run_loop.QuitClosure());
    message_port->ReceiveMessage(receiver.GetReceiveClosure());
    CheckRunWithTimeout(&run_loop);
    EXPECT_EQ("ack ping", ReadFromBufferOrDie(receiver.captured().data));
    EXPECT_TRUE(post_result.captured());
  }

  frame.Unbind();
}

// Send a MessagePort to the content, then perform bidirectional messaging
// over its channel.
IN_PROC_BROWSER_TEST_F(FrameImplTest, PostMessageMessagePortDisconnected) {
  chromium::web::FramePtr frame = CreateFrame();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL post_message_url(embedded_test_server()->GetURL("/message_port.html"));

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  CheckLoadUrl(post_message_url.spec(), "messageport", controller.get());

  chromium::web::MessagePortPtr message_port;
  chromium::web::WebMessage msg;
  {
    msg.outgoing_transfer =
        std::make_unique<chromium::web::OutgoingTransferable>();
    msg.outgoing_transfer->set_message_port(message_port.NewRequest());
    msg.data = MemBufferFromString("hi");
    AsyncValueReceiver<bool> post_result;
    frame->PostMessage(std::move(msg), post_message_url.GetOrigin().spec(),
                       post_result.GetReceiveClosure());

    base::RunLoop run_loop;
    AsyncValueReceiver<chromium::web::WebMessage> receiver(
        run_loop.QuitClosure());
    message_port->ReceiveMessage(receiver.GetReceiveClosure());
    CheckRunWithTimeout(&run_loop);
    EXPECT_EQ("got_port", ReadFromBufferOrDie(receiver.captured().data));
    EXPECT_TRUE(post_result.captured());
  }

  // Navigating off-page should tear down the Mojo channel, thereby causing the
  // MessagePortImpl to self-destruct and tear down its FIDL channel.
  {
    base::RunLoop run_loop;
    message_port.set_error_handler(
        [&run_loop](zx_status_t status) { run_loop.Quit(); });
    controller->LoadUrl(url::kAboutBlankURL, nullptr);
    CheckRunWithTimeout(&run_loop);
  }

  frame.Unbind();
}

// Send a MessagePort to the content, and through that channel, receive a
// different MessagePort that was created by the content. Verify the second
// channel's liveness by sending a ping to it.
IN_PROC_BROWSER_TEST_F(FrameImplTest, PostMessageUseContentProvidedPort) {
  chromium::web::FramePtr frame = CreateFrame();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL post_message_url(embedded_test_server()->GetURL("/message_port.html"));

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  CheckLoadUrl(post_message_url.spec(), "messageport", controller.get());

  chromium::web::MessagePortPtr incoming_message_port;
  chromium::web::WebMessage msg;
  {
    chromium::web::MessagePortPtr message_port;
    msg.outgoing_transfer =
        std::make_unique<chromium::web::OutgoingTransferable>();
    msg.outgoing_transfer->set_message_port(message_port.NewRequest());
    msg.data = MemBufferFromString("hi");
    AsyncValueReceiver<bool> post_result;
    frame->PostMessage(std::move(msg), "*", post_result.GetReceiveClosure());

    base::RunLoop run_loop;
    AsyncValueReceiver<chromium::web::WebMessage> receiver(
        run_loop.QuitClosure());
    message_port->ReceiveMessage(receiver.GetReceiveClosure());
    CheckRunWithTimeout(&run_loop);
    EXPECT_EQ("got_port", ReadFromBufferOrDie(receiver.captured().data));
    incoming_message_port =
        receiver.captured().incoming_transfer->message_port().Bind();
    EXPECT_TRUE(post_result.captured());
  }

  {
    AsyncValueReceiver<bool> post_result;
    msg.data = MemBufferFromString("ping");
    incoming_message_port->PostMessage(std::move(msg),
                                       post_result.GetReceiveClosure());
    base::RunLoop run_loop;
    AsyncValueReceiver<chromium::web::WebMessage> receiver(
        run_loop.QuitClosure());
    incoming_message_port->ReceiveMessage(receiver.GetReceiveClosure());
    CheckRunWithTimeout(&run_loop);
    EXPECT_EQ("ack ping", ReadFromBufferOrDie(receiver.captured().data));
    EXPECT_TRUE(post_result.captured());
  }

  frame.Unbind();
}

// TODO BEFORE SUBMITTING(kmarshall): bad origin tests, multiple buffered
// messages, perhaps a proof-of-concept test with injected bindings *and*
// message i/o.

}  // namespace webrunner
