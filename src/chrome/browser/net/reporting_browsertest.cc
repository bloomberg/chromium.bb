// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/reporting/reporting_policy.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "url/gurl.h"

namespace {

const char kReportingHost[] = "example.com";

class BaseReportingBrowserTest : public CertVerifierBrowserTest,
                                 public ::testing::WithParamInterface<bool> {
 public:
  BaseReportingBrowserTest()
      : https_server_(net::test_server::EmbeddedTestServer::TYPE_HTTPS) {
    std::vector<base::Feature> required_features = {
        network::features::kReporting, network::features::kNetworkErrorLogging,
        features::kCrashReporting};
    if (UseDocumentReporting()) {
      required_features.push_back(net::features::kDocumentReporting);
    }
    scoped_feature_list_.InitWithFeatures(
        // enabled_features
        required_features,
        // disabled_features
        {});
  }

  BaseReportingBrowserTest(const BaseReportingBrowserTest&) = delete;
  BaseReportingBrowserTest& operator=(const BaseReportingBrowserTest&) = delete;

  ~BaseReportingBrowserTest() override = default;

  void SetUp() override;
  void SetUpOnMainThread() override;

  net::EmbeddedTestServer* server() { return &https_server_; }

  net::test_server::ControllableHttpResponse* original_response() {
    return original_response_.get();
  }

  net::test_server::ControllableHttpResponse* upload_response() {
    return upload_response_.get();
  }

  GURL GetReportingEnabledURL() const {
    return https_server_.GetURL(kReportingHost, "/original");
  }

  GURL GetCollectorURL() const {
    return https_server_.GetURL(kReportingHost, "/upload");
  }

  std::string GetAppropriateReportingHeader() const {
    return UseDocumentReporting() ? GetReportingEndpointsHeader()
                                  : GetReportToHeader();
  }

  std::string GetReportingEndpointsHeader() const {
    return "Reporting-Endpoints: default=\"" + GetCollectorURL().spec() +
           "\"\r\n";
  }

  std::string GetReportToHeader() const {
    return "Report-To: {\"endpoints\":[{\"url\":\"" + GetCollectorURL().spec() +
           "\"}],\"max_age\":86400}\r\n";
  }

  std::string GetNELHeader() const {
    return "NEL: "
           "{\"report_to\":\"default\",\"max_age\":86400,\"success_fraction\":"
           "1.0}\r\n";
  }

  std::string GetCSPHeader() const {
    return "Content-Security-Policy: script-src 'none'; report-to default\r\n";
  }

 protected:
  bool UseDocumentReporting() const {
#if BUILDFLAG(ENABLE_REPORTING)
    return GetParam();
#else
    return false;
#endif
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_;
  std::unique_ptr<net::test_server::ControllableHttpResponse>
      original_response_;
  std::unique_ptr<net::test_server::ControllableHttpResponse> upload_response_;
};

void BaseReportingBrowserTest::SetUp() {
  CertVerifierBrowserTest::SetUp();

  // Make report delivery happen instantly.
  net::ReportingPolicy policy;
  policy.delivery_interval = base::Seconds(0);
  net::ReportingPolicy::UsePolicyForTesting(policy);
}

void BaseReportingBrowserTest::SetUpOnMainThread() {
  CertVerifierBrowserTest::SetUpOnMainThread();

  host_resolver()->AddRule("*", "127.0.0.1");

  original_response_ =
      std::make_unique<net::test_server::ControllableHttpResponse>(server(),
                                                                   "/original");
  upload_response_ =
      std::make_unique<net::test_server::ControllableHttpResponse>(server(),
                                                                   "/upload");

  // Reporting and NEL will ignore configurations headers if the response
  // doesn't come from an HTTPS origin, or if the origin's certificate is
  // invalid.  Our test certs are valid, so we need a mock certificate verifier
  // to trick the Reporting stack into paying attention to our test headers.
  mock_cert_verifier()->set_default_result(net::OK);
  server()->AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(server()->Start());
}

class ReportingBrowserTest : public BaseReportingBrowserTest {
 public:
  ReportingBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kPartitionNelAndReportingByNetworkIsolationKey);
  }

