// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/gfx/geometry/point.h"
#include "url/gurl.h"

namespace content {

namespace {

using ::testing::Field;

// WebContentsObserver that waits until a source is available on a
// navigation handle for a finished navigation.
class SourceObserver : public TestNavigationObserver {
 public:
  explicit SourceObserver(WebContents* contents, size_t num_impressions = 1u)
      : TestNavigationObserver(contents),
        expected_num_impressions_(num_impressions) {}

  // WebContentsObserver
  void OnDidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (!navigation_handle->GetImpression()) {
      if (waiting_for_null_impression_)
        impression_loop_.Quit();
      return;
    }

    last_impression_ = *(navigation_handle->GetImpression());
    num_impressions_++;

    if (!waiting_for_null_impression_ &&
        num_impressions_ >= expected_num_impressions_) {
      impression_loop_.Quit();
    }
  }

  const blink::Impression& last_impression() { return *last_impression_; }

  // Waits for |expected_num_impressions_| navigations with impressions, and
  // returns the last impression.
  const blink::Impression& Wait() {
    if (num_impressions_ >= expected_num_impressions_)
      return *last_impression_;
    impression_loop_.Run();
    return last_impression();
  }

  bool WaitForNavigationWithNoImpression() {
    waiting_for_null_impression_ = true;
    impression_loop_.Run();
    waiting_for_null_impression_ = false;
    return true;
  }

 private:
  size_t num_impressions_ = 0u;
  const size_t expected_num_impressions_ = 0u;
  absl::optional<blink::Impression> last_impression_;
  bool waiting_for_null_impression_ = false;
  base::RunLoop impression_loop_;
};

class AttributionSourceDisabledBrowserTest : public ContentBrowserTest {
 public:
  AttributionSourceDisabledBrowserTest() {
    AttributionManagerImpl::RunInMemoryForTesting();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "content/test/data/attribution_reporting");
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    net::test_server::RegisterDefaultHandlers(https_server_.get());
    https_server_->ServeFilesFromSourceDirectory(
        "content/test/data/attribution_reporting");
    https_server_->ServeFilesFromSourceDirectory("content/test/data");
    SetupCrossSiteRedirector(https_server_.get());
    ASSERT_TRUE(https_server_->Start());
  }

  WebContents* web_contents() { return shell()->web_contents(); }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

}  // namespace

// Verifies that impressions are not logged when the Runtime feature isn't
// enabled.
IN_PROC_BROWSER_TEST_F(AttributionSourceDisabledBrowserTest,
                       ImpressionWithoutFeatureEnabled_NotReceived) {
  SourceObserver source_observer(web_contents());
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createAndClickImpressionTag({url: 'page_with_conversion_redirect.html',
                        data: '1',
                        destination: 'https://a.com'});)"));

  // No impression should be observed.
  EXPECT_TRUE(source_observer.WaitForNavigationWithNoImpression());
}

class AttributionSourceDeclarationBrowserTest
    : public AttributionSourceDisabledBrowserTest {
 public:
  AttributionSourceDeclarationBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets up the blink runtime feature for ConversionMeasurement.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);

    // Sets up support for event sources.
    command_line->AppendSwitch(switches::kEnableBlinkTestFeatures);
  }
};

IN_PROC_BROWSER_TEST_F(AttributionSourceDeclarationBrowserTest,
                       ImpressionTagClicked_ImpressionReceived) {
  SourceObserver source_observer(web_contents());
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  // Create an anchor tag with impression attributes and click the link. By
  // default the target is set to "_top".
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createAndClickImpressionTag({url: 'page_with_conversion_redirect.html',
                        data: '1',
                        destination: 'https://a.com',
                        reportOrigin: 'https://report.com',
                        expiry: 1000});)"));

  // Wait for the impression to be seen by the observer.
  blink::Impression last_impression = source_observer.Wait();

  // Verify the attributes of the impression are set as expected.
  EXPECT_EQ(1UL, last_impression.impression_data);
  EXPECT_EQ(url::Origin::Create(GURL("https://a.com")),
            last_impression.conversion_destination);
  EXPECT_EQ(url::Origin::Create(GURL("https://report.com")),
            last_impression.reporting_origin);
  EXPECT_EQ(base::Milliseconds(1000), *last_impression.expiry);

  // Verify default attribution source priority.
  EXPECT_EQ(0, last_impression.priority);
}

