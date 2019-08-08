// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/optimization_guide/hints_component_info.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/test_hints_component_creator.h"
#include "components/previews/content/previews_decider_impl.h"
#include "components/previews/content/previews_optimization_guide.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/previews/core/previews_constants.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/reporting/reporting_policy.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_quality_tracker.h"

namespace {

// Retries fetching |histogram_name| until it contains at least |count| samples.
void RetryForHistogramUntilCountReached(base::HistogramTester* histogram_tester,
                                        const std::string& histogram_name,
                                        size_t count) {
  while (true) {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();

    content::FetchHistogramsFromChildProcesses();
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    const std::vector<base::Bucket> buckets =
        histogram_tester->GetAllSamples(histogram_name);
    size_t total_count = 0;
    for (const auto& bucket : buckets) {
      total_count += bucket.count;
    }
    if (total_count >= count) {
      break;
    }
  }
}

}  // namespace

class PreviewsBrowserTest : public InProcessBrowserTest {
 public:
  PreviewsBrowserTest() = default;

  ~PreviewsBrowserTest() override = default;

  void SetUpOnMainThread() override {
    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_2G);

    // Set up https server with resource monitor.
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->ServeFilesFromSourceDirectory("chrome/test/data/previews");
    https_server_->RegisterRequestMonitor(base::BindRepeating(
        &PreviewsBrowserTest::MonitorResourceRequest, base::Unretained(this)));
    ASSERT_TRUE(https_server_->Start());

    https_url_ = https_server_->GetURL("/noscript_test.html");
    ASSERT_TRUE(https_url_.SchemeIs(url::kHttpsScheme));

    https_no_transform_url_ =
        https_server_->GetURL("/noscript_test_with_no_transform_header.html");
    ASSERT_TRUE(https_no_transform_url_.SchemeIs(url::kHttpsScheme));

    https_hint_setup_url_ = https_server_->GetURL("/hint_setup.html");
    ASSERT_TRUE(https_hint_setup_url_.SchemeIs(url::kHttpsScheme));
    ASSERT_EQ(https_hint_setup_url_.host(), https_url_.host());

    // Set up http server with resource monitor and redirect handler.
    http_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTP));
    http_server_->ServeFilesFromSourceDirectory("chrome/test/data/previews");
    http_server_->RegisterRequestMonitor(base::BindRepeating(
        &PreviewsBrowserTest::MonitorResourceRequest, base::Unretained(this)));
    http_server_->RegisterRequestHandler(base::BindRepeating(
        &PreviewsBrowserTest::HandleRedirectRequest, base::Unretained(this)));
    ASSERT_TRUE(http_server_->Start());

    http_url_ = http_server_->GetURL("/noscript_test.html");
    ASSERT_TRUE(http_url_.SchemeIs(url::kHttpScheme));

    redirect_url_ = http_server_->GetURL("/redirect.html");
    ASSERT_TRUE(redirect_url_.SchemeIs(url::kHttpScheme));

    http_hint_setup_url_ = http_server_->GetURL("/hint_setup.html");
    ASSERT_TRUE(http_hint_setup_url_.SchemeIs(url::kHttpScheme));
    ASSERT_EQ(http_hint_setup_url_.host(), http_url_.host());
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch("enable-spdy-proxy-auth");
    // Due to race conditions, it's possible that blacklist data is not loaded
    // at the time of first navigation. That may prevent Preview from
    // triggering, and causing the test to flake.
    cmd->AppendSwitch(previews::switches::kIgnorePreviewsBlacklist);
  }

  const GURL& https_url() const { return https_url_; }
  const GURL& https_no_transform_url() const { return https_no_transform_url_; }
  const GURL& https_hint_setup_url() const { return https_hint_setup_url_; }
  const GURL& http_url() const { return http_url_; }
  const GURL& redirect_url() const { return redirect_url_; }
  const GURL& http_hint_setup_url() const { return http_hint_setup_url_; }

  bool noscript_css_requested() const {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    return noscript_css_requested_;
  }
  bool noscript_js_requested() const {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    return noscript_js_requested_;
  }

 private:
  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server_->ShutdownAndWaitUntilComplete());
    EXPECT_TRUE(http_server_->ShutdownAndWaitUntilComplete());

    InProcessBrowserTest::TearDownOnMainThread();
  }

  // Called by |https_server_|.
  void MonitorResourceRequest(const net::test_server::HttpRequest& request) {
    // This method is called on embedded test server thread. Post the
    // information on UI thread.
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&PreviewsBrowserTest::MonitorResourceRequestOnUIThread,
                       base::Unretained(this), request));
    base::RunLoop().RunUntilIdle();
  }

  void MonitorResourceRequestOnUIThread(
      const net::test_server::HttpRequest& request) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (request.GetURL().spec().find("noscript_test.css") !=
        std::string::npos) {
      noscript_css_requested_ = true;
    }
    if (request.GetURL().spec().find("noscript_test.js") != std::string::npos) {
      noscript_js_requested_ = true;
    }
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRedirectRequest(
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response;
    if (request.GetURL().spec().find("redirect") != std::string::npos) {
      response.reset(new net::test_server::BasicHttpResponse);
      response->set_code(net::HTTP_FOUND);
      response->AddCustomHeader("Location", https_url().spec());
    }
    return std::move(response);
  }

  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<net::EmbeddedTestServer> http_server_;
  GURL https_url_;
  GURL https_no_transform_url_;
  GURL https_hint_setup_url_;
  GURL http_url_;
  GURL redirect_url_;
  GURL http_hint_setup_url_;

  // Should be accessed only on UI thread.
  bool noscript_css_requested_ = false;
  bool noscript_js_requested_ = false;

  DISALLOW_COPY_AND_ASSIGN(PreviewsBrowserTest);
};

