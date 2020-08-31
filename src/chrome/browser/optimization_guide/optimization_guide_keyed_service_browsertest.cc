// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_hints_manager.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_top_host_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/command_line_top_host_provider.h"
#include "components/optimization_guide/optimization_guide_enums.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_prefs.h"
#include "components/optimization_guide/optimization_guide_store.h"
#include "components/optimization_guide/optimization_guide_switches.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/test_hints_component_creator.h"
#include "components/prefs/pref_service.h"
#include "components/previews/core/previews_switches.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/escape.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"

namespace {

// Fetch and calculate the total number of samples from all the bins for
// |histogram_name|. Note: from some browertests run, there might be two
// profiles created, and this will return the total sample count across
// profiles.
int GetTotalHistogramSamples(const base::HistogramTester& histogram_tester,
                             const std::string& histogram_name) {
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples(histogram_name);
  int total = 0;
  for (const auto& bucket : buckets)
    total += bucket.count;

  return total;
}

// Retries fetching |histogram_name| until it contains at least |count| samples.
int RetryForHistogramUntilCountReached(
    const base::HistogramTester& histogram_tester,
    const std::string& histogram_name,
    int count) {
  int total = 0;
  while (true) {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();

    total = GetTotalHistogramSamples(histogram_tester, histogram_name);
    if (total >= count)
      return total;
  }
}

// A WebContentsObserver that asks whether an optimization type can be applied.
class OptimizationGuideConsumerWebContentsObserver
    : public content::WebContentsObserver {
 public:
  OptimizationGuideConsumerWebContentsObserver(
      content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~OptimizationGuideConsumerWebContentsObserver() override = default;

  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (callback_) {
      OptimizationGuideKeyedService* service =
          OptimizationGuideKeyedServiceFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
      service->CanApplyOptimizationAsync(navigation_handle,
                                         optimization_guide::proto::NOSCRIPT,
                                         std::move(callback_));
    }
  }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    OptimizationGuideKeyedService* service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
    last_should_target_navigation_decision_ = service->ShouldTargetNavigation(
        navigation_handle,
        optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
    last_can_apply_optimization_decision_ = service->CanApplyOptimization(
        navigation_handle, optimization_guide::proto::NOSCRIPT,
        /*optimization_metadata=*/nullptr);
  }

  // Returns the last optimization guide decision that was returned by the
  // OptimizationGuideKeyedService's ShouldTargetNavigation() method.
  optimization_guide::OptimizationGuideDecision
  last_should_target_navigation_decision() {
    return last_should_target_navigation_decision_;
  }

  // Returns the last optimization guide decision that was returned by the
  // OptimizationGuideKeyedService's CanApplyOptimization() method.
  optimization_guide::OptimizationGuideDecision
  last_can_apply_optimization_decision() {
    return last_can_apply_optimization_decision_;
  }

  void set_callback(
      optimization_guide::OptimizationGuideDecisionCallback callback) {
    callback_ = std::move(callback);
  }

 private:
  optimization_guide::OptimizationGuideDecision
      last_should_target_navigation_decision_ =
          optimization_guide::OptimizationGuideDecision::kUnknown;
  optimization_guide::OptimizationGuideDecision
      last_can_apply_optimization_decision_ =
          optimization_guide::OptimizationGuideDecision::kUnknown;
  optimization_guide::OptimizationGuideDecisionCallback callback_;
};

}  // namespace