IN_PROC_BROWSER_TEST_F(AttributionSourceDeclarationBrowserTest,
                       ImpressionTagNavigatesRemoteFrame_ImpressionReceived) {
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  // Create an impression tag with a target frame that does not exist, which
  // will open a new window to navigate.
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTag({id: 'link',
                        url: 'page_with_conversion_redirect.html',
                        data: '1',
                        destination: 'https://a.com',
                        target: 'target'});)"));

  SourceObserver source_observer(nullptr);
  source_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(ExecJs(shell(), "simulateClick('link');"));

  // Wait for the impression to be seen by the observer.
  blink::Impression last_impression = source_observer.Wait();
  EXPECT_EQ(1UL, source_observer.last_impression().impression_data);
}

// Flaky: crbug.com/1077216
IN_PROC_BROWSER_TEST_F(
    AttributionSourceDeclarationBrowserTest,
    DISABLED_ImpressionTagNavigatesExistingRemoteFrame_ImpressionReceived) {
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  WebContents* initial_web_contents = web_contents();

  ShellAddedObserver new_shell_observer;
  GURL remote_url = https_server()->GetURL("c.test", "/title1.html");
  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("window.open($1, 'target');", remote_url)));

  // Get the new web contents associated with the remote frame.
  WebContents* remote_web_contents =
      new_shell_observer.GetShell()->web_contents();

  // Click on the impression and target the existing remote frame.
  EXPECT_TRUE(ExecJs(initial_web_contents, R"(
    createImpressionTag(id: 'link',
                        url: 'page_with_conversion_redirect.html',
                        data: '1',
                        destination: 'https://a.com',
                        target: 'target'});)"));

  SourceObserver source_observer(remote_web_contents);
  EXPECT_TRUE(ExecJs(shell(), "simulateClick('link');"));

  // Wait for the impression to be seen by the observer.
  blink::Impression last_impression = source_observer.Wait();
  EXPECT_EQ(1UL, source_observer.last_impression().impression_data);
}

IN_PROC_BROWSER_TEST_F(AttributionSourceDeclarationBrowserTest,
                       ImpressionTagWithOutOfBoundData_DefaultedTo0) {
  SourceObserver source_observer(web_contents());
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  // The provided data underflows an unsigned 64 bit int, and should be handled
  // properly.
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createAndClickImpressionTag({url: 'page_with_conversion_redirect.html',
                        data: '-1',
                        destination: 'https://a.com'});)"));

  // Wait for the impression to be seen by the observer.
  blink::Impression last_impression = source_observer.Wait();
  EXPECT_EQ(0UL, source_observer.last_impression().impression_data);
}

IN_PROC_BROWSER_TEST_F(
    AttributionSourceDeclarationBrowserTest,
    ImpressionTagNavigatesFromMiddleClick_ImpressionReceived) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  // Create an impression tag that is opened via middle click. This navigates in
  // a new WebContents.
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTag({id: 'link',
                        url: 'page_with_conversion_redirect.html',
                        data: '1',
                        destination: 'https://a.com'});)"));

  SourceObserver source_observer(nullptr);
  source_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(ExecJs(shell(), "simulateMiddleClick(\'link\');"));

  blink::Impression last_impression = source_observer.Wait();

  // Verify the attributes of the impression are set as expected.
  EXPECT_EQ(1UL, last_impression.impression_data);
}