// Loads a webpage that has both script and noscript tags and also requests
// a script resource. Verifies that the noscript tag is not evaluated and the
// script resource is loaded.
IN_PROC_BROWSER_TEST_F(PreviewsBrowserTest, NoScriptPreviewsDisabled) {
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), https_url());

  // Verify loaded js resource but not css triggered by noscript tag.
  EXPECT_TRUE(noscript_js_requested());
  EXPECT_FALSE(noscript_css_requested());

  // Verify info bar not presented via histogram check.
  histogram_tester.ExpectTotalCount("Previews.PreviewShown.NoScript", 0);
}

// This test class enables NoScriptPreviews but without OptimizationHints.
class PreviewsNoScriptBrowserTest : public PreviewsBrowserTest {
 public:
  PreviewsNoScriptBrowserTest() {}

  ~PreviewsNoScriptBrowserTest() override {}

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {previews::features::kPreviews, previews::features::kOptimizationHints,
         previews::features::kNoScriptPreviews,
         data_reduction_proxy::features::
             kDataReductionProxyEnabledWithNetworkService},
        {});
    PreviewsBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch("enable-spdy-proxy-auth");
    cmd->AppendSwitch("optimization-guide-disable-installer");
    cmd->AppendSwitch("purge_hint_cache_store");
    cmd->AppendSwitch(previews::switches::kIgnorePreviewsBlacklist);
  }

  // Creates hint data for the |hint_setup_url|'s host and then performs a
  // navigation to |hint_setup_url| to trigger the hints to be loaded into the
  // hint cache so they will be available for a subsequent navigation to a test
  // url to the same host.
  void SetUpNoScriptWhitelist(const GURL& hint_setup_url) {
    const optimization_guide::HintsComponentInfo& component_info =
        test_hints_component_creator_.CreateHintsComponentInfoWithPageHints(
            optimization_guide::proto::NOSCRIPT, {hint_setup_url.host()}, {});

    base::HistogramTester histogram_tester;

    // Register a QuitClosure for when the next hint update is started below.
    base::RunLoop run_loop;
    PreviewsServiceFactory::GetForProfile(
        Profile::FromBrowserContext(browser()
                                        ->tab_strip_model()
                                        ->GetActiveWebContents()
                                        ->GetBrowserContext()))
        ->previews_ui_service()
        ->previews_decider_impl()
        ->previews_opt_guide()
        ->ListenForNextUpdateForTesting(run_loop.QuitClosure());

    g_browser_process->optimization_guide_service()->MaybeUpdateHintsComponent(
        component_info);

    run_loop.Run();

    // Navigate to |hint_setup_url| to prime the OptimizationGuide hints for the
    // url's host and ensure that they have been loaded from the store (via
    // histogram) prior to the navigation that tests functionality.
    ui_test_utils::NavigateToURL(browser(), hint_setup_url);

    RetryForHistogramUntilCountReached(
        &histogram_tester,
        previews::kPreviewsOptimizationGuideOnLoadedHintResultHistogramString,
        1);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  optimization_guide::testing::TestHintsComponentCreator
      test_hints_component_creator_;
};

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
#define DISABLE_ON_WIN_MAC_CHROMESOS(x) DISABLED_##x
#else
#define DISABLE_ON_WIN_MAC_CHROMESOS(x) x
#endif