class OptimizationGuideKeyedServiceDisabledBrowserTest
    : public InProcessBrowserTest {
 public:
  OptimizationGuideKeyedServiceDisabledBrowserTest() {
    feature_list_.InitWithFeatures(
        {}, {optimization_guide::features::kOptimizationHints});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceDisabledBrowserTest,
                       KeyedServiceEnabledButOptimizationHintsDisabled) {
  EXPECT_EQ(nullptr, OptimizationGuideKeyedServiceFactory::GetForProfile(
                         browser()->profile()));
}

class OptimizationGuideKeyedServiceBrowserTest
    : public OptimizationGuideKeyedServiceDisabledBrowserTest {
 public:
  OptimizationGuideKeyedServiceBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {optimization_guide::features::kOptimizationHints}, {});
  }

  ~OptimizationGuideKeyedServiceBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch(optimization_guide::switches::kPurgeHintsStore);
  }

  void SetUpOnMainThread() override {
    OptimizationGuideKeyedServiceDisabledBrowserTest::SetUpOnMainThread();

    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    https_server_->RegisterRequestHandler(base::BindRepeating(
        &OptimizationGuideKeyedServiceBrowserTest::HandleRequest,
        base::Unretained(this)));
    ASSERT_TRUE(https_server_->Start());

    url_with_hints_ = https_server_->GetURL("/simple.html");
    url_that_redirects_ =
        https_server_->GetURL("/redirect?" + url_with_hints_.spec());
    url_that_redirects_to_no_hints_ =
        https_server_->GetURL("/redirect?https://nohints.com/");

    SetEffectiveConnectionType(
        net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server_->ShutdownAndWaitUntilComplete());

    OptimizationGuideKeyedServiceDisabledBrowserTest::TearDownOnMainThread();
  }

  void RegisterWithKeyedService() {
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->RegisterOptimizationTypesAndTargets(
            {optimization_guide::proto::NOSCRIPT},
            {optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

    // Set up an OptimizationGuideKeyedService consumer.
    consumer_.reset(new OptimizationGuideConsumerWebContentsObserver(
        browser()->tab_strip_model()->GetActiveWebContents()));
  }

  optimization_guide::TopHostProvider* top_host_provider() {
    auto* optimization_guide_keyed_service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile());
    return optimization_guide_keyed_service->GetTopHostProvider();
  }

  optimization_guide::PredictionManager* prediction_manager() {
    auto* optimization_guide_keyed_service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile());
    return optimization_guide_keyed_service->GetPredictionManager();
  }

  void PushHintsComponentAndWaitForCompletion() {
    base::RunLoop run_loop;
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->GetHintsManager()
        ->ListenForNextUpdateForTesting(run_loop.QuitClosure());

    const optimization_guide::HintsComponentInfo& component_info =
        test_hints_component_creator_.CreateHintsComponentInfoWithPageHints(
            optimization_guide::proto::NOSCRIPT, {url_with_hints_.host()},
            "simple.html", {});

    g_browser_process->optimization_guide_service()->MaybeUpdateHintsComponent(
        component_info);

    run_loop.Run();
  }

  // Sets the effective connection type that the Network Quality Tracker will
  // report.
  void SetEffectiveConnectionType(
      net::EffectiveConnectionType effective_connection_type) {
    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(effective_connection_type);
  }

  // Sets the callback on the consumer of the OptimizationGuideKeyedService. If
  // set, this will call the async version of CanApplyOptimization.
  void SetCallbackOnConsumer(
      optimization_guide::OptimizationGuideDecisionCallback callback) {
    ASSERT_TRUE(consumer_);

    consumer_->set_callback(std::move(callback));
  }

  // Returns the last decision from the CanApplyOptimization() method seen by
  // the consumer of the OptimizationGuideKeyedService.
  optimization_guide::OptimizationGuideDecision
  last_should_target_navigation_decision() {
    return consumer_->last_should_target_navigation_decision();
  }

  // Returns the last decision from the CanApplyOptimization() method seen by
  // the consumer of the OptimizationGuideKeyedService.
  optimization_guide::OptimizationGuideDecision
  last_can_apply_optimization_decision() {
    return consumer_->last_can_apply_optimization_decision();
  }

  GURL url_with_hints() { return url_with_hints_; }

  GURL url_that_redirects_to_hints() { return url_that_redirects_; }

  GURL url_that_redirects_to_no_hints() {
    return url_that_redirects_to_no_hints_;
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().spec().find("redirect") == std::string::npos) {
      return std::unique_ptr<net::test_server::HttpResponse>();
    }

    GURL request_url = request.GetURL();
    std::string dest =
        net::UnescapeBinaryURLComponent(request_url.query_piece());

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_FOUND);
    http_response->AddCustomHeader("Location", dest);
    return http_response;
  }

  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  GURL url_with_hints_;
  GURL url_that_redirects_;
  GURL url_that_redirects_to_no_hints_;
  base::test::ScopedFeatureList scoped_feature_list_;
  optimization_guide::testing::TestHintsComponentCreator
      test_hints_component_creator_;
  std::unique_ptr<OptimizationGuideConsumerWebContentsObserver> consumer_;
  // Histogram tester used specifically to capture metrics that are recorded
  // during browser initialization.
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(OptimizationGuideKeyedServiceBrowserTest);
};

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       PredictionManagerNotCreatedIfFeatureDisabled) {
  ASSERT_FALSE(prediction_manager());
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       TopHostProviderNotSetIfNotAllowed) {
  ASSERT_FALSE(top_host_provider());

  // ChromeOS has multiple profiles and optimization guide currently does not
  // run on non-Android.
#if !defined(OS_CHROMEOS)
  histogram_tester()->ExpectUniqueSample(
      "OptimizationGuide.RemoteFetchingEnabled", false, 1);
#endif
}