// See https://crbug.com/1186077.
IN_PROC_BROWSER_TEST_F(
    AttributionSourceDeclarationBrowserTest,
    TagNavigatesFromMiddleClickInSubframe_ImpressionReceived) {
  GURL page_url = https_server()->GetURL("b.test", "/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));
  EXPECT_TRUE(ExecJs(shell(), R"(
     let frame = document.getElementById('test_iframe');
     frame.setAttribute('allow', 'attribution-reporting');)"));

  GURL subframe_url =
      https_server()->GetURL("c.test", "/page_with_impression_creator.html");
  NavigateIframeToURL(web_contents(), "test_iframe", subframe_url);

  // Create an impression tag that is opened via middle click in the subframe.
  RenderFrameHost* subframe = ChildFrameAt(web_contents()->GetMainFrame(), 0);
  EXPECT_TRUE(ExecJs(subframe, R"(
    createImpressionTag({id: 'link',
                        url: 'page_with_conversion_redirect.html',
                        data: '1',
                        destination: 'https://a.com'});)"));

  SourceObserver source_observer(nullptr);
  source_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(ExecJs(subframe, "simulateMiddleClick(\'link\');"));

  // Verify the navigation was annotated with an impression.
  blink::Impression last_impression = source_observer.Wait();
  EXPECT_EQ(1UL, last_impression.impression_data);
}

// https://crbug.com/1219907 started flaking after Field Trial Testing Config
// was enabled for content_browsertests.
IN_PROC_BROWSER_TEST_F(
    AttributionSourceDeclarationBrowserTest,
    DISABLED_ImpressionTagNavigatesFromEnterPress_ImpressionReceived) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTag({id: 'link',
                        url: 'page_with_conversion_redirect.html',
                        data: '1",
                        destination: 'https://a.com'});)"));

  // Focus the element, wait for it to receive focus, and simulate an enter
  // press.
  std::u16string expected_title = u"focused";
  content::TitleWatcher title_watcher(web_contents(), expected_title);
  EXPECT_TRUE(ExecJs(shell(), R"(
    let link = document.getElementById('link');
    link.addEventListener('focus', function() { document.title = 'focused'; });
    link.focus();)"));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  SourceObserver source_observer(web_contents());
  content::SimulateKeyPress(web_contents(), ui::DomKey::ENTER,
                            ui::DomCode::ENTER, ui::VKEY_RETURN, false, false,
                            false, false);

  blink::Impression last_impression = source_observer.Wait();

  // Verify the attributes of the impression are set as expected.
  EXPECT_EQ(1UL, last_impression.impression_data);
}

IN_PROC_BROWSER_TEST_F(AttributionSourceDeclarationBrowserTest,
                       ImpressionOnInsecureSite_NotRegistered) {
  // Navigate to a page with the non-https server.
  EXPECT_TRUE(NavigateToURL(
      web_contents(), embedded_test_server()->GetURL(
                          "b.test", "/page_with_impression_creator.html")));

  SourceObserver source_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createAndClickImpressionTag({url: 'page_with_conversion_redirect.html',
                        data: '1',
                        destination: 'https://a.com'});)"));

  // We should see a null impression on the navigation
  EXPECT_TRUE(source_observer.WaitForNavigationWithNoImpression());
}

IN_PROC_BROWSER_TEST_F(AttributionSourceDeclarationBrowserTest,
                       ImpressionWithInsecureDestination_NotRegistered) {
  // Navigate to a page with the non-https server.
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  SourceObserver source_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createAndClickImpressionTag({url: 'page_with_conversion_redirect.html',
                        data: '1',
                        destination: 'http://a.com'});)"));

  // We should see a null impression on the navigation
  EXPECT_TRUE(source_observer.WaitForNavigationWithNoImpression());
}

IN_PROC_BROWSER_TEST_F(AttributionSourceDeclarationBrowserTest,
                       ImpressionWithInsecureReportingOrigin_NotRegistered) {
  // Navigate to a page with the non-https server.
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  SourceObserver source_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createAndClickImpressionTag({url: 'page_with_conversion_redirect.html',
                        data: '1',
                        destination: 'https://a.com',
                        reportOrigin: 'http://reporting.com',
                        expiry: 1000});)"));

  // We should see a null impression on the navigation
  EXPECT_TRUE(source_observer.WaitForNavigationWithNoImpression());
}

