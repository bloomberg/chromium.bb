// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/values_test_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/test/test_content_browser_client.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

namespace {
constexpr char kBaseDataDir[] = "content/test/data/";
}

// Waits for the a given |report_url| to be received by the test server. Wraps a
// ControllableHttpResponse so that it can wait for the server request in a
// thread-safe manner. Therefore, these must be registered prior to |server|
// starting.
struct ExpectedReportWaiter {
  ExpectedReportWaiter(GURL report_url,
                       std::string attribution_destination,
                       std::string source_event_id,
                       std::string source_type,
                       std::string trigger_data,
                       net::EmbeddedTestServer* server)
      : ExpectedReportWaiter(std::move(report_url),
                             base::DictionaryValue(),
                             server) {
    expected_body.SetStringKey("attribution_destination",
                               std::move(attribution_destination));
    expected_body.SetStringKey("source_event_id", std::move(source_event_id));
    expected_body.SetStringKey("source_type", std::move(source_type));
    expected_body.SetStringKey("trigger_data", std::move(trigger_data));
  }

  // ControllableHTTPResponses can only wait for relative urls, so only supply
  // the path.
  ExpectedReportWaiter(GURL report_url,
                       base::Value body,
                       net::EmbeddedTestServer* server)
      : expected_url(std::move(report_url)),
        expected_body(std::move(body)),
        response(std::make_unique<net::test_server::ControllableHttpResponse>(
            server,
            expected_url.path())) {}

  GURL expected_url;
  base::Value expected_body;
  std::unique_ptr<net::test_server::ControllableHttpResponse> response;

  bool HasRequest() { return !!response->http_request(); }

  // Waits for a report to be received matching the report url. Verifies that
  // the report url and report body were set correctly.
  void WaitForReport() {
    if (!response->http_request())
      response->WaitForRequest();

    // The embedded test server resolves all urls to 127.0.0.1, so get the real
    // request host from the request headers.
    const net::test_server::HttpRequest& request = *response->http_request();
    DCHECK(request.headers.find("Host") != request.headers.end());
    const GURL& request_url = request.GetURL();
    GURL header_url = GURL("https://" + request.headers.at("Host"));
    std::string host = header_url.host();
    GURL::Replacements replace_host;
    replace_host.SetHostStr(host);

    base::Value body = base::test::ParseJson(request.content);
    EXPECT_THAT(body, base::test::DictionaryHasValues(expected_body));

    // Clear the port as it is assigned by the EmbeddedTestServer at runtime.
    replace_host.SetPortStr("");

    // Compare the expected report url with a URL formatted with the host
    // defined in the headers. This would not match |expected_url| if the host
    // for report url was not set properly.
    EXPECT_EQ(expected_url, request_url.ReplaceComponents(replace_host));
  }
};

}  // namespace

class AttributionsBrowserTest : public ContentBrowserTest {
 public:
  AttributionsBrowserTest() { AttributionManagerImpl::RunInMemoryForTesting(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kConversionsDebugMode);

    // Sets up the blink runtime feature for ConversionMeasurement.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);

    // Sets up support for event sources.
    command_line->AppendSwitch(switches::kEnableBlinkTestFeatures);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    net::test_server::RegisterDefaultHandlers(https_server_.get());
    https_server_->ServeFilesFromSourceDirectory("content/test/data");
    SetupCrossSiteRedirector(https_server_.get());
  }

  WebContents* web_contents() { return shell()->web_contents(); }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

 protected:
  AttributionDisallowingContentBrowserClient disallowed_browser_client_;

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

// Verifies that storage initialization does not hang when initialized in a
// browsertest context, see https://crbug.com/1080764).
IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
                       FeatureEnabled_StorageInitWithoutHang) {}

IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
                       ImpressionConversion_ReportSent) {
  // Expected reports must be registered before the server starts.
  ExpectedReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-attribution"),
      /*attribution_destination=*/"https://b.test",
      /*source_event_id=*/"1", /*source_type=*/"navigation",
      /*trigger_data=*/"7", https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  // Create an anchor tag with impression attributes and click the link. By
  // default the target is set to "_top".
  GURL conversion_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/page_with_conversion_redirect.html");
  EXPECT_TRUE(
      ExecJs(web_contents(),
             JsReplace(R"(
    createImpressionTag({id: 'link',
                        url: $1,
                        data: '1',
                        destination: $2});)",
                       conversion_url, url::Origin::Create(conversion_url))));

  TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(ExecJs(shell(), "simulateClick('link');"));
  observer.Wait();

  // Register a conversion with the original page as the reporting origin.
  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("registerConversion({data: 7, origin: $1})",
                               url::Origin::Create(impression_url))));

  expected_report.WaitForReport();
}

IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
                       WindowOpenDeprecatedAPI_NoException) {
  // Expected reports must be registered before the server starts.
  ExpectedReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-attribution"),
      /*body=*/base::Value(), https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  // Create an anchor tag with impression attributes and click the link. By
  // default the target is set to "_top".
  GURL conversion_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/page_with_conversion_redirect.html");
  TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(
      ExecJs(web_contents(),
             JsReplace(R"(window.open($1, '_top', '',
               {attributionSourceEventId: '1', attributeOn: $2});)",
                       conversion_url, url::Origin::Create(conversion_url))));
  observer.Wait();

  // Register a conversion with the original page as the reporting origin.
  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("registerConversion({data: 7, origin: $1})",
                               url::Origin::Create(impression_url))));

  // TODO(johnidel): This API surface was removed due to
  // https://crbug.com/1187881. This test should be updated to verify the
  // behavior with the new surface.
  base::RunLoop run_loop;
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(100));
  run_loop.Run();
  EXPECT_FALSE(expected_report.HasRequest());
}

IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
                       WindowOpenImpressionConversion_ReportSent) {
  // Expected reports must be registered before the server starts.
  ExpectedReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-attribution"),
      /*attribution_destination=*/"https://b.test",
      /*source_event_id=*/"1", /*source_type=*/"navigation",
      /*trigger_data=*/"7", https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  GURL conversion_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/page_with_conversion_redirect.html");

  // We can't use `JsReplace` directly to input the origin as it will use string
  // literals which shouldn't be provided in the window features string.
  std::string window_features =
      base::StrCat({"attributionsourceeventid=1,attributiondestination=",
                    url::Origin::Create(conversion_url).Serialize()});

  TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(
      ExecJs(web_contents(), JsReplace(R"(window.open($1, '_top', $2);)",
                                       conversion_url, window_features)));
  observer.Wait();

  // Register a conversion with the original page as the reporting origin.
  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("registerConversion({data: 7, origin: $1})",
                               url::Origin::Create(impression_url))));

  expected_report.WaitForReport();
}

IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
                       ImpressionFromCrossOriginSubframe_ReportSent) {
  ExpectedReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-attribution"),
      /*attribution_destination=*/"https://b.test",
      /*source_event_id=*/"1", /*source_type=*/"navigation",
      /*trigger_data=*/"7", https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL page_url = https_server()->GetURL("a.test", "/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  GURL subframe_url = https_server()->GetURL(
      "c.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(ExecJs(shell(), R"(
    let frame= document.getElementById('test_iframe');
    frame.setAttribute('allow', 'attribution-reporting');)"));
  NavigateIframeToURL(web_contents(), "test_iframe", subframe_url);
  RenderFrameHost* subframe = ChildFrameAt(web_contents()->GetMainFrame(), 0);

  // Create an impression tag in the subframe and target a popup window.
  GURL conversion_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/page_with_conversion_redirect.html");
  EXPECT_TRUE(ExecJs(subframe, JsReplace(R"(
    createImpressionTag({id: 'link',
                        url: $1,
                        data: '1',
                        destination: $2,
                        target: 'new_frame'});)",
                                         conversion_url,
                                         url::Origin::Create(conversion_url))));

  ShellAddedObserver new_shell_observer;
  TestNavigationObserver observer(nullptr);
  observer.StartWatchingNewWebContents();
  EXPECT_TRUE(ExecJs(subframe, "simulateClick('link');"));
  WebContents* popup_contents = new_shell_observer.GetShell()->web_contents();
  observer.Wait();

  // Register a conversion with the original page as the reporting origin.
  EXPECT_TRUE(ExecJs(popup_contents,
                     JsReplace("registerConversion({data: 7, origin: $1})",
                               url::Origin::Create(page_url))));

  expected_report.WaitForReport();
}

IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
                       ImpressionOnNoOpenerNavigation_ReportSent) {
  ExpectedReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-attribution"),
      /*attribution_destination=*/"https://b.test",
      /*source_event_id=*/"1", /*source_type=*/"navigation",
      /*trigger_data=*/"7", https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  GURL conversion_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/page_with_conversion_redirect.html");

  // target="_blank" navs are rel="noopener" by default.
  EXPECT_TRUE(
      ExecJs(web_contents(),
             JsReplace(R"(
    createImpressionTag({id: 'link',
                        url: $1,
                        data: '1',
                        destination: $2,
                        target: '_blank'});)",
                       conversion_url, url::Origin::Create(conversion_url))));

  TestNavigationObserver observer(nullptr);
  observer.StartWatchingNewWebContents();
  EXPECT_TRUE(ExecJs(shell(), "simulateClick('link');"));
  observer.Wait();

  EXPECT_TRUE(ExecJs(Shell::windows()[1]->web_contents(),
                     JsReplace("registerConversion({data: 7, origin: $1})",
                               url::Origin::Create(impression_url))));
  expected_report.WaitForReport();
}

IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
                       ImpressionConversionSameDomain_ReportSent) {
  // Expected reports must be registered before the server starts.
  ExpectedReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-attribution"),
      /*attribution_destination=*/"https://b.test",
      /*source_event_id=*/"1", /*source_type=*/"navigation",
      /*trigger_data=*/"7", https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  // Create an anchor tag with impression attributes and click the link. By
  // default the target is set to "_top".
  GURL conversion_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/page_with_conversion_redirect.html");
  GURL conversion_dest_url = https_server()->GetURL(
      "sub.b.test",
      "/attribution_reporting/page_with_conversion_redirect.html");
  EXPECT_TRUE(ExecJs(
      web_contents(),
      JsReplace(R"(
    createImpressionTag({id: 'link',
                        url: $1,
                        data: '1',
                        destination: $2});)",
                conversion_url, url::Origin::Create(conversion_dest_url))));

  TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(ExecJs(shell(), "simulateClick('link');"));
  observer.Wait();

  // Register a conversion with the original page as the reporting origin.
  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("registerConversion({data: 7, origin: $1})",
                               url::Origin::Create(impression_url))));

  expected_report.WaitForReport();
}

IN_PROC_BROWSER_TEST_F(
    AttributionsBrowserTest,
    ConversionOnDifferentSubdomainThanLandingPage_ReportSent) {
  // Expected reports must be registered before the server starts.
  ExpectedReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-attribution"),
      /*attribution_destination=*/"https://b.test",
      /*source_event_id=*/"1", /*source_type=*/"navigation",
      /*trigger_data=*/"7", https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  // Create an anchor tag with impression attributes and click the link. By
  // default the target is set to "_top".
  GURL conversion_landing_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/page_with_conversion_redirect.html");
  GURL conversion_dest_url = https_server()->GetURL(
      "sub.b.test",
      "/attribution_reporting/page_with_conversion_redirect.html");
  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace(R"(
    createImpressionTag({id: 'link',
                        url: $1,
                        data: '1',
                        destination: $2});)",
                               conversion_landing_url,
                               url::Origin::Create(conversion_dest_url))));

  TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(ExecJs(shell(), "simulateClick('link');"));
  observer.Wait();

  // Navigate to a same domain origin that is different than the landing page
  // for the click and convert there. A report should still be sent.
  GURL conversion_url = https_server()->GetURL(
      "other.b.test",
      "/attribution_reporting/page_with_conversion_redirect.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), conversion_url));

  // Register a conversion with the original page as the reporting origin.
  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("registerConversion({data: 7, origin: $1})",
                               url::Origin::Create(impression_url))));

  expected_report.WaitForReport();
}