IN_PROC_BROWSER_TEST_F(
    OptimizationGuideKeyedServiceBrowserTest,
    NavigateToPageWithHintsButNoRegistrationDoesNotAttemptToLoadHint) {
  PushHintsComponentAndWaitForCompletion();

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), url_with_hints());
  histogram_tester.ExpectTotalCount("OptimizationGuide.LoadedHint.Result", 0);

  // Navigate away so UKM get recorded.
  ui_test_utils::NavigateToURL(browser(), url_with_hints());

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       NavigateToPageWithAsyncCallbackReturnsAnswerRedirect) {
  PushHintsComponentAndWaitForCompletion();
  RegisterWithKeyedService();

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  SetCallbackOnConsumer(base::BindOnce(
      [](base::RunLoop* run_loop,
         optimization_guide::OptimizationGuideDecision decision,
         const optimization_guide::OptimizationMetadata& metadata) {
        EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kFalse,
                  decision);
        run_loop->Quit();
      },
      run_loop.get()));

  ui_test_utils::NavigateToURL(browser(), url_that_redirects_to_no_hints());
  run_loop->Run();
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       NavigateToPageWithAsyncCallbackReturnsAnswer) {
  PushHintsComponentAndWaitForCompletion();
  RegisterWithKeyedService();

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  SetCallbackOnConsumer(base::BindOnce(
      [](base::RunLoop* run_loop,
         optimization_guide::OptimizationGuideDecision decision,
         const optimization_guide::OptimizationMetadata& metadata) {
        EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kTrue,
                  decision);
        EXPECT_TRUE(metadata.previews_metadata().has_value());
        run_loop->Quit();
      },
      run_loop.get()));

  ui_test_utils::NavigateToURL(browser(), url_with_hints());
  run_loop->Run();
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       NavigateToPageWithAsyncCallbackReturnsAnswerEventually) {
  PushHintsComponentAndWaitForCompletion();
  RegisterWithKeyedService();

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  SetCallbackOnConsumer(base::BindOnce(
      [](base::RunLoop* run_loop,
         optimization_guide::OptimizationGuideDecision decision,
         const optimization_guide::OptimizationMetadata& metadata) {
        EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kFalse,
                  decision);
        run_loop->Quit();
      },
      run_loop.get()));

  ui_test_utils::NavigateToURL(browser(), GURL("https://nohints.com/"));
  run_loop->Run();
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       NavigateToPageWithHintsLoadsHint) {
  PushHintsComponentAndWaitForCompletion();
  RegisterWithKeyedService();

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), url_with_hints());

  EXPECT_GT(RetryForHistogramUntilCountReached(
                histogram_tester, "OptimizationGuide.LoadedHint.Result", 1),
            0);
  // There is a hint that matches this URL, so there should be an attempt to
  // load a hint that succeeds.
  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      true, 1);
  // We had a hint and it was loaded and it was painful enough.
  EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kTrue,
            last_should_target_navigation_decision());
  EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kTrue,
            last_can_apply_optimization_decision());

  // Navigate away so metrics get recorded.
  ui_test_utils::NavigateToURL(browser(), url_with_hints());

  // Expect that UKM is recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry,
      ukm::builders::OptimizationGuide::kRegisteredOptimizationTypesName));
  // NOSCRIPT = 1, so bit mask is 10, which equals 2.
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kRegisteredOptimizationTypesName,
      2);
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       RecordsMetricsWhenTabHidden) {
  PushHintsComponentAndWaitForCompletion();
  RegisterWithKeyedService();

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), url_with_hints());

  EXPECT_GT(RetryForHistogramUntilCountReached(
                histogram_tester, "OptimizationGuide.LoadedHint.Result", 1),
            0);
  // There is a hint that matches this URL, so there should be an attempt to
  // load a hint that succeeds.
  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      true, 1);
  // We had a hint and it was loaded and it was painful enough.
  EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kTrue,
            last_should_target_navigation_decision());
  EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kTrue,
            last_can_apply_optimization_decision());

  // Make sure metrics get recorded when tab is hidden.
  browser()->tab_strip_model()->GetActiveWebContents()->WasHidden();

  // Expect that the optimization guide UKM is recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry,
      ukm::builders::OptimizationGuide::kRegisteredOptimizationTypesName));
  // NOSCRIPT = 1, so bit mask is 10, which equals 2.
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kRegisteredOptimizationTypesName,
      2);
}