IN_PROC_BROWSER_TEST_F(AttributionSourceDeclarationBrowserTest,
                       ImpressionWithPermissionsPolicyDisabled_NotRegistered) {
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL(
          "b.test", "/page_with_conversion_measurement_disabled.html")));

  SourceObserver source_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createAndClickImpressionTag({url: 'page_with_conversion_redirect.html',
                        data: '1',
                        destination: 'https://a.com'});)"));

  // We should see a null impression on the navigation
  EXPECT_TRUE(source_observer.WaitForNavigationWithNoImpression());
}

IN_PROC_BROWSER_TEST_F(
    AttributionSourceDeclarationBrowserTest,
    ImpressionInSubframeWithoutPermissionsPolicy_NotRegistered) {
  GURL page_url = https_server()->GetURL("b.test", "/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  GURL subframe_url =
      https_server()->GetURL("c.test", "/page_with_impression_creator.html");
  NavigateIframeToURL(web_contents(), "test_iframe", subframe_url);

  SourceObserver source_observer(web_contents());
  RenderFrameHost* subframe = ChildFrameAt(web_contents()->GetMainFrame(), 0);
  EXPECT_TRUE(ExecJs(subframe, R"(
    createAndClickImpressionTag({url: 'page_with_conversion_redirect.html',
                        data: '1',
                        destination: 'https://a.com'});)"));

  // We should see a null impression on the navigation
  EXPECT_TRUE(source_observer.WaitForNavigationWithNoImpression());
}

IN_PROC_BROWSER_TEST_F(AttributionSourceDeclarationBrowserTest,
                       ImpressionInSubframeWithPermissionsPolicy_Registered) {
  GURL page_url = https_server()->GetURL("b.test", "/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));
  EXPECT_TRUE(ExecJs(shell(), R"(
     let frame = document.getElementById('test_iframe');
     frame.setAttribute('allow', 'attribution-reporting');)"));

  GURL subframe_url =
      https_server()->GetURL("c.test", "/page_with_impression_creator.html");
  NavigateIframeToURL(web_contents(), "test_iframe", subframe_url);

  SourceObserver source_observer(web_contents());
  RenderFrameHost* subframe = ChildFrameAt(web_contents()->GetMainFrame(), 0);
  EXPECT_TRUE(ExecJs(subframe, R"(
    createAndClickImpressionTag({url: 'page_with_conversion_redirect.html',
                        data: '1',
                        destination: 'https://a.com'});)"));

  // We should see a null impression on the navigation
  EXPECT_EQ(1u, source_observer.Wait().impression_data);
}

// Tests that when a context menu is shown, there is an impression attached to
// the ContextMenu data forwarded to the browser process.

// TODO(johnidel): SimulateMouseClickAt() does not work on Android, find a
// different way to invoke the context menu that works on Android.
#if !defined(OS_ANDROID)
// https://crbug.com/1219907 started flaking after Field Trial Testing Config
// was enabled for content_browsertests.
IN_PROC_BROWSER_TEST_F(AttributionSourceDeclarationBrowserTest,
                       DISABLED_ContextMenuShownForImpression_ImpressionSet) {
  // Navigate to a page with the non-https server.
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTag({id: 'link',
                        url: 'page_with_conversion_redirect.html',
                        data: '10',
                        destination: 'https://dest.com',
                        left: 100,
                        top: 100});)"));

  auto context_menu_interceptor =
      std::make_unique<content::ContextMenuInterceptor>(
          web_contents()->GetMainFrame(),
          ContextMenuInterceptor::ShowBehavior::kPreventShow);

  content::SimulateMouseClickAt(web_contents(), 0,
                                blink::WebMouseEvent::Button::kRight,
                                gfx::Point(100, 100));

  context_menu_interceptor->Wait();
  blink::UntrustworthyContextMenuParams params =
      context_menu_interceptor->get_params();
  EXPECT_TRUE(params.impression);
  EXPECT_EQ(10UL, params.impression->impression_data);
  EXPECT_EQ(url::Origin::Create(GURL("https://dest.com")),
            params.impression->conversion_destination);
}
#endif  // !defined(OS_ANDROID)

IN_PROC_BROWSER_TEST_F(AttributionSourceDeclarationBrowserTest,
                       ImpressionNavigationReloads_NoImpression) {
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  SourceObserver source_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createAndClickImpressionTag({url: 'page_with_conversion_redirect.html',
                        data: '1',
                        destination: 'https://a.com'});)"));
  EXPECT_EQ(1UL, source_observer.Wait().impression_data);

  SourceObserver reload_observer(web_contents());
  shell()->Reload();

  // The reload navigation should not have an impression set.
  EXPECT_TRUE(reload_observer.WaitForNavigationWithNoImpression());
}

// Same as the above test but via a renderer initiated reload.
IN_PROC_BROWSER_TEST_F(AttributionSourceDeclarationBrowserTest,
                       RendererReloadImpressionNavigation_NoImpression) {
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  SourceObserver source_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createAndClickImpressionTag({url: 'page_with_conversion_redirect.html',
                        data: '1',
                        destination: 'https://a.com'});)"));
  EXPECT_EQ(1UL, source_observer.Wait().impression_data);

  SourceObserver reload_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), "window.location.reload()"));

  // The reload navigation should not have an impression set.
  EXPECT_TRUE(reload_observer.WaitForNavigationWithNoImpression());
}