IN_PROC_BROWSER_TEST_F(
    AttributionsBrowserTest,
    MultipleImpressionsPerConversion_ReportSentWithAttribution) {
  ExpectedReportWaiter expected_report(
      GURL("https://d.test/.well-known/attribution-reporting/"
           "report-attribution"),
      /*attribution_destination=*/"https://b.test",
      /*source_event_id=*/"2", /*source_type=*/"navigation",
      /*trigger_data=*/"7", https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL first_impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), first_impression_url));

  GURL second_impression_url = https_server()->GetURL(
      "c.test", "/attribution_reporting/page_with_impression_creator.html");
  Shell* shell2 =
      Shell::CreateNewWindow(shell()->web_contents()->GetBrowserContext(),
                             GURL(), nullptr, gfx::Size(100, 100));
  EXPECT_TRUE(NavigateToURL(shell2->web_contents(), second_impression_url));

  // Register impressions from both windows.
  GURL conversion_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/page_with_conversion_redirect.html");
  url::Origin reporting_origin =
      url::Origin::Create(https_server()->GetURL("d.test", "/"));
  std::string impression_js = R"(
    createImpressionTag({id: 'link',
                        url: $1,
                        data: $2,
                        destination: $3,
                        reportOrigin: $4});)";

  TestNavigationObserver first_nav_observer(shell()->web_contents());
  EXPECT_TRUE(
      ExecJs(shell(),
             JsReplace(impression_js, conversion_url, "1" /* impression_data */,
                       url::Origin::Create(conversion_url), reporting_origin)));
  EXPECT_TRUE(ExecJs(shell(), "simulateClick('link');"));
  first_nav_observer.Wait();

  TestNavigationObserver second_nav_observer(shell2->web_contents());
  EXPECT_TRUE(
      ExecJs(shell2,
             JsReplace(impression_js, conversion_url, "2" /* impression_data */,
                       url::Origin::Create(conversion_url), reporting_origin)));
  EXPECT_TRUE(ExecJs(shell2, "simulateClick('link');"));
  second_nav_observer.Wait();

  // Register a conversion after both impressions have been registered.
  EXPECT_TRUE(
      ExecJs(shell2, JsReplace("registerConversion({data: 7, origin: $1})",
                               reporting_origin)));

  expected_report.WaitForReport();
}

IN_PROC_BROWSER_TEST_F(
    AttributionsBrowserTest,
    MultipleImpressionsPerConversion_ReportSentWithHighestPriority) {
  // Report will be sent for the impression with highest priority.
  ExpectedReportWaiter expected_report(
      GURL("https://d.test/.well-known/attribution-reporting/"
           "report-attribution"),
      /*attribution_destination=*/"https://b.test",
      /*source_event_id=*/"1", /*source_type=*/"navigation",
      /*trigger_data=*/"7", https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL first_impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), first_impression_url));

  GURL second_impression_url = https_server()->GetURL(
      "c.test", "/attribution_reporting/page_with_impression_creator.html");
  Shell* shell2 =
      Shell::CreateNewWindow(shell()->web_contents()->GetBrowserContext(),
                             GURL(), nullptr, gfx::Size(100, 100));
  EXPECT_TRUE(NavigateToURL(shell2->web_contents(), second_impression_url));

  // Register impressions from both windows.
  GURL conversion_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/page_with_conversion_redirect.html");
  url::Origin reporting_origin =
      url::Origin::Create(https_server()->GetURL("d.test", "/"));
  std::string impression_js = R"(
    createImpressionTag({id: 'link',
                        url: $1,
                        data: $2,
                        destination: $3,
                        reportOrigin: $4,
                        priority: $5});)";

  TestNavigationObserver first_nav_observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(shell(), JsReplace(impression_js, conversion_url,
                                        "1" /* impression_data */,
                                        url::Origin::Create(conversion_url),
                                        reporting_origin, 10 /* priority */)));
  EXPECT_TRUE(ExecJs(shell(), "simulateClick('link');"));
  first_nav_observer.Wait();

  TestNavigationObserver second_nav_observer(shell2->web_contents());
  EXPECT_TRUE(ExecJs(shell2, JsReplace(impression_js, conversion_url,
                                       "2" /* impression_data */,
                                       url::Origin::Create(conversion_url),
                                       reporting_origin, 5 /* priority */)));
  EXPECT_TRUE(ExecJs(shell2, "simulateClick('link');"));
  second_nav_observer.Wait();

  // Register a conversion after both impressions have been registered.
  EXPECT_TRUE(
      ExecJs(shell2, JsReplace("registerConversion({data: 7, origin: $1})",
                               reporting_origin)));
  expected_report.WaitForReport();
}

IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
                       ConversionRegisteredWithEmbedderDisallow_NoData) {
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&disallowed_browser_client_);

  // Expected reports must be registered before the server starts.
  ExpectedReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-attribution"),
      /*body=*/base::Value(), https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  // Create an anchor tag with impression attributes and click the link. By
  // default the target is set to "_top".
  GURL conversion_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/page_with_conversion_redirect.html");
  EXPECT_TRUE(
      ExecJs(web_contents(),
             JsReplace(R"(
    createImpressionTag({id: 'link',
                        url: $1,
                        data: '1',
                        destination: $2});)",
                       conversion_url, url::Origin::Create(conversion_url))));

  TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(ExecJs(shell(), "simulateClick('link');"));
  observer.Wait();

  // Register a conversion with the original page as the reporting origin.
  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("registerConversion({data: 7, origin: $1})",
                               url::Origin::Create(impression_url))));

  // Since we want to verify that a report _isn't_ sent, we can't really wait on
  // any event here. The best thing we can do is just impose a short delay and
  // verify the browser didn't send anything. Worst case, this should start
  // flakily failing if the logic breaks.
  base::RunLoop run_loop;
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(100));
  run_loop.Run();
  EXPECT_FALSE(expected_report.HasRequest());

  SetBrowserClientForTesting(old_browser_client);
}

IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
                       EventSourceImpressionConversion_ReportSent) {
  // Expected reports must be registered before the server starts.
  // 123 in the `registerConversion` call below is sanitized to 1 in
  // the report's `trigger_data`.
  ExpectedReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-attribution"),
      /*attribution_destination=*/"https://b.test",
      /*source_event_id=*/"7", /*source_type=*/"event", /*trigger_data=*/"1",
      https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  // Create an anchor tag with impression attributes.
  GURL conversion_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/page_with_conversion_redirect.html");
  EXPECT_TRUE(
      ExecJs(web_contents(),
             JsReplace(R"(
    createImpressionTag({id: 'link',
                        url: $1,
                        data: '7',
                        destination: $2,
                        registerAttributionSource: true});)",
                       conversion_url, url::Origin::Create(conversion_url))));

  EXPECT_TRUE(NavigateToURL(web_contents(), conversion_url));

  // Register a conversion with the original page as the reporting origin.
  EXPECT_TRUE(
      ExecJs(web_contents(), JsReplace(R"(registerConversion({data: 0,
                                       origin: $1,
                                       eventSourceTriggerData: 123});)",
                                       url::Origin::Create(impression_url))));

  expected_report.WaitForReport();
}

IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
                       EventSourceImpressionTwoConversions_OneReportSent) {
  // Expected reports must be registered before the server starts.
  // 123 in the `registerConversion` call below is sanitized to 1 in
  // the report's `trigger_data`.
  ExpectedReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-attribution"),
      /*attribution_destination=*/"https://b.test",
      /*source_event_id=*/"7", /*source_type=*/"event", /*trigger_data=*/"1",
      https_server());
  ExpectedReportWaiter expected_report_not_sent(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-attribution"),
      /*body=*/base::Value(), https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  // Create an anchor tag with impression attributes.
  GURL conversion_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/page_with_conversion_redirect.html");
  EXPECT_TRUE(
      ExecJs(web_contents(),
             JsReplace(R"(
    createImpressionTag({id: 'link',
                        url: $1,
                        data: '7',
                        destination: $2,
                        registerAttributionSource: true});)",
                       conversion_url, url::Origin::Create(conversion_url))));

  EXPECT_TRUE(NavigateToURL(web_contents(), conversion_url));

  // Register two conversions with the original page as the reporting origin.
  for (int i = 0; i < 2; i++) {
    EXPECT_TRUE(
        ExecJs(web_contents(), JsReplace(R"(registerConversion({data: 0,
                                       origin: $1,
                                       eventSourceTriggerData: 123});)",
                                         url::Origin::Create(impression_url))));
  }

  expected_report.WaitForReport();

  // Since we want to verify that a report _isn't_ sent, we can't really wait on
  // any event here. The best thing we can do is just impose a short delay and
  // verify the browser didn't send anything. Worst case, this should start
  // flakily failing if the logic breaks.
  base::RunLoop run_loop;
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(100));
  run_loop.Run();
  EXPECT_FALSE(expected_report_not_sent.HasRequest());
}

IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
                       EventSourceImpressionConversionFromJS_ReportSent) {
  // Expected reports must be registered before the server starts.
  // 123 in the `registerConversion` call below is sanitized to 1 in
  // the report's `trigger_data`.
  ExpectedReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-attribution"),
      /*attribution_destination=*/"https://b.test",
      /*source_event_id=*/"7", /*source_type=*/"event", /*trigger_data=*/"1",
      https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  GURL conversion_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/page_with_conversion_redirect.html");
  EXPECT_TRUE(
      ExecJs(web_contents(), JsReplace(R"(
              window.attributionReporting.registerAttributionSource({
                attributionSourceEventId: "7",
                attributionDestination: $1,
              });)",
                                       url::Origin::Create(conversion_url))));

  EXPECT_TRUE(NavigateToURL(web_contents(), conversion_url));

  // Register a conversion with the original page as the reporting origin.
  EXPECT_TRUE(
      ExecJs(web_contents(), JsReplace(R"(registerConversion({data: 0,
                                       origin: $1,
                                       eventSourceTriggerData: 123});)",
                                       url::Origin::Create(impression_url))));

  expected_report.WaitForReport();
}

IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
                       ImpressionConversionWithDedupKey_Deduped) {
  // Expected reports must be registered before the server starts.
  ExpectedReportWaiter expected_report1(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-attribution"),
      /*attribution_destination=*/"https://b.test",
      /*source_event_id=*/"1", /*source_type=*/"navigation",
      /*trigger_data=*/"7", https_server());
  // 12 below is sanitized to 4 here by the `AttributionPolicy`.
  ExpectedReportWaiter expected_report2(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-attribution"),
      /*attribution_destination=*/"https://b.test",
      /*source_event_id=*/"1", /*source_type=*/"navigation",
      /*trigger_data=*/"4", https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  // Create an anchor tag with impression attributes and click the link. By
  // default the target is set to "_top".
  GURL conversion_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/page_with_conversion_redirect.html");
  EXPECT_TRUE(
      ExecJs(web_contents(),
             JsReplace(R"(
    createImpressionTag({id: 'link',
                        url: $1,
                        data: '1',
                        destination: $2});)",
                       conversion_url, url::Origin::Create(conversion_url))));

  TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(ExecJs(shell(), "simulateClick('link');"));
  observer.Wait();

  EXPECT_TRUE(
      ExecJs(web_contents(), JsReplace(R"(registerConversion({data: 7,
                                       origin: $1,
                                       dedupKey: 123});)",
                                       url::Origin::Create(impression_url))));

  expected_report1.WaitForReport();

  // This report should be deduped against the previous one.
  EXPECT_TRUE(
      ExecJs(web_contents(), JsReplace(R"(registerConversion({data: 9,
                                       origin: $1,
                                       dedupKey: 123});)",
                                       url::Origin::Create(impression_url))));

  // This report should be received, as it has a different dedupKey.
  EXPECT_TRUE(
      ExecJs(web_contents(), JsReplace(R"(registerConversion({data: 12,
                                       origin: $1,
                                       dedupKey: 456});)",
                                       url::Origin::Create(impression_url))));

  expected_report2.WaitForReport();
}

class AttributionsPrerenderBrowserTest : public AttributionsBrowserTest {
 public:
  AttributionsPrerenderBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&AttributionsBrowserTest::web_contents,
                                base::Unretained(this))) {}
  ~AttributionsPrerenderBrowserTest() override = default;

 protected:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(AttributionsPrerenderBrowserTest,
                       NoImpressionRegisteredOnPrerender) {
  ExpectedReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-attribution"),
      /*attribution_destination=*/"https://b.test",
      /*source_event_id=*/"7", /*source_type=*/"event", /*trigger_data=*/"1",
      https_server());
  ASSERT_TRUE(https_server()->Start());

  // Navigate to a starting page in the same origin.
  const GURL kInitialUrl = https_server()->GetURL("a.test", "/empty.html");
  {
    auto url_loader_interceptor =
        content::URLLoaderInterceptor::ServeFilesFromDirectoryAtOrigin(
            kBaseDataDir, kInitialUrl.DeprecatedGetOriginAsURL());
    EXPECT_TRUE(NavigateToURL(web_contents(), kInitialUrl));
  }

  // Pre-render a page with a impression creator.
  const GURL kImpressionUrl = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  int host_id = prerender_helper_.AddPrerender(kImpressionUrl);
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);

  prerender_helper_.WaitForPrerenderLoadCompletion(kImpressionUrl);
  content::RenderFrameHost* prerender_rfh =
      prerender_helper_.GetPrerenderedMainFrameHost(host_id);

  const GURL kConversionUrl = https_server()->GetURL(
      "b.test", "/attribution_reporting/page_with_conversion_redirect.html");

  // Register impression during pre-rendering for the target conversion url.
  EXPECT_TRUE(
      ExecJs(prerender_rfh, JsReplace(R"(
              window.attributionReporting.registerAttributionSource({
                attributionSourceEventId: "7",
                attributionDestination: $1,
              });)",
                                      url::Origin::Create(kConversionUrl))));

  // Navigate to the conversion url.
  EXPECT_TRUE(NavigateToURL(web_contents(), kConversionUrl));

  // Register a conversion with the original page as the reporting origin.
  EXPECT_TRUE(
      ExecJs(web_contents(), JsReplace(R"(registerConversion({data: 0,
                                       origin: $1,
                                       eventSourceTriggerData: 123});)",
                                       url::Origin::Create(kImpressionUrl))));

  // Verify that registering a conversion had no effect on reports, as the
  // impressions were never passed to the conversion URL, as the page was only
  // pre-rendered.
  base::RunLoop run_loop;
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(100));
  EXPECT_FALSE(expected_report.HasRequest());
}