  ReportingBrowserTest(const ReportingBrowserTest&) = delete;
  ReportingBrowserTest& operator=(const ReportingBrowserTest&) = delete;

  ~ReportingBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class NonIsolatedReportingBrowserTest : public BaseReportingBrowserTest {
 public:
  NonIsolatedReportingBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        net::features::kPartitionNelAndReportingByNetworkIsolationKey);
  }

  NonIsolatedReportingBrowserTest(const NonIsolatedReportingBrowserTest&) =
      delete;
  NonIsolatedReportingBrowserTest& operator=(
      const NonIsolatedReportingBrowserTest&) = delete;

  ~NonIsolatedReportingBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

std::unique_ptr<base::Value> ParseReportUpload(const std::string& payload) {
  auto parsed_payload = base::test::ParseJsonDeprecated(payload);
  // Clear out any non-reproducible fields.
  for (auto& report : parsed_payload->GetList()) {
    report.RemoveKey("age");
    report.RemovePath("body.elapsed_time");
    auto* user_agent =
        report.FindKeyOfType("user_agent", base::Value::Type::STRING);
    if (user_agent)
      *user_agent = base::Value("Mozilla/1.0");
  }
  return parsed_payload;
}

}  // namespace

// Tests that NEL reports are delivered correctly, whether or not reporting
// isolation is enabled. NEL reports can only be configured with the Report-To
// header, but this header should continue to function until support is
// completely removed.
IN_PROC_BROWSER_TEST_P(ReportingBrowserTest, TestNELHeadersProcessed) {
  NavigateParams params(browser(), GetReportingEnabledURL(),
                        ui::PAGE_TRANSITION_LINK);
  Navigate(&params);

  original_response()->WaitForRequest();
  original_response()->Send("HTTP/1.1 204 OK\r\n");
  original_response()->Send(GetReportToHeader());
  original_response()->Send(GetNELHeader());
  original_response()->Send("\r\n");
  original_response()->Done();

  upload_response()->WaitForRequest();
  std::unique_ptr<base::Value> actual =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 204 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  ASSERT_TRUE(actual);
  base::Value expected = base::test::ParseJson(base::StringPrintf(
      R"json(
        [
          {
            "body": {
              "protocol": "http/1.1",
              "referrer": "",
              "sampling_fraction": 1.0,
              "server_ip": "127.0.0.1",
              "method": "GET",
              "status_code": 204,
              "phase": "application",
              "type": "ok",
            },
            "type": "network-error",
            "url": "%s",
            "user_agent": "Mozilla/1.0",
          },
        ]
      )json",
      GetReportingEnabledURL().spec().c_str()));
  EXPECT_EQ(expected, *actual);
}

// Tests that CSP reports are delivered properly whether configured with the
// v0 Report-To header or the v1 Reporting-Endpoints header.
IN_PROC_BROWSER_TEST_P(ReportingBrowserTest, TestReportingHeadersProcessed) {
  NavigateParams params(browser(), GetReportingEnabledURL(),
                        ui::PAGE_TRANSITION_LINK);
  Navigate(&params);

  original_response()->WaitForRequest();
  original_response()->Send("HTTP/1.1 200 OK\r\n");
  original_response()->Send("Content-Type: text/html\r\n");
  original_response()->Send(GetAppropriateReportingHeader());
  original_response()->Send(GetCSPHeader());
  original_response()->Send("\r\n");
  original_response()->Send("<script>alert(1);</script>\r\n");
  original_response()->Done();

  upload_response()->WaitForRequest();
  std::unique_ptr<base::Value> actual =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 204 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  ASSERT_TRUE(actual);
  base::Value expected = base::test::ParseJson(base::StringPrintf(
      R"json(
        [ {
           "body": {
              "blockedURL": "inline",
              "disposition": "enforce",
              "documentURL": "%s",
              "effectiveDirective": "script-src-elem",
              "lineNumber": 1,
              "originalPolicy": "script-src 'none'; report-to default",
              "referrer": "",
              "sample": "",
              "sourceFile": "%s",
              "statusCode": 200
           },
           "type": "csp-violation",
           "url": "%s",
           "user_agent": "Mozilla/1.0"
        } ]
      )json",
      GetReportingEnabledURL().spec().c_str(),
      GetReportingEnabledURL().spec().c_str(),
      GetReportingEnabledURL().spec().c_str()));
  EXPECT_EQ(expected, *actual);
}