IN_PROC_BROWSER_TEST_F(AttributionSourceDeclarationBrowserTest,
                       BackNavigateToImpressionNavigation_NoImpression) {
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  SourceObserver source_observer(web_contents());

  // Click the default impression on the page.
  EXPECT_TRUE(ExecJs(shell(), "simulateClick('impression_tag');"));
  EXPECT_EQ(1UL, source_observer.Wait().impression_data);

  // Navigate away so we can back navigate to the impression's navigated page.
  EXPECT_TRUE(NavigateToURL(web_contents(), GURL("about:blank")));

  // The back navigation should not have an impression set.
  SourceObserver back_nav_observer(web_contents());
  shell()->GoBackOrForward(-1);
  EXPECT_TRUE(back_nav_observer.WaitForNavigationWithNoImpression());

  // Navigate back to the original page and ensure subsequent clicks also log
  // impressions.
  SourceObserver second_back_nav_observer(web_contents());
  shell()->GoBackOrForward(-1);
  EXPECT_TRUE(second_back_nav_observer.WaitForNavigationWithNoImpression());

  // Wait for the page to load and render the impression tag.
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  SourceObserver second_impression_observer(web_contents());
  EXPECT_TRUE(ExecJs(shell(), "simulateClick('impression_tag');"));
  EXPECT_EQ(1UL, second_impression_observer.Wait().impression_data);
}

IN_PROC_BROWSER_TEST_F(
    AttributionSourceDeclarationBrowserTest,
    ImpressionTagNavigatesCurrentFrame_ImpressionPageMetrics) {
  base::HistogramTester histograms;

  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  // Create an anchor tag with impression attributes and click the link. By
  // default the target is set to "_top".
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createAndClickImpressionTag({url: 'page_with_impression_creator.html',
                        data: '1',
                        destination: 'https://a.com',
                        reportOrigin: 'https://example1.test'});)"));

  WaitForLoadStop(web_contents());

  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createAndClickImpressionTag({url: 'page_with_impression_creator.html',
                        data: '2',
                        destination: 'https://a.com',
                        reortOrigin: 'https://example2.test'});)"));

  WaitForLoadStop(web_contents());

  // Navigate away to have the data captured.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  histograms.ExpectBucketCount("Conversions.RegisteredImpressionsPerPage", 1,
                               2);
  histograms.ExpectBucketCount(
      "Conversions.UniqueReportingOriginsPerPage.Impressions", 1, 2);
}

