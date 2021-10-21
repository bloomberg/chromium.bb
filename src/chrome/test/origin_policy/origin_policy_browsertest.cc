// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {
const base::FilePath::CharType kDataRoot[] =
    FILE_PATH_LITERAL("chrome/test/data/origin_policy_browsertest");

// The title of the Origin Policy error interstitial. This is used to determine
// whether the page load was blocked by the origin policy throttle.
const char16_t kErrorInterstitialTitle[] = u"Origin Policy Error";
}  // namespace

namespace content {

// OriginPolicyBrowserTest tests several aspects of OriginPolicyThrottle (plus
// associated logic elsewhere). These tests focus on error conditions, since
// the normal operating conditions are already well covered in cross-browser
// Web Platform Tests (wpt/origin-policy/*).

class OriginPolicyBrowserTest : public InProcessBrowserTest {
 public:
  OriginPolicyBrowserTest() : status_(net::HTTP_OK) {}

  OriginPolicyBrowserTest(const OriginPolicyBrowserTest&) = delete;
  OriginPolicyBrowserTest& operator=(const OriginPolicyBrowserTest&) = delete;

  ~OriginPolicyBrowserTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    server_ = std::make_unique<net::test_server::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);
    server_->AddDefaultHandlers(base::FilePath(kDataRoot));
    server_->RegisterRequestHandler(base::BindRepeating(
        &OriginPolicyBrowserTest::HandleResponse, base::Unretained(this)));
    EXPECT_TRUE(server()->Start());

    feature_list_.InitAndEnableFeature(features::kOriginPolicy);
  }

  void TearDownInProcessBrowserTestFixture() override { server_.reset(); }

  net::test_server::EmbeddedTestServer* server() { return server_.get(); }

  // Most tests here are set up to use the page title to distinguish between
  // successful load or the error page. For those tests, this method implements
  // the bulk of the test logic.
  std::u16string NavigateToAndReturnTitle(const char* url) {
    EXPECT_TRUE(server());
    EXPECT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(server()->GetURL(url))));
    std::u16string title;
    ui_test_utils::GetCurrentTabTitle(browser(), &title);
    return title;
  }

  void SetStatus(const net::HttpStatusCode& status) { status_ = status; }
  void SetLocationHeader(const std::string& header) {
    location_header_ = header;
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleResponse(
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        std::make_unique<net::test_server::BasicHttpResponse>();

    if (request.relative_url == "/.well-known/origin-policy") {
      response->set_code(status_);

      if (status_ == net::HTTP_OK) {
        response->set_content(R"({ "ids": ["my-policy"] })");
      } else if (location_header_.has_value()) {
        response->AddCustomHeader("Location", *location_header_);
      }

      return std::move(response);
    }

    // If we return nullptr, then the server will do the default behavior.
    return nullptr;
  }

  std::unique_ptr<net::test_server::EmbeddedTestServer> server_;
  base::test::ScopedFeatureList feature_list_;

  net::HttpStatusCode status_;
  absl::optional<std::string> location_header_;
};

IN_PROC_BROWSER_TEST_F(OriginPolicyBrowserTest, PageWithoutPolicy) {
  EXPECT_EQ(u"Page Without Policy",
            NavigateToAndReturnTitle("/page-without-policy.html"));
}

IN_PROC_BROWSER_TEST_F(OriginPolicyBrowserTest, PageWithoutPolicyPolicy404s) {
  SetStatus(net::HTTP_NOT_FOUND);
  EXPECT_EQ(u"Page Without Policy",
            NavigateToAndReturnTitle("/page-without-policy.html"));
}

IN_PROC_BROWSER_TEST_F(OriginPolicyBrowserTest, PageWithoutPolicyPolicy301s) {
  SetStatus(net::HTTP_MOVED_PERMANENTLY);
  SetLocationHeader("/.well-known/origin-policy/example-policy");
  EXPECT_EQ(u"Page Without Policy",
            NavigateToAndReturnTitle("/page-without-policy.html"));
}

IN_PROC_BROWSER_TEST_F(OriginPolicyBrowserTest, ApplyPolicy) {
  EXPECT_EQ(u"Page With Policy",
            NavigateToAndReturnTitle("/page-with-policy.html"));
}

IN_PROC_BROWSER_TEST_F(OriginPolicyBrowserTest, ErrorPolicy301Redirect) {
  SetStatus(net::HTTP_MOVED_PERMANENTLY);
  SetLocationHeader("/.well-known/origin-policy/example-policy");
  EXPECT_EQ(kErrorInterstitialTitle,
            NavigateToAndReturnTitle("/page-with-policy.html"));
}

IN_PROC_BROWSER_TEST_F(OriginPolicyBrowserTest, ErrorPolicy302Redirect) {
  SetStatus(net::HTTP_FOUND);
  SetLocationHeader("/.well-known/origin-policy/example-policy");
  EXPECT_EQ(kErrorInterstitialTitle,
            NavigateToAndReturnTitle("/page-with-policy.html"));
}

IN_PROC_BROWSER_TEST_F(OriginPolicyBrowserTest, ErrorPolicy307Redirect) {
  SetStatus(net::HTTP_TEMPORARY_REDIRECT);
  SetLocationHeader("/.well-known/origin-policy/example-policy");
  EXPECT_EQ(kErrorInterstitialTitle,
            NavigateToAndReturnTitle("/page-with-policy.html"));
}

}  // namespace content