IN_PROC_BROWSER_TEST_F(
    OptimizationGuideKeyedServiceBrowserTest,
    NavigateToPageThatRedirectsToUrlWithHintsShouldAttemptTwoLoads) {
  PushHintsComponentAndWaitForCompletion();
  RegisterWithKeyedService();

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), url_that_redirects_to_hints());

  EXPECT_EQ(RetryForHistogramUntilCountReached(
                histogram_tester, "OptimizationGuide.LoadedHint.Result", 2),
            2);
  // Should attempt and succeed to load a hint once for the initial navigation
  // and redirect.
  histogram_tester.ExpectBucketCount("OptimizationGuide.LoadedHint.Result",
                                     true, 2);
  // Hint is still applicable so we expect it to be allowed to be applied.
  EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kTrue,
            last_should_target_navigation_decision());
  EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kTrue,
            last_can_apply_optimization_decision());
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       NavigateToPageWithoutHint) {
  PushHintsComponentAndWaitForCompletion();
  RegisterWithKeyedService();

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), GURL("https://nohints.com/"));

  EXPECT_EQ(RetryForHistogramUntilCountReached(
                histogram_tester, "OptimizationGuide.LoadedHint.Result", 1),
            1);
  // There were no hints that match this URL, but there should still be an
  // attempt to load a hint but still fail.
  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      false, 1);
  EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kTrue,
            last_should_target_navigation_decision());
  EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kFalse,
            last_can_apply_optimization_decision());

  // Navigate away so metrics get recorded.
  ui_test_utils::NavigateToURL(browser(), url_with_hints());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.NoScript",
      static_cast<int>(
          optimization_guide::OptimizationTypeDecision::kNoHintAvailable),
      1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.TargetDecision.PainfulPageLoad",
      static_cast<int>(
          optimization_guide::OptimizationTargetDecision::kPageLoadMatches),
      1);
}

class OptimizationGuideKeyedServiceDataSaverUserWithInfobarShownTest
    : public OptimizationGuideKeyedServiceBrowserTest {
 public:
  OptimizationGuideKeyedServiceDataSaverUserWithInfobarShownTest() = default;
  ~OptimizationGuideKeyedServiceDataSaverUserWithInfobarShownTest() override =
      default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        optimization_guide::features::kRemoteOptimizationGuideFetching);

    OptimizationGuideKeyedServiceBrowserTest::SetUp();
  }

  void TearDown() override {
    OptimizationGuideKeyedServiceBrowserTest::TearDown();

    scoped_feature_list_.Reset();
  }

  void SetUpOnMainThread() override {
    OptimizationGuideKeyedServiceBrowserTest::SetUpOnMainThread();

    SeedSiteEngagementService();
    // Set the blacklist state to initialized so the sites in the engagement
    // service will be used and not blacklisted on the first GetTopHosts
    // request.
    InitializeTopHostBlacklist();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    OptimizationGuideKeyedServiceBrowserTest::SetUpCommandLine(cmd);

    cmd->AppendSwitch("enable-spdy-proxy-auth");
    // Add switch to avoid having to see the infobar in the test.
    cmd->AppendSwitch(previews::switches::kDoNotRequireLitePageRedirectInfoBar);
    // Add switch to avoid racing navigations in the test.
    cmd->AppendSwitch(optimization_guide::switches::
                          kDisableFetchingHintsAtNavigationStartForTesting);
  }

 private:
  // Seeds the Site Engagement Service with two HTTP and two HTTPS sites for the
  // current profile.
  void SeedSiteEngagementService() {
    SiteEngagementService* service = SiteEngagementService::Get(
        Profile::FromBrowserContext(browser()
                                        ->tab_strip_model()
                                        ->GetActiveWebContents()
                                        ->GetBrowserContext()));
    GURL https_url1("https://myfavoritesite.com/");
    service->AddPointsForTesting(https_url1, 15);

    GURL https_url2("https://myotherfavoritesite.com/");
    service->AddPointsForTesting(https_url2, 3);
  }

  void InitializeTopHostBlacklist() {
    Profile::FromBrowserContext(browser()
                                    ->tab_strip_model()
                                    ->GetActiveWebContents()
                                    ->GetBrowserContext())
        ->GetPrefs()
        ->SetInteger(
            optimization_guide::prefs::kHintsFetcherTopHostBlacklistState,
            static_cast<int>(
                optimization_guide::prefs::HintsFetcherTopHostBlacklistState::
                    kInitialized));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    OptimizationGuideKeyedServiceDataSaverUserWithInfobarShownTest,
    TopHostProviderIsSentDown) {
  ASSERT_TRUE(top_host_provider());
  std::vector<std::string> top_hosts = top_host_provider()->GetTopHosts();
  EXPECT_EQ(2ul, top_hosts.size());
  EXPECT_EQ("myfavoritesite.com", top_hosts[0]);
  EXPECT_EQ("myotherfavoritesite.com", top_hosts[1]);

  // ChromeOS has multiple profiles and optimization guide currently does not
  // run on non-Android.
#if !defined(OS_CHROMEOS)
  histogram_tester()->ExpectUniqueSample(
      "OptimizationGuide.RemoteFetchingEnabled", true, 1);
#endif
}