// Tests that CSP reports are delivered properly whether configured with the
// v0 Report-To header or the v1 Reporting-Endpoints header. This is a Non-
// isolated test, so will run with NIK-based report isolation disabled. This is
// a regression test for https://crbug.com/1258112.
IN_PROC_BROWSER_TEST_P(NonIsolatedReportingBrowserTest,
                       TestReportingHeadersProcessed) {
  NavigateParams params(browser(), GetReportingEnabledURL(),
                        ui::PAGE_TRANSITION_LINK);
  Navigate(&params);

  original_response()->WaitForRequest();
  original_response()->Send("HTTP/1.1 200 OK\r\n");
  original_response()->Send("Content-Type: text/html\r\n");
  original_response()->Send(GetAppropriateReportingHeader());
  original_response()->Send(GetCSPHeader());
  original_response()->Send("\r\n");
  original_response()->Send("<script>alert(1);</script>\r\n");
  original_response()->Done();

  // Ensure that the correct endpoint was found, and that a report was sent.
  // (If the endpoint cannot not be found, then a report will be sent at all.)
  upload_response()->WaitForRequest();
  std::unique_ptr<base::Value> actual =
      ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 204 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  ASSERT_TRUE(actual);
  base::Value expected = base::test::ParseJson(base::StringPrintf(
      R"json(
        [ {
           "body": {
              "blockedURL": "inline",
              "disposition": "enforce",
              "documentURL": "%s",
              "effectiveDirective": "script-src-elem",
              "lineNumber": 1,
              "originalPolicy": "script-src 'none'; report-to default",
              "referrer": "",
              "sample": "",
              "sourceFile": "%s",
              "statusCode": 200
           },
           "type": "csp-violation",
           "url": "%s",
           "user_agent": "Mozilla/1.0"
        } ]
      )json",
      GetReportingEnabledURL().spec().c_str(),
      GetReportingEnabledURL().spec().c_str(),
      GetReportingEnabledURL().spec().c_str()));
  EXPECT_EQ(expected, *actual);
}

IN_PROC_BROWSER_TEST_P(ReportingBrowserTest,
                       ReportingRespectsNetworkIsolationKeys) {
  // Navigate main frame to a kReportingHost URL and learn NEL and Reporting
  // information for that host.
  NavigateParams params(browser(), GetReportingEnabledURL(),
                        ui::PAGE_TRANSITION_LINK);
  Navigate(&params);
  original_response()->WaitForRequest();
  original_response()->Send("HTTP/1.1 204 OK\r\n");
  original_response()->Send(GetReportToHeader());
  original_response()->Send(
      "NEL: {"
      "  \"report_to\":\"default\","
      "  \"max_age\":86400,"
      "  \"failure_fraction\":1.0"
      "}\r\n\r\n");
  original_response()->Done();

  // Open a cross-origin kReportingHost iframe that fails to load. No report
  // should be uploaded, since the NetworkIsolationKey does not match.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), server()->GetURL("/iframe_blank.html")));
  content::NavigateIframeToURL(
      browser()->tab_strip_model()->GetActiveWebContents(), "test",
      server()->GetURL(kReportingHost, "/close-socket?should-not-be-reported"));

  // Navigate the main frame to a kReportingHost URL that fails to load. A
  // report should be uploaded, since the NetworkIsolationKey matches that of
  // the original request where reporting information was learned.
  GURL expect_reported_url =
      server()->GetURL(kReportingHost, "/close-socket?should-be-reported");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), expect_reported_url));
  upload_response()->WaitForRequest();
  std::unique_ptr<base::Value> actual =
      ParseReportUpload(upload_response()->http_request()->content);

  // Verify the contents of the received report.
  ASSERT_TRUE(actual);
  std::unique_ptr<base::Value> expected =
      base::test::ParseJsonDeprecated(base::StringPrintf(
          R"json(
        [
          {
            "body": {
              "protocol": "http/1.1",
              "referrer": "",
              "sampling_fraction": 1.0,
              "server_ip": "127.0.0.1",
              "method": "GET",
              "status_code": 0,
              "phase": "application",
              "type": "http.response.invalid.empty",
            },
            "type": "network-error",
            "url": "%s",
            "user_agent": "Mozilla/1.0",
          },
        ]
      )json",
          expect_reported_url.spec().c_str()));
  EXPECT_EQ(*expected, *actual);
}