IN_PROC_BROWSER_TEST_F(
    AttributionSourceDeclarationBrowserTest,
    ImpressionTagNavigatesRemoteFrame_ImpressionPageMetrics) {
  base::HistogramTester histograms;

  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  // Create an impression tag with a target frame that does not exist, which
  // will open a new window to navigate.
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createAndClickImpressionTag({url: 'page_with_conversion_redirect.html',
                        data: '1',
                        destination: 'https://a.com',
                        reportOrigin: 'https://example1.test',
                        target: 'target'});)"));

  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createAndClickImpressionTag({url: 'page_with_conversion_redirect.html',
                        data: '2',
                        destination: 'https://a.com',
                        reportOrigin: 'https://example2.test',
                        target: 'target'});)"));

  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createAndClickImpressionTag({url: 'page_with_conversion_redirect.html',
                        data: '3',
                        destination: 'https://a.com',
                        reportOrigin: 'https://example1.test',
                        target: 'target'});)"));

  // Navigate away to have the data captured.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  histograms.ExpectBucketCount("Conversions.RegisteredImpressionsPerPage", 3,
                               1);
  histograms.ExpectBucketCount(
      "Conversions.UniqueReportingOriginsPerPage.Impressions", 2, 1);
}

IN_PROC_BROWSER_TEST_F(
    AttributionSourceDeclarationBrowserTest,
    ImpressionTagWithRegisterAttributionSource_ImpressionReceived) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  base::RunLoop loop;
  MockAttributionHost host(web_contents());
  EXPECT_CALL(host, RegisterImpression(
                        Field(&blink::Impression::impression_data, 200UL)))
      .WillOnce([&]() { loop.Quit(); });

  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTag({id: 'link',
                        url: 'page_with_conversion_redirect.html',
                        data: '200',
                        destination: 'https://a.com',
                        registerAttributionSource: true});)"));
  loop.Run();

  EXPECT_TRUE(ExecJs(web_contents(), R"(
    document.getElementById("link").removeAttribute("registerattributionsource");)"));

  // Conversion mojo messages are sent on the same message pipe as navigation
  // messages. Because the conversion would have been sequenced prior to the
  // navigation message, it would be observed before the NavigateToURL() call
  // finishes.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
}

IN_PROC_BROWSER_TEST_F(
    AttributionSourceDeclarationBrowserTest,
    ImpressionTagWithRegisterAttributionSource_ImpressionReceivedWithNewData) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  base::RunLoop loop1, loop2;
  MockAttributionHost host(web_contents());
  EXPECT_CALL(host, RegisterImpression(
                        Field(&blink::Impression::impression_data, 200UL)))
      .WillOnce([&]() { loop1.Quit(); });
  EXPECT_CALL(host, RegisterImpression(
                        Field(&blink::Impression::impression_data, 300UL)))
      .WillOnce([&]() { loop2.Quit(); });

  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTag({id: 'link',
                        url: 'page_with_conversion_redirect.html',
                        data: '200',
                        destination: 'https://a.com',
                        registerAttributionSource: true});)"));
  loop1.Run();

  EXPECT_TRUE(ExecJs(web_contents(), R"(
    let link = document.getElementById("link");
    link.removeAttribute("registerattributionsource");
    link.setAttribute("attributionsourceeventid", "300");
    link.setAttribute("registerattributionsource", "");)"));

  loop2.Run();
}