IN_PROC_BROWSER_TEST_F(AttributionsPrerenderBrowserTest,
                       ImpressionRegisteredOnActivatedPrerender) {
  ExpectedReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-attribution"),
      /*attribution_destination=*/"https://b.test",
      /*source_event_id=*/"7", /*source_type=*/"event", /*trigger_data=*/"1",
      https_server());
  ASSERT_TRUE(https_server()->Start());

  // Navigate to a starting page in the same origin.
  const GURL kInitialUrl = https_server()->GetURL("a.test", "/empty.html");
  {
    auto url_loader_interceptor =
        content::URLLoaderInterceptor::ServeFilesFromDirectoryAtOrigin(
            kBaseDataDir, kInitialUrl.DeprecatedGetOriginAsURL());
    EXPECT_TRUE(NavigateToURL(web_contents(), kInitialUrl));
  }

  // Pre-render a page with a impression creator.
  const GURL kImpressionUrl = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  int host_id = prerender_helper_.AddPrerender(kImpressionUrl);
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);

  prerender_helper_.WaitForPrerenderLoadCompletion(kImpressionUrl);
  content::RenderFrameHost* prerender_rfh =
      prerender_helper_.GetPrerenderedMainFrameHost(host_id);

  const GURL kConversionUrl = https_server()->GetURL(
      "b.test", "/attribution_reporting/page_with_conversion_redirect.html");

  // Register impression during pre-rendering for the target conversion url.
  EXPECT_TRUE(
      ExecJs(prerender_rfh, JsReplace(R"(
              window.attributionReporting.registerAttributionSource({
                attributionSourceEventId: "7",
                attributionDestination: $1,
              });)",
                                      url::Origin::Create(kConversionUrl))));

  // Navigate to pre-rendered page, bringing it to the fore.
  prerender_helper_.NavigatePrimaryPage(kImpressionUrl);
  ASSERT_EQ(kImpressionUrl, web_contents()->GetLastCommittedURL());

  // Navigate to the conversion url.
  EXPECT_TRUE(NavigateToURL(web_contents(), kConversionUrl));

  // Register a conversion with the original page as the reporting origin.
  EXPECT_TRUE(
      ExecJs(web_contents(), JsReplace(R"(registerConversion({data: 0,
                                       origin: $1,
                                       eventSourceTriggerData: 123});)",
                                       url::Origin::Create(kImpressionUrl))));

  // Confirm that reports work as expected, and impressions were retrieved from
  // the pre-rendered page, once it became a primary page.
  expected_report.WaitForReport();
}