// These tests intentionally crash a render process, and so fail ASan tests.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_CrashReport DISABLED_CrashReport
#define MAYBE_CrashReportUnresponsive DISABLED_CrashReportUnresponsive
#else
#define MAYBE_CrashReport CrashReport

// Flaky on Mac (multiple versions), see https://crbug.com/1261749
#if defined(OS_MAC)
#define MAYBE_CrashReportUnresponsive DISABLED_CrashReportUnresponsive
#else
#define MAYBE_CrashReportUnresponsive CrashReportUnresponsive
#endif
#endif

IN_PROC_BROWSER_TEST_P(ReportingBrowserTest, MAYBE_CrashReport) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver navigation_observer(contents);

  // Navigate to reporting-enabled page.
  NavigateParams params(browser(), GetReportingEnabledURL(),
                        ui::PAGE_TRANSITION_LINK);
  Navigate(&params);

  original_response()->WaitForRequest();
  original_response()->Send("HTTP/1.1 200 OK\r\n");
  original_response()->Send(GetAppropriateReportingHeader());
  original_response()->Send("\r\n");
  original_response()->Done();
  navigation_observer.Wait();

  // Simulate a crash on the page.
  content::ScopedAllowRendererCrashes allow_renderer_crashes(contents);
  contents->GetController().LoadURL(GURL(blink::kChromeUICrashURL),
                                    content::Referrer(),
                                    ui::PAGE_TRANSITION_TYPED, std::string());

  upload_response()->WaitForRequest();
  auto response = ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  EXPECT_TRUE(response != nullptr);
  auto report = response->GetList().begin();
  auto* type = report->FindKeyOfType("type", base::Value::Type::STRING);
  auto* url = report->FindKeyOfType("url", base::Value::Type::STRING);

  EXPECT_EQ("crash", type->GetString());
  EXPECT_EQ(GetReportingEnabledURL().spec(), url->GetString());
}

IN_PROC_BROWSER_TEST_P(ReportingBrowserTest, MAYBE_CrashReportUnresponsive) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver navigation_observer(contents);

  // Navigate to reporting-enabled page.
  NavigateParams params(browser(), GetReportingEnabledURL(),
                        ui::PAGE_TRANSITION_LINK);
  Navigate(&params);

  original_response()->WaitForRequest();
  original_response()->Send("HTTP/1.1 200 OK\r\n");
  original_response()->Send(GetAppropriateReportingHeader());
  original_response()->Send("\r\n");
  original_response()->Done();
  navigation_observer.Wait();

  // Simulate the page being killed due to being unresponsive.
  content::ScopedAllowRendererCrashes allow_renderer_crashes(contents);
  contents->GetMainFrame()->GetProcess()->Shutdown(content::RESULT_CODE_HUNG);

  upload_response()->WaitForRequest();
  auto response = ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 200 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  EXPECT_TRUE(response != nullptr);
  auto report = response->GetList().begin();
  auto* type = report->FindKeyOfType("type", base::Value::Type::STRING);
  auto* url = report->FindKeyOfType("url", base::Value::Type::STRING);
  auto* body = report->FindKeyOfType("body", base::Value::Type::DICTIONARY);
  auto* reason = body->FindKeyOfType("reason", base::Value::Type::STRING);

  EXPECT_EQ("crash", type->GetString());
  EXPECT_EQ(GetReportingEnabledURL().spec(), url->GetString());
  EXPECT_EQ("unresponsive", reason->GetString());
}

INSTANTIATE_TEST_SUITE_P(All, ReportingBrowserTest, ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(All,
                         NonIsolatedReportingBrowserTest,
                         ::testing::Bool());