IN_PROC_BROWSER_TEST_F(
    AttributionSourceDeclarationBrowserTest,
    RegisterAttributionSourceInSubFrameWithoutPermissionsPolicy_NotReceived) {
  GURL page_url = https_server()->GetURL("b.test", "/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  GURL subframe_url =
      https_server()->GetURL("c.test", "/page_with_impression_creator.html");
  NavigateIframeToURL(web_contents(), "test_iframe", subframe_url);

  MockAttributionHost host(web_contents());
  EXPECT_CALL(host, RegisterImpression).Times(0);

  RenderFrameHost* subframe = ChildFrameAt(web_contents()->GetMainFrame(), 0);
  EXPECT_TRUE(ExecJs(subframe, R"(
    createImpressionTag({id: 'link',
                        url: 'page_with_conversion_redirect.html',
                        data: '200',
                        destination: 'https://a.com',
                        registerAttributionSource: true});)"));

  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
}

IN_PROC_BROWSER_TEST_F(AttributionSourceDeclarationBrowserTest,
                       WindowOpenImpression_ImpressionReceived) {
  SourceObserver source_observer(web_contents());
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  // Navigate the page using window.open and set an impression.
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    window.open("https://a.com", "_top",
    "attributionsourceeventid=1,attributiondestination=https://a.com,\
    attributionreportto=https://report.com,attributionexpiry=1000,\
    attributionsourcepriority=10");)"));

  // Wait for the impression to be seen by the observer.
  blink::Impression last_impression = source_observer.Wait();

  // Verify the attributes of the impression are set as expected.
  EXPECT_EQ(1UL, last_impression.impression_data);
  EXPECT_EQ(url::Origin::Create(GURL("https://a.com")),
            last_impression.conversion_destination);
  EXPECT_EQ(url::Origin::Create(GURL("https://report.com")),
            last_impression.reporting_origin);
  EXPECT_EQ(base::Milliseconds(1000), *last_impression.expiry);
  EXPECT_EQ(10, last_impression.priority);
}

// TODO(crbug.com/1215063): Flaky timeout on serveral builders.
IN_PROC_BROWSER_TEST_F(
    AttributionSourceDeclarationBrowserTest,
    DISABLED_WindowOpenAttributionSourceFeatures_FeaturesHandled) {
  struct {
    std::string features;
    bool expected;
  } kTestCases[] = {
      {"", false},
      {"attributionsourceeventid=1", false},
      {"attributiondestination=1", false},
      {"attributionexpiry=1", false},
      {"attributionsourcepriority=10", false},
      {"attributionsourceeventid=1,attributiondestination=1234", false},
      {"attributionsourceeventid=1,attributiondestination=abcdefg", false},
      {"attributionsourceeventid=1,attributiondestination=http://a.com", false},
      {"attributionsourceeventid=1,attributiondestination=https://a.com", true},
      {"attributionsourceeventid=bb,attributiondestination=https://a.com",
       true},
      {"attributionsourceeventid=bb,attributiondestination=https://"
       "a.com,attributionsourcepriority=10",
       true},
  };

  for (const auto& test_case : kTestCases) {
    SourceObserver source_observer(web_contents());
    GURL page_url =
        https_server()->GetURL("b.test", "/page_with_impression_creator.html");
    EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

    // Navigate the page using window.open and set an impression.
    EXPECT_TRUE(ExecJs(web_contents(),
                       JsReplace(R"(window.open("https://a.com", "_top", $1);)",
                                 test_case.features)));

    // Wait for the impression to be seen by the observer.
    if (test_case.expected)
      source_observer.Wait();
    else
      EXPECT_TRUE(source_observer.WaitForNavigationWithNoImpression());
  }
}

IN_PROC_BROWSER_TEST_F(AttributionSourceDeclarationBrowserTest,
                       WindowOpenNoUserGesture_NoImpression) {
  SourceObserver source_observer(web_contents());
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  // Navigate the page using window.open and set an impression, but do not give
  // a user gesture.
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    window.open("https://a.com", "_top",
    "attributionsourceeventid=1,attributiondestination=https://a.com");)",
                     EXECUTE_SCRIPT_NO_USER_GESTURE));

  EXPECT_TRUE(source_observer.WaitForNavigationWithNoImpression());
}

IN_PROC_BROWSER_TEST_F(AttributionSourceDeclarationBrowserTest,
                       ImpressionTagWithPriorityClicked_ImpressionReceived) {
  SourceObserver source_observer(web_contents());
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  // Create an anchor tag with impression attributes and click the link. By
  // default the target is set to "_top".
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createAndClickImpressionTag({url: 'page_with_conversion_redirect.html',
                        data: '1',
                        destination: 'https://a.com',
                        reportOrigin: 'https://report.com',
                        priority: 1000});)"));

  // Wait for the impression to be seen by the observer.
  blink::Impression last_impression = source_observer.Wait();

  // Verify the attributes of the impression are set as expected.
  EXPECT_EQ(1UL, last_impression.impression_data);
  EXPECT_EQ(url::Origin::Create(GURL("https://a.com")),
            last_impression.conversion_destination);
  EXPECT_EQ(url::Origin::Create(GURL("https://report.com")),
            last_impression.reporting_origin);
  EXPECT_EQ(1000, last_impression.priority);
}