class OptimizationGuideKeyedServiceCommandLineOverridesTest
    : public OptimizationGuideKeyedServiceDataSaverUserWithInfobarShownTest {
 public:
  OptimizationGuideKeyedServiceCommandLineOverridesTest() = default;
  ~OptimizationGuideKeyedServiceCommandLineOverridesTest() override = default;

  void SetUpCommandLine(base::CommandLine* cmd) override {
    OptimizationGuideKeyedServiceDataSaverUserWithInfobarShownTest::
        SetUpCommandLine(cmd);

    cmd->AppendSwitchASCII(optimization_guide::switches::kFetchHintsOverride,
                           "whatever.com,somehost.com");
  }
};

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceCommandLineOverridesTest,
                       TopHostProviderIsSentDown) {
  ASSERT_TRUE(top_host_provider());

  std::vector<std::string> top_hosts = top_host_provider()->GetTopHosts();
  EXPECT_EQ(2ul, top_hosts.size());
  EXPECT_EQ("whatever.com", top_hosts[0]);
  EXPECT_EQ("somehost.com", top_hosts[1]);

  // ChromeOS has multiple profiles and optimization guide currently does not
  // run on non-Android.
#if !defined(OS_CHROMEOS)
  histogram_tester()->ExpectUniqueSample(
      "OptimizationGuide.RemoteFetchingEnabled", true, 1);
#endif
}

class OptimizationGuideKeyedServiceTargetPredictionEnabledBrowserTest
    : public OptimizationGuideKeyedServiceBrowserTest {
 public:
  OptimizationGuideKeyedServiceTargetPredictionEnabledBrowserTest() = default;
  ~OptimizationGuideKeyedServiceTargetPredictionEnabledBrowserTest() override =
      default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {optimization_guide::features::kRemoteOptimizationGuideFetching,
         optimization_guide::features::kOptimizationTargetPrediction},
        {});

    OptimizationGuideKeyedServiceBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitchASCII(optimization_guide::switches::kFetchHintsOverride,
                           "whatever.com,somehost.com");
  }

  void TearDown() override {
    OptimizationGuideKeyedServiceBrowserTest::TearDown();

    scoped_feature_list_.Reset();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    OptimizationGuideKeyedServiceTargetPredictionEnabledBrowserTest,
    PredictionManagerIsCreated) {
  ASSERT_TRUE(prediction_manager());
}

IN_PROC_BROWSER_TEST_F(
    OptimizationGuideKeyedServiceTargetPredictionEnabledBrowserTest,
    PredictionManagerDecisionOverridesHintsManager) {
  PushHintsComponentAndWaitForCompletion();
  RegisterWithKeyedService();

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), url_with_hints());

  EXPECT_EQ(RetryForHistogramUntilCountReached(
                histogram_tester, "OptimizationGuide.LoadedHint.Result", 1),
            1);
  // There should be a hint that matches this URL.
  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      true, 1);
  EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kUnknown,
            last_should_target_navigation_decision());
  EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kTrue,
            last_can_apply_optimization_decision());

  // Navigate away so metrics get recorded.
  ui_test_utils::NavigateToURL(browser(), url_with_hints());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.TargetDecision.PainfulPageLoad",
      static_cast<int>(optimization_guide::OptimizationTargetDecision::
                           kModelNotAvailableOnClient),
      1);
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry,
      ukm::builders::OptimizationGuide::kRegisteredOptimizationTargetsName));
  // PAINFUL_PAGE_LOAD = 1 so bit mask should be 10 which equals 2.
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::OptimizationGuide::kRegisteredOptimizationTargetsName, 2);
}
