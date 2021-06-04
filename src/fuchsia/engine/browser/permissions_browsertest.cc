// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/scoped_blocking_call.h"
#include "content/public/test/browser_test.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/base/test/frame_test_util.h"
#include "fuchsia/engine/browser/frame_impl_browser_test_base.h"
#include "fuchsia/engine/test/test_data.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

static const char kIframeTestPagePath[] = "/iframe.html";
static const char kMicTestPagePath[] = "/mic.html";
static const char kMicNoPermissionTestPagePath[] = "/mic.html?NoPermission";

class PermissionsBrowserTest : public FrameImplTestBaseWithServer {
 public:
  PermissionsBrowserTest() = default;
  ~PermissionsBrowserTest() override = default;

  void SetUpOnMainThread() override {
    FrameImplTestBaseWithServer::SetUpOnMainThread();
    frame_ = CreateFrame();
  }

  void GrantPermission(fuchsia::web::PermissionType type,
                       const std::string& origin) {
    fuchsia::web::PermissionDescriptor permission;
    permission.set_type(type);
    frame_->SetPermissionState(std::move(permission), origin,
                               fuchsia::web::PermissionState::GRANTED);
  }

 protected:
  void InjectBeforeLoadJs(const std::string& code);

  // Loads iframe.html with the specified URL used for the embedded page.
  void LoadPageInIframe(const std::string& url);

  std::unique_ptr<net::test_server::HttpResponse>
  RequestHandlerWithPermissionPolicy(
      const net::test_server::HttpRequest& request);

  uint64_t before_load_js_id_ = 1;
  fuchsia::web::FramePtr frame_;
};

void PermissionsBrowserTest::InjectBeforeLoadJs(const std::string& code) {
  frame_->AddBeforeLoadJavaScript(
      before_load_js_id_++, {"*"},
      cr_fuchsia::MemBufferFromString(code, "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        CHECK(result.is_response());
      });
}

void PermissionsBrowserTest::LoadPageInIframe(const std::string& url) {
  // Before loading a page on the default embedded test server, set the iframe
  // src to be |url|.
  InjectBeforeLoadJs(base::StringPrintf("iframeSrc = '%s';", url.c_str()));

  fuchsia::web::NavigationControllerPtr controller;
  frame_->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), {},
      embedded_test_server()->GetURL(kIframeTestPagePath).spec()));
}

IN_PROC_BROWSER_TEST_F(PermissionsBrowserTest, PermissionInSameOriginIframe) {
  GrantPermission(fuchsia::web::PermissionType::MICROPHONE,
                  embedded_test_server()->GetURL("/").GetOrigin().spec());

  // Mic permission is expected to be granted since the iframe is loaded from
  // the same origin.
  GURL iframe_src = embedded_test_server()->GetURL(kMicTestPagePath);

  ASSERT_NO_FATAL_FAILURE(LoadPageInIframe(iframe_src.spec()));

  navigation_listener_.RunUntilTitleEquals("ended");
}

IN_PROC_BROWSER_TEST_F(PermissionsBrowserTest, NoPermissionInSameOriginIframe) {
  // Mic permission is expected to be denied since it wasn't granted to the
  // parent frame.
  GURL iframe_src =
      embedded_test_server()->GetURL(kMicNoPermissionTestPagePath);

  ASSERT_NO_FATAL_FAILURE(LoadPageInIframe(iframe_src.spec()));

  navigation_listener_.RunUntilTitleEquals("ended-NotFoundError");
}

IN_PROC_BROWSER_TEST_F(PermissionsBrowserTest, PermissionInCrossOriginIframe) {
  GrantPermission(fuchsia::web::PermissionType::MICROPHONE,
                  embedded_test_server()->GetURL("/").GetOrigin().spec());

  // Start a second embedded test server. It's used to load the page inside
  // the <iframe> from an origin different from the origin of the embedding
  // page.
  net::EmbeddedTestServer second_test_server;
  second_test_server.ServeFilesFromSourceDirectory(
      base::FilePath(cr_fuchsia::kTestServerRoot));
  ASSERT_TRUE(second_test_server.Start());

  // Mic permissions are expected to be denied since the page is cross-origin.
  GURL iframe_src = second_test_server.GetURL(kMicNoPermissionTestPagePath);

  ASSERT_NO_FATAL_FAILURE(LoadPageInIframe(iframe_src.spec()));

  navigation_listener_.RunUntilTitleEquals("ended-NotAllowedError");
}

IN_PROC_BROWSER_TEST_F(PermissionsBrowserTest,
                       PermissionInCrossOriginIframeWithPermissionPolicy) {
  GrantPermission(fuchsia::web::PermissionType::MICROPHONE,
                  embedded_test_server()->GetURL("/").GetOrigin().spec());

  // Start a second embedded test server. It's used to load the page inside
  // the <iframe> from an origin different from the origin of the embedding
  // page.
  net::EmbeddedTestServer second_test_server;
  second_test_server.ServeFilesFromSourceDirectory(
      base::FilePath(cr_fuchsia::kTestServerRoot));
  ASSERT_TRUE(second_test_server.Start());

  // Mic permissions are expected to be granted because the parent frame has
  // access to the microphone and it's delegated to the child by the permission
  // policy (see code below).
  GURL iframe_src = second_test_server.GetURL(kMicTestPagePath);

  InjectBeforeLoadJs(
      base::StringPrintf("iframePermissionPolicy = 'microphone %s';",
                         iframe_src.GetOrigin().spec().c_str()));

  ASSERT_NO_FATAL_FAILURE(LoadPageInIframe(iframe_src.spec()));

  navigation_listener_.RunUntilTitleEquals("ended");
}