IN_PROC_BROWSER_TEST_F(AttributionSourceDeclarationBrowserTest,
                       JSRegisterAttributionSource_ImpressionReceived) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  base::RunLoop loop;
  MockAttributionHost host(web_contents());
  EXPECT_CALL(host, RegisterImpression(
                        Field(&blink::Impression::impression_data, 200UL)))
      .WillOnce([&]() { loop.Quit(); });

  EXPECT_TRUE(ExecJs(web_contents(), R"(
    window.attributionReporting.registerAttributionSource({
      attributionSourceEventId: "200",
      attributionDestination: "https://a.com",
    });)"));
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(
    AttributionSourceDeclarationBrowserTest,
    JSRegisterAttributionSource_MissingAttributionSourceEventId_ImpressionNotReceived) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  MockAttributionHost host(web_contents());
  EXPECT_CALL(host, RegisterImpression).Times(0);

  EXPECT_FALSE(ExecJs(web_contents(), R"(
    window.attributionReporting.registerAttributionSource({
      attributionDestination: "https://a.com",
    });)"));

  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
}

IN_PROC_BROWSER_TEST_F(
    AttributionSourceDeclarationBrowserTest,
    JSRegisterAttributionSource_MissingAttributionDestination_ImpressionNotReceived) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  MockAttributionHost host(web_contents());
  EXPECT_CALL(host, RegisterImpression).Times(0);

  EXPECT_FALSE(ExecJs(web_contents(), R"(
    window.attributionReporting.registerAttributionSource({
      attributionSourceEventId: "200",
    });)"));

  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
}

IN_PROC_BROWSER_TEST_F(
    AttributionSourceDeclarationBrowserTest,
    JSRegisterAttributionSource_InsecureAttributionDestination_ImpressionNotReceived) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  MockAttributionHost host(web_contents());
  EXPECT_CALL(host, RegisterImpression).Times(0);

  EXPECT_FALSE(ExecJs(web_contents(), R"(
    window.attributionReporting.registerAttributionSource({
      attributionSourceEventId: "200",
      attributionDestination: "http://a.com",
    });)"));

  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
}

IN_PROC_BROWSER_TEST_F(AttributionSourceDeclarationBrowserTest,
                       ImpressionInSubframeInInsecureContext_NotRegistered) {
  // Start with localhost(secure) iframing a.test (insecure) iframing
  // localhost(secure). This context is insecure since the middle iframe in the
  // ancestor chain is insecure.

  GURL main_frame_url =
      embedded_test_server()->GetURL("/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), main_frame_url));

  EXPECT_TRUE(ExecJs(shell(), R"(
        let frame = document.getElementById('test_iframe');
        frame.setAttribute('allow', 'attribution-reporting');)"));

  GURL middle_iframe_url = embedded_test_server()->GetURL(
      "insecure.example", "/page_with_iframe.html");
  NavigateIframeToURL(web_contents(), "test_iframe", middle_iframe_url);

  RenderFrameHost* middle_iframe =
      ChildFrameAt(web_contents()->GetMainFrame(), 0);

  GURL innermost_iframe_url(
      embedded_test_server()->GetURL("/page_with_impression_creator.html"));
  EXPECT_TRUE(ExecJs(middle_iframe, JsReplace(R"(
      let frame = document.getElementById('test_iframe');
      frame.setAttribute('allow', 'attribution-reporting');
      frame.src = $1;)",
                                              innermost_iframe_url)));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  SourceObserver source_observer(web_contents());
  RenderFrameHost* innermost_iframe = ChildFrameAt(middle_iframe, 0);
  EXPECT_TRUE(ExecJs(innermost_iframe, R"(
    createAndClickImpressionTag({url: 'page_with_conversion_redirect.html',
                        data: '1',
                        destination: 'https://a.com'});)"));

  // We should see a null impression on the navigation.
  EXPECT_TRUE(source_observer.WaitForNavigationWithNoImpression());
}

}  // namespace content