IN_PROC_BROWSER_TEST_F(AttributionsPrerenderBrowserTest,
                       NoConversionsOnPrerender) {
  ExpectedReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-attribution"),
      /*attribution_destination=*/"https://b.test",
      /*source_event_id=*/"7", /*source_type=*/"event", /*trigger_data=*/"1",
      https_server());
  ASSERT_TRUE(https_server()->Start());

  // Navigate to a page with impression creator.
  const GURL kImpressionUrl = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), kImpressionUrl));

  const GURL kConversionUrl = https_server()->GetURL(
      "b.test", "/attribution_reporting/page_with_conversion_redirect.html");

  // Register impression for the target conversion url.
  EXPECT_TRUE(
      ExecJs(web_contents(), JsReplace(R"(
                window.attributionReporting.registerAttributionSource({
                  attributionSourceEventId: "7",
                  attributionDestination: $1,
                });)",
                                       url::Origin::Create(kConversionUrl))));

  // Navigate to a starting same origin page with the conversion url.
  const GURL kEmptyUrl = https_server()->GetURL("b.test", "/empty.html");
  {
    auto url_loader_interceptor =
        content::URLLoaderInterceptor::ServeFilesFromDirectoryAtOrigin(
            kBaseDataDir, kEmptyUrl.DeprecatedGetOriginAsURL());
    EXPECT_TRUE(NavigateToURL(web_contents(), kEmptyUrl));
  }

  // Pre-render the conversion url.
  int host_id = prerender_helper_.AddPrerender(kConversionUrl);
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);

  prerender_helper_.WaitForPrerenderLoadCompletion(kConversionUrl);
  content::RenderFrameHost* prerender_rfh =
      prerender_helper_.GetPrerenderedMainFrameHost(host_id);

  // Register a conversion with the original page as the reporting origin during
  // pre-rendering.
  EXPECT_TRUE(
      ExecJs(prerender_rfh, JsReplace(R"(registerConversion({data: 0,
                                         origin: $1,
                                         eventSourceTriggerData: 123});)",
                                      url::Origin::Create(kImpressionUrl))));

  // Verify that registering a conversion had no effect on reports, as the
  // impressions were never passed to the conversion URL, as the page was only
  // pre-rendered.
  base::RunLoop run_loop;
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(100));
  EXPECT_FALSE(expected_report.HasRequest());
}

IN_PROC_BROWSER_TEST_F(AttributionsPrerenderBrowserTest,
                       ConversionsRegisteredOnActivatedPrerender) {
  ExpectedReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-attribution"),
      /*attribution_destination=*/"https://b.test",
      /*source_event_id=*/"7", /*source_type=*/"event", /*trigger_data=*/"1",
      https_server());
  ASSERT_TRUE(https_server()->Start());

  // Navigate to a page with impression creator.
  const GURL kImpressionUrl = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), kImpressionUrl));

  // Register impression for the target conversion url.
  const GURL kConversionUrl = https_server()->GetURL(
      "b.test", "/attribution_reporting/page_with_conversion_redirect.html");
  EXPECT_TRUE(
      ExecJs(web_contents(), JsReplace(R"(
                window.attributionReporting.registerAttributionSource({
                  attributionSourceEventId: "7",
                  attributionDestination: $1,
                });)",
                                       url::Origin::Create(kConversionUrl))));

  // Navigate to a starting same origin page with the conversion url.
  const GURL kEmptyUrl = https_server()->GetURL("b.test", "/empty.html");
  {
    auto url_loader_interceptor =
        content::URLLoaderInterceptor::ServeFilesFromDirectoryAtOrigin(
            kBaseDataDir, kEmptyUrl.DeprecatedGetOriginAsURL());
    EXPECT_TRUE(NavigateToURL(web_contents(), kEmptyUrl));
  }

  // Pre-render the conversion url.
  int host_id = prerender_helper_.AddPrerender(kConversionUrl);
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);

  prerender_helper_.WaitForPrerenderLoadCompletion(kConversionUrl);
  content::RenderFrameHost* prerender_rfh =
      prerender_helper_.GetPrerenderedMainFrameHost(host_id);

  // Register a conversion with the original page as the reporting origin.
  EXPECT_TRUE(
      ExecJs(prerender_rfh, JsReplace(R"(registerConversion({data: 0,
                                         origin: $1,
                                         eventSourceTriggerData: 123});)",
                                      url::Origin::Create(kImpressionUrl))));

  // Navigate to pre-rendered page, bringing it to the fore.
  prerender_helper_.NavigatePrimaryPage(kConversionUrl);
  ASSERT_EQ(kConversionUrl, web_contents()->GetLastCommittedURL());

  // Confirm that reports work as expected, and impressions were retrieved from
  // the pre-rendered page, once it became a primary page.
  expected_report.WaitForReport();
}

}  // namespace content