// Loads a webpage that has both script and noscript tags and also requests
// a script resource. Verifies that the noscript tag is evaluated and the
// script resource is not loaded.
IN_PROC_BROWSER_TEST_F(PreviewsNoScriptBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMESOS(NoScriptPreviewsEnabled)) {
  GURL url = https_url();

  // Whitelist NoScript for https_hint_setup_url()'s' host.
  SetUpNoScriptWhitelist(https_hint_setup_url());

  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), url);

  // Verify loaded noscript tag triggered css resource but not js one.
  EXPECT_TRUE(noscript_css_requested());
  EXPECT_FALSE(noscript_js_requested());

  // Verify info bar presented via histogram check.
  RetryForHistogramUntilCountReached(&histogram_tester,
                                     "Previews.PreviewShown.NoScript", 1);
}

IN_PROC_BROWSER_TEST_F(
    PreviewsNoScriptBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMESOS(NoScriptPreviewsEnabledButHttpRequest)) {
  GURL url = http_url();

  // Whitelist NoScript for http_hint_setup_url() host.
  SetUpNoScriptWhitelist(http_hint_setup_url());

  ui_test_utils::NavigateToURL(browser(), url);

  // Verify loaded js resource but not css triggered by noscript tag.
  EXPECT_TRUE(noscript_js_requested());
  EXPECT_FALSE(noscript_css_requested());
}

IN_PROC_BROWSER_TEST_F(PreviewsNoScriptBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMESOS(
                           NoScriptPreviewsEnabledButNoTransformDirective)) {
  GURL url = https_no_transform_url();

  // Whitelist NoScript for https_hint_setup_url()'s' host.
  SetUpNoScriptWhitelist(https_hint_setup_url());

  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), url);

  // Verify loaded js resource but not css triggered by noscript tag.
  EXPECT_TRUE(noscript_js_requested());
  EXPECT_FALSE(noscript_css_requested());

  histogram_tester.ExpectUniqueSample(
      "Previews.CacheControlNoTransform.BlockedPreview", 5 /* NoScript */, 1);
}

IN_PROC_BROWSER_TEST_F(
    PreviewsNoScriptBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMESOS(NoScriptPreviewsEnabledHttpRedirectToHttps)) {
  GURL url = redirect_url();

  // Whitelist NoScript for http_hint_setup_url() host.
  SetUpNoScriptWhitelist(http_hint_setup_url());

  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), url);

  // Verify loaded noscript tag triggered css resource but not js one.
  EXPECT_TRUE(noscript_css_requested());
  EXPECT_FALSE(noscript_js_requested());

  // Verify info bar presented via histogram check.
  RetryForHistogramUntilCountReached(&histogram_tester,
                                     "Previews.PreviewShown.NoScript", 1);
}

IN_PROC_BROWSER_TEST_F(
    PreviewsNoScriptBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMESOS(NoScriptPreviewsRecordsOptOut)) {
  GURL url = redirect_url();

  // Whitelist NoScript for http_hint_setup_url()'s' host.
  SetUpNoScriptWhitelist(http_hint_setup_url());

  base::HistogramTester histogram_tester;

  // Navigate to a NoScript Preview page.
  ui_test_utils::NavigateToURL(browser(), url);

  // Terminate the previous page (non-opt out) and pull up a new NoScript page.
  ui_test_utils::NavigateToURL(browser(), url);
  histogram_tester.ExpectUniqueSample("Previews.OptOut.UserOptedOut.NoScript",
                                      0, 1);

  // Opt out of the NoScript Preview page.
  PreviewsUITabHelper::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents())
      ->ReloadWithoutPreviews();

  histogram_tester.ExpectBucketCount("Previews.OptOut.UserOptedOut.NoScript", 1,
                                     1);
}

IN_PROC_BROWSER_TEST_F(
    PreviewsNoScriptBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMESOS(NoScriptPreviewsEnabledByWhitelist)) {
  GURL url = https_url();

  // Whitelist NoScript for https_hint_setup_url()'s' host.
  SetUpNoScriptWhitelist(https_hint_setup_url());

  ui_test_utils::NavigateToURL(browser(), url);

  // Verify loaded noscript tag triggered css resource but not js one.
  EXPECT_TRUE(noscript_css_requested());
  EXPECT_FALSE(noscript_js_requested());
}

IN_PROC_BROWSER_TEST_F(
    PreviewsNoScriptBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMESOS(NoScriptPreviewsNotEnabledByWhitelist)) {
  GURL url = https_url();

  // Whitelist random site for NoScript.
  SetUpNoScriptWhitelist(GURL("https://foo.com"));

  ui_test_utils::NavigateToURL(browser(), url);

  // Verify loaded js resource but not css triggered by noscript tag.
  EXPECT_TRUE(noscript_js_requested());
  EXPECT_FALSE(noscript_css_requested());
}

namespace {

class PreviewsReportingBrowserTest : public CertVerifierBrowserTest {
 public:
  PreviewsReportingBrowserTest()
      : https_server_(net::test_server::EmbeddedTestServer::TYPE_HTTPS) {}
  ~PreviewsReportingBrowserTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {network::features::kReporting, previews::features::kPreviews,
         previews::features::kClientLoFi,
         data_reduction_proxy::features::
             kDataReductionProxyEnabledWithNetworkService},
        {network::features::kNetworkErrorLogging});
    CertVerifierBrowserTest::SetUp();
    // Make report delivery happen instantly.
    net::ReportingPolicy policy;
    policy.delivery_interval = base::TimeDelta::FromSeconds(0);
    net::ReportingPolicy::UsePolicyForTesting(policy);
  }

  void SetUpOnMainThread() override {
    CertVerifierBrowserTest::SetUpOnMainThread();

    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_2G);

    host_resolver()->AddRule("*", "127.0.0.1");

    main_frame_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            server(), "/lofi_test");
    upload_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(server(),
                                                                     "/upload");
    mock_cert_verifier()->set_default_result(net::OK);
    ASSERT_TRUE(server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    CertVerifierBrowserTest::SetUpCommandLine(cmd);
    cmd->AppendSwitch("enable-spdy-proxy-auth");

    // Due to race conditions, it's possible that blacklist data is not loaded
    // at the time of first navigation. That may prevent Preview from
    // triggering, and causing the test to flake.
    cmd->AppendSwitch(previews::switches::kIgnorePreviewsBlacklist);
  }

  net::EmbeddedTestServer* server() { return &https_server_; }
  int port() const { return https_server_.port(); }

  net::test_server::ControllableHttpResponse* main_frame_response() {
    return main_frame_response_.get();
  }

  net::test_server::ControllableHttpResponse* upload_response() {
    return upload_response_.get();
  }

  GURL GetReportingEnabledURL() const {
    return GURL(base::StringPrintf("https://example.com:%d/lofi_test", port()));
  }

  GURL GetCollectorURL() const {
    return GURL(base::StringPrintf("https://example.com:%d/upload", port()));
  }

  std::string GetReportToHeader() const {
    return "Report-To: {\"endpoints\":[{\"url\":\"" + GetCollectorURL().spec() +
           "\"}],\"max_age\":86400}\r\n";
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_;
  std::unique_ptr<net::test_server::ControllableHttpResponse>
      main_frame_response_;
  std::unique_ptr<net::test_server::ControllableHttpResponse> upload_response_;

  DISALLOW_COPY_AND_ASSIGN(PreviewsReportingBrowserTest);
};

std::unique_ptr<base::Value> ParseReportUpload(const std::string& payload) {
  auto parsed_payload = base::test::ParseJsonDeprecated(payload);
  // Clear out any non-reproducible fields.
  for (auto& report : parsed_payload->GetList()) {
    report.RemoveKey("age");
    auto* user_agent =
        report.FindKeyOfType("user_agent", base::Value::Type::STRING);
    if (user_agent != nullptr)
      *user_agent = base::Value("Mozilla/1.0");
  }
  return parsed_payload;
}

}  // namespace

// Checks that the intervention is reported during a LoFi load.
IN_PROC_BROWSER_TEST_F(PreviewsReportingBrowserTest,
                       TestReportingHeadersSentForLoFiPreview) {
  NavigateParams params(browser(), GetReportingEnabledURL(),
                        ui::PAGE_TRANSITION_LINK);
  Navigate(&params);

  main_frame_response()->WaitForRequest();
  main_frame_response()->Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html\r\n"
      "Empty page \r\n");
  main_frame_response()->Send(GetReportToHeader());
  main_frame_response()->Send("\r\n");
  main_frame_response()->Done();

  upload_response()->WaitForRequest();
  auto actual = ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 204 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  EXPECT_TRUE(actual != nullptr);
  auto expected = base::test::ParseJsonDeprecated(base::StringPrintf(
      R"text(
        [
          {
            "body": {
              "id": "LitePageServed",
              "message": "Modified page load behavior on the page because )text"
      R"text(the page was expected to take a long amount of time to load. )text"
      R"text(https://www.chromestatus.com/feature/5148050062311424"
            },
            "type": "intervention",
            "url": "https://example.com:%d/lofi_test",
            "user_agent": "Mozilla/1.0"
          }
        ]
      )text",
      port()));
  EXPECT_EQ(*expected, *actual);
}
