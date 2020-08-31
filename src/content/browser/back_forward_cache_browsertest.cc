// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <unordered_map>

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/hash/hash.h"
#include "base/location.h"
#include "base/metrics/metrics_hashes.h"
#include "base/optional.h"
#include "base/strings/string_piece_forward.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/frame_host/back_forward_cache_impl.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/generic_sensor/sensor_provider_proxy_impl.h"
#include "content/browser/presentation/presentation_test_utils.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/frame_service_base.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_javascript_dialog_manager.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/echo.mojom.h"
#include "media/base/media_switches.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "services/device/public/cpp/test/fake_sensor_and_provider.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "services/device/public/mojom/vibration_manager.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"
#include "third_party/blink/public/mojom/app_banner/app_banner.mojom.h"

using testing::_;
using testing::Each;
using testing::ElementsAre;
using testing::Not;
using testing::UnorderedElementsAreArray;

namespace content {

namespace {

const char* kDisabledReasonForTest = "DisabledByBackForwardCacheBrowserTest";

// hash for std::unordered_map.
struct FeatureHash {
  size_t operator()(base::Feature feature) const {
    return base::FastHash(feature.name);
  }
};

// compare operator for std::unordered_map.
struct FeatureEqualOperator {
  bool operator()(base::Feature feature1, base::Feature feature2) const {
    return std::strcmp(feature1.name, feature2.name) == 0;
  }
};

// Test about the BackForwardCache.
class BackForwardCacheBrowserTest : public ContentBrowserTest,
                                    public WebContentsObserver {
 public:
  ~BackForwardCacheBrowserTest() override = default;

 protected:
  using UkmMetrics = ukm::TestUkmRecorder::HumanReadableUkmMetrics;

  // Disables checking metrics that are recorded recardless of the domains. By
  // default, this class' Expect* function checks the metrics both for the
  // specific domain and for all domains at the same time. In the case when the
  // test results need to be different, call this function.
  void DisableCheckingMetricsForAllSites() { check_all_sites_ = false; }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeUIForMediaStream);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kIgnoreCertificateErrors);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
#if defined(OS_LINUX)
    // Due to problems with PulseAudio failing to start, use a fake audio
    // stream. https://crbug.com/1047655#c70
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableAudioOutput);
#endif
    // TODO(sreejakshetty): Initialize ScopedFeatureLists from test constructor.
    EnableFeatureAndSetParams(features::kBackForwardCache,
                              "TimeToLiveInBackForwardCacheInSeconds", "3600");
#if defined(OS_ANDROID)
    EnableFeatureAndSetParams(features::kBackForwardCache,
                              "process_binding_strength", "NORMAL");
#endif
    SetupFeaturesAndParameters();

    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures, "WakeLock");
    ContentBrowserTest::SetUpCommandLine(command_line);
  }

  void SetupFeaturesAndParameters() {
    std::vector<base::test::ScopedFeatureList::FeatureAndParams>
        enabled_features;

    for (auto feature_param = features_with_params_.begin();
         feature_param != features_with_params_.end(); feature_param++) {
      enabled_features.push_back({feature_param->first, feature_param->second});
    }

    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features_);
  }

  void EnableFeatureAndSetParams(base::Feature feature,
                                 std::string param_name,
                                 std::string param_value) {
    features_with_params_[feature][param_name] = param_value;
  }

  void DisableFeature(base::Feature feature) {
    disabled_features_.push_back(feature);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    // TestAutoSetUkmRecorder's constructor requires a sequenced context.
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    ContentBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    ukm_recorder_.reset();
    ContentBrowserTest::TearDownOnMainThread();
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* current_frame_host() {
    return web_contents()->GetFrameTree()->root()->current_frame_host();
  }

  RenderFrameHostManager* render_frame_host_manager() {
    return web_contents()->GetFrameTree()->root()->render_manager();
  }

  std::string DepictFrameTree(FrameTreeNode* node) {
    return visualizer_.DepictFrameTree(node);
  }

  void ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome outcome,
                     base::Location location) {
    base::HistogramBase::Sample sample = base::HistogramBase::Sample(outcome);
    AddSampleToBuckets(&expected_outcomes_, sample);

    EXPECT_THAT(histogram_tester_.GetAllSamples(
                    "BackForwardCache.HistoryNavigationOutcome"),
                UnorderedElementsAreArray(expected_outcomes_))
        << location.ToString();

    if (!check_all_sites_)
      return;

    EXPECT_THAT(histogram_tester_.GetAllSamples(
                    "BackForwardCache.AllSites.HistoryNavigationOutcome"),
                UnorderedElementsAreArray(expected_outcomes_))
        << location.ToString();

    std::string is_served_from_bfcache =
        "BackForwardCache.IsServedFromBackForwardCache";
    bool ukm_outcome =
        outcome == BackForwardCacheMetrics::HistoryNavigationOutcome::kRestored;
    expected_ukm_outcomes_.push_back(
        {{is_served_from_bfcache, static_cast<int64_t>(ukm_outcome)}});
    EXPECT_THAT(ukm_recorder_->GetMetrics("HistoryNavigation",
                                          {is_served_from_bfcache}),
                expected_ukm_outcomes_)
        << location.ToString();
  }

  void ExpectOutcomeDidNotChange(base::Location location) {
    EXPECT_EQ(expected_outcomes_,
              histogram_tester_.GetAllSamples(
                  "BackForwardCache.HistoryNavigationOutcome"))
        << location.ToString();

    if (!check_all_sites_)
      return;

    EXPECT_EQ(expected_outcomes_,
              histogram_tester_.GetAllSamples(
                  "BackForwardCache.AllSites.HistoryNavigationOutcome"))
        << location.ToString();

    std::string is_served_from_bfcache =
        "BackForwardCache.IsServedFromBackForwardCache";
    EXPECT_THAT(ukm_recorder_->GetMetrics("HistoryNavigation",
                                          {is_served_from_bfcache}),
                expected_ukm_outcomes_)
        << location.ToString();
  }

  void ExpectNotRestored(
      std::vector<BackForwardCacheMetrics::NotRestoredReason> reasons,
      base::Location location) {
    uint64_t not_restored_reasons_bits = 0;
    for (BackForwardCacheMetrics::NotRestoredReason reason : reasons) {
      base::HistogramBase::Sample sample = base::HistogramBase::Sample(reason);
      AddSampleToBuckets(&expected_not_restored_, sample);
      not_restored_reasons_bits |= 1 << static_cast<int>(reason);
    }

    EXPECT_THAT(histogram_tester_.GetAllSamples(
                    "BackForwardCache.HistoryNavigationOutcome."
                    "NotRestoredReason"),
                UnorderedElementsAreArray(expected_not_restored_))
        << location.ToString();

    if (!check_all_sites_)
      return;

    EXPECT_THAT(histogram_tester_.GetAllSamples(
                    "BackForwardCache.AllSites.HistoryNavigationOutcome."
                    "NotRestoredReason"),
                UnorderedElementsAreArray(expected_not_restored_))
        << location.ToString();

    std::string not_restored_reasons = "BackForwardCache.NotRestoredReasons";
    expected_ukm_not_restored_reasons_.push_back(
        {{not_restored_reasons, not_restored_reasons_bits}});
    EXPECT_THAT(
        ukm_recorder_->GetMetrics("HistoryNavigation", {not_restored_reasons}),
        expected_ukm_not_restored_reasons_)
        << location.ToString();
  }

  void ExpectNotRestoredDidNotChange(base::Location location) {
    EXPECT_EQ(expected_not_restored_,
              histogram_tester_.GetAllSamples(
                  "BackForwardCache.HistoryNavigationOutcome."
                  "NotRestoredReason"))
        << location.ToString();

    std::string not_restored_reasons = "BackForwardCache.NotRestoredReasons";

    if (!check_all_sites_)
      return;

    EXPECT_EQ(expected_not_restored_,
              histogram_tester_.GetAllSamples(
                  "BackForwardCache.AllSites.HistoryNavigationOutcome."
                  "NotRestoredReason"))
        << location.ToString();

    EXPECT_THAT(
        ukm_recorder_->GetMetrics("HistoryNavigation", {not_restored_reasons}),
        expected_ukm_not_restored_reasons_)
        << location.ToString();
  }

  void ExpectBlocklistedFeature(
      blink::scheduler::WebSchedulerTrackedFeature feature,
      base::Location location) {
    base::HistogramBase::Sample sample = base::HistogramBase::Sample(feature);
    AddSampleToBuckets(&expected_blocklisted_features_, sample);

    EXPECT_THAT(histogram_tester_.GetAllSamples(
                    "BackForwardCache.HistoryNavigationOutcome."
                    "BlocklistedFeature"),
                UnorderedElementsAreArray(expected_blocklisted_features_))
        << location.ToString();

    if (!check_all_sites_)
      return;

    EXPECT_THAT(histogram_tester_.GetAllSamples(
                    "BackForwardCache.AllSites.HistoryNavigationOutcome."
                    "BlocklistedFeature"),
                UnorderedElementsAreArray(expected_blocklisted_features_))
        << location.ToString();
  }

  void ExpectDisabledWithReason(const std::string& reason,
                                base::Location location) {
    base::HistogramBase::Sample sample =
        base::HistogramBase::Sample(base::HashMetricName(reason));
    AddSampleToBuckets(&expected_disabled_reasons_, sample);

    EXPECT_THAT(histogram_tester_.GetAllSamples(
                    "BackForwardCache.HistoryNavigationOutcome."
                    "DisabledForRenderFrameHostReason"),
                UnorderedElementsAreArray(expected_disabled_reasons_))
        << location.ToString();
  }

  void ExpectEvictedAfterCommitted(
      std::vector<BackForwardCacheMetrics::EvictedAfterDocumentRestoredReason>
          reasons,
      base::Location location) {
    for (BackForwardCacheMetrics::EvictedAfterDocumentRestoredReason reason :
         reasons) {
      base::HistogramBase::Sample sample = base::HistogramBase::Sample(reason);
      AddSampleToBuckets(&expected_eviction_after_committing_, sample);
    }

    EXPECT_THAT(histogram_tester_.GetAllSamples(
                    "BackForwardCache.EvictedAfterDocumentRestoredReason"),
                UnorderedElementsAreArray(expected_eviction_after_committing_))
        << location.ToString();
    if (!check_all_sites_)
      return;

    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            "BackForwardCache.AllSites.EvictedAfterDocumentRestoredReason"),
        UnorderedElementsAreArray(expected_eviction_after_committing_))
        << location.ToString();
  }

  void EvictByJavaScript(RenderFrameHostImpl* rfh) {
    // Run JavaScript on a page in the back-forward cache. The page should be
    // evicted. As the frame is deleted, ExecJs returns false without executing.
    EXPECT_FALSE(ExecJs(rfh, "console.log('hi');"));
  }

  void StartRecordingEvents(RenderFrameHostImpl* rfh) {
    EXPECT_TRUE(ExecJs(rfh, R"(
      window.testObservedEvents = [];
      let event_list = [
        'visibilitychange',
        'pagehide',
        'pageshow',
        'freeze',
        'resume',
      ];
      for (event_name of event_list) {
        let result = event_name;
        window.addEventListener(event_name, event => {
          if (event.persisted)
            result += '.persisted';
          window.testObservedEvents.push('window.' + result);
        });
        document.addEventListener(event_name,
            () => window.testObservedEvents.push('document.' + result));
      }
    )"));
  }

  void MatchEventList(RenderFrameHostImpl* rfh,
                      base::ListValue list,
                      base::Location location = base::Location::Current()) {
    EXPECT_EQ(list, EvalJs(rfh, "window.testObservedEvents"))
        << location.ToString();
  }

  // Creates a minimal HTTPS server, accessible through https_server().
  // Returns a pointer to the server.
  net::EmbeddedTestServer* CreateHttpsServer() {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->AddDefaultHandlers(GetTestDataFilePath());
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    return https_server();
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  base::HistogramTester histogram_tester_;

 private:
  void AddSampleToBuckets(std::vector<base::Bucket>* buckets,
                          base::HistogramBase::Sample sample) {
    auto it = std::find_if(
        buckets->begin(), buckets->end(),
        [sample](const base::Bucket& bucket) { return bucket.min == sample; });
    if (it == buckets->end()) {
      buckets->push_back(base::Bucket(sample, 1));
    } else {
      it->count++;
    }
  }

  base::test::ScopedFeatureList feature_list_;

  FrameTreeVisualizer visualizer_;
  std::vector<base::Bucket> expected_outcomes_;
  std::vector<base::Bucket> expected_not_restored_;
  std::vector<base::Bucket> expected_blocklisted_features_;
  std::vector<base::Bucket> expected_disabled_reasons_;
  std::vector<base::Bucket> expected_eviction_after_committing_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unordered_map<base::Feature,
                     std::map<std::string, std::string>,
                     FeatureHash,
                     FeatureEqualOperator>
      features_with_params_;
  std::vector<base::Feature> disabled_features_;

  std::vector<UkmMetrics> expected_ukm_outcomes_;
  std::vector<UkmMetrics> expected_ukm_not_restored_reasons_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;

  // Indicates whether metrics for all sites regardless of the domains are
  // checked or not.
  bool check_all_sites_ = true;
};

// Match RenderFrameHostImpl* that are in the BackForwardCache.
MATCHER(InBackForwardCache, "") {
  return arg->IsInBackForwardCache();
}

// Match RenderFrameDeleteObserver* which observed deletion of the RenderFrame.
MATCHER(Deleted, "") {
  return arg->deleted();
}

// Helper function to pass an initializer list to the EXPECT_THAT macro. This is
// indeed the identity function.
std::initializer_list<RenderFrameHostImpl*> Elements(
    std::initializer_list<RenderFrameHostImpl*> t) {
  return t;
}

// Execute a custom callback when navigation is ready to commit. This is
// useful for simulating race conditions happening when a page enters the
// BackForwardCache and receive inflight messages sent when it wasn't frozen
// yet.
class ReadyToCommitNavigationCallback : public WebContentsObserver {
 public:
  ReadyToCommitNavigationCallback(
      WebContents* content,
      base::OnceCallback<void(NavigationHandle*)> callback)
      : WebContentsObserver(content), callback_(std::move(callback)) {}

 private:
  // WebContentsObserver:
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override {
    if (callback_)
      std::move(callback_).Run(navigation_handle);
  }

  base::OnceCallback<void(NavigationHandle*)> callback_;

  DISALLOW_COPY_AND_ASSIGN(ReadyToCommitNavigationCallback);
};

class FirstVisuallyNonEmptyPaintObserver : public WebContentsObserver {
 public:
  explicit FirstVisuallyNonEmptyPaintObserver(WebContents* contents)
      : WebContentsObserver(contents) {}
  void DidFirstVisuallyNonEmptyPaint() override {
    if (observed_)
      return;
    observed_ = true;
    run_loop_.Quit();
  }

  bool did_fire() const { return observed_; }

  void Wait() { run_loop_.Run(); }

 private:
  bool observed_ = false;
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
};

void WaitForFirstVisuallyNonEmptyPaint(WebContents* contents) {
  if (contents->CompletedFirstVisuallyNonEmptyPaint())
    return;
  FirstVisuallyNonEmptyPaintObserver observer(contents);
  observer.Wait();
}

class ThemeColorObserver : public WebContentsObserver {
 public:
  explicit ThemeColorObserver(WebContents* contents)
      : WebContentsObserver(contents) {}
  void DidChangeThemeColor() override { observed_ = true; }

  bool did_fire() const { return observed_; }

 private:
  bool observed_ = false;
};

class DOMContentLoadedObserver : public WebContentsObserver {
 public:
  explicit DOMContentLoadedObserver(RenderFrameHostImpl* render_frame_host)
      : WebContentsObserver(
            WebContents::FromRenderFrameHost(render_frame_host)),
        render_frame_host_(render_frame_host) {}

  void DOMContentLoaded(RenderFrameHost* render_frame_host) override {
    if (render_frame_host_ == render_frame_host)
      run_loop_.Quit();
  }

  void Wait() {
    if (render_frame_host_->IsDOMContentLoaded())
      run_loop_.Quit();
    run_loop_.Run();
  }

 private:
  RenderFrameHostImpl* render_frame_host_;
  base::RunLoop run_loop_;
};

void WaitForDOMContentLoaded(RenderFrameHostImpl* rfh) {
  DOMContentLoadedObserver observer(rfh);
  observer.Wait();
}

}  // namespace

// Navigate from A to B and go back.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, Basic) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  url::Origin origin_a = url::Origin::Create(url_a);
  url::Origin origin_b = url::Origin::Create(url_b);

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_EQ(rfh_a->GetVisibilityState(), PageVisibilityState::kHidden);
  EXPECT_EQ(origin_a, rfh_a->GetLastCommittedOrigin());
  EXPECT_EQ(origin_b, rfh_b->GetLastCommittedOrigin());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());
  EXPECT_EQ(rfh_b->GetVisibilityState(), PageVisibilityState::kVisible);

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_EQ(origin_a, rfh_a->GetLastCommittedOrigin());
  EXPECT_EQ(origin_b, rfh_b->GetLastCommittedOrigin());
  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_EQ(rfh_a->GetVisibilityState(), PageVisibilityState::kVisible);
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  EXPECT_EQ(rfh_b->GetVisibilityState(), PageVisibilityState::kHidden);

  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kRestored,
                FROM_HERE);
}

// Navigate from A to B and go back.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, BasicDocumentInitiated) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(ExecJs(shell(), JsReplace("location = $1;", url_b.spec())));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());

  // The two pages are using different BrowsingInstances.
  EXPECT_FALSE(rfh_a->GetSiteInstance()->IsRelatedSiteInstance(
      rfh_b->GetSiteInstance()));

  // 3) Go back to A.
  EXPECT_TRUE(ExecJs(shell(), "history.back();"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());

  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kRestored,
                FROM_HERE);
}

// Navigate from back and forward repeatedly.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       NavigateBackForwardRepeatedly) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());

  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kRestored,
                FROM_HERE);

  // 4) Go forward to B.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(rfh_b, current_frame_host());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());

  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kRestored,
                FROM_HERE);

  // 5) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());

  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kRestored,
                FROM_HERE);

  // 6) Go forward to B.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(rfh_b, current_frame_host());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());

  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());

  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kRestored,
                FROM_HERE);
}

// The BackForwardCache does not cache same-website navigations for now.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DoesNotCacheSameWebsiteNavigations) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a2(embedded_test_server()->GetURL("a.com", "/title2.html"));

  // 1) Navigate to A1.
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));
  RenderFrameHostImpl* rfh_a1 = current_frame_host();
  RenderFrameDeletedObserver delete_rfh_a1(rfh_a1);
  int browsing_instance_id = rfh_a1->GetSiteInstance()->GetBrowsingInstanceId();

  // 2) Navigate to A2.
  EXPECT_TRUE(NavigateToURL(shell(), url_a2));
  RenderFrameHostImpl* rfh_a2 = current_frame_host();
  // The BrowsingInstance shouldn't have changed.
  EXPECT_EQ(browsing_instance_id,
            rfh_a2->GetSiteInstance()->GetBrowsingInstanceId());
  EXPECT_FALSE(rfh_a1->IsInBackForwardCache());
  // The main frame should have been reused for the navigation.
  EXPECT_EQ(rfh_a1, rfh_a2);
}

// The current page can't enter the BackForwardCache if another page can script
// it. This can happen when one document opens a popup using window.open() for
// instance. It prevents the BackForwardCache from being used.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, WindowOpen) {
  // This test assumes cross-site navigation staying in the same
  // BrowsingInstance to use a different SiteInstance. Otherwise, it will
  // timeout at step 2).
  if (!SiteIsolationPolicy::UseDedicatedProcessesForAllSites())
    return;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A and open a popup.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  EXPECT_EQ(1u, rfh_a->GetSiteInstance()->GetRelatedActiveContentsCount());
  Shell* popup = OpenPopup(rfh_a, url_a, "");
  EXPECT_EQ(2u, rfh_a->GetSiteInstance()->GetRelatedActiveContentsCount());

  // 2) Navigate to B. The previous document can't enter the BackForwardCache,
  // because of the popup.
  EXPECT_TRUE(ExecJs(rfh_a, JsReplace("location = $1;", url_b.spec())));
  delete_observer_rfh_a.WaitUntilDeleted();
  RenderFrameHostImpl* rfh_b = current_frame_host();
  EXPECT_EQ(2u, rfh_b->GetSiteInstance()->GetRelatedActiveContentsCount());

  // 3) Go back to A. The previous document can't enter the BackForwardCache,
  // because of the popup.
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);
  EXPECT_TRUE(ExecJs(rfh_b, "history.back();"));
  delete_observer_rfh_b.WaitUntilDeleted();

  // 4) Make the popup drop the window.opener connection. It happens when the
  //    user does an omnibox-initiated navigation, which happens in a new
  //    BrowsingInstance.
  RenderFrameHostImpl* rfh_a_new = current_frame_host();
  EXPECT_EQ(2u, rfh_a_new->GetSiteInstance()->GetRelatedActiveContentsCount());
  EXPECT_TRUE(NavigateToURL(popup, url_b));
  EXPECT_EQ(1u, rfh_a_new->GetSiteInstance()->GetRelatedActiveContentsCount());

  // 5) Navigate to B again. As the scripting relationship with the popup is
  // now severed, the current page (|rfh_a_new|) can enter back-forward cache.
  RenderFrameDeletedObserver delete_observer_rfh_a_new(rfh_a_new);
  EXPECT_TRUE(ExecJs(rfh_a_new, JsReplace("location = $1;", url_b.spec())));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a_new.deleted());
  EXPECT_TRUE(rfh_a_new->IsInBackForwardCache());

  // 6) Go back to A. The current document can finally enter the
  // BackForwardCache, because it is alone in its BrowsingInstance and has never
  // been related to any other document.
  RenderFrameHostImpl* rfh_b_new = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b_new(rfh_b_new);
  EXPECT_TRUE(ExecJs(rfh_b_new, "history.back();"));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_FALSE(delete_observer_rfh_b_new.deleted());
  EXPECT_TRUE(rfh_b_new->IsInBackForwardCache());
}

// Navigate from A(B) to C and go back.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, BasicIframe) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1) Navigate to A(B).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  // 2) Navigate to C.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  RenderFrameHostImpl* rfh_c = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_c(rfh_c);
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  EXPECT_FALSE(rfh_c->IsInBackForwardCache());

  // 3) Go back to A(B).
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_FALSE(delete_observer_rfh_c.deleted());
  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());
  EXPECT_TRUE(rfh_c->IsInBackForwardCache());

  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kRestored,
                FROM_HERE);
}

// Ensure flushing the BackForwardCache works properly.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, BackForwardCacheFlush) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);
  EXPECT_FALSE(delete_observer_rfh_a.deleted());

  // 3) Flush A.
  web_contents()->GetController().GetBackForwardCache().Flush();
  delete_observer_rfh_a.WaitUntilDeleted();
  EXPECT_FALSE(delete_observer_rfh_b.deleted());

  // 4) Go back to a new A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_FALSE(delete_observer_rfh_b.deleted());

  // 5) Flush B.
  web_contents()->GetController().GetBackForwardCache().Flush();
  delete_observer_rfh_b.WaitUntilDeleted();
}

// Check the visible URL in the omnibox is properly updated when restoring a
// document from the BackForwardCache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, VisibleURL) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Go to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  // 2) Go to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(url_a, web_contents()->GetVisibleURL());

  // 4) Go forward to B.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(url_b, web_contents()->GetVisibleURL());
}

// Test only 1 document is kept in the at a time BackForwardCache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       CacheSizeLimitedToOneDocumentPerTab) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  // BackForwardCache is empty.
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  // BackForwardCache contains only rfh_a.
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  // BackForwardCache contains only rfh_b.
  delete_observer_rfh_a.WaitUntilDeleted();
  EXPECT_FALSE(delete_observer_rfh_b.deleted());

  // If/when the cache size is increased, this can be tested iteratively, see
  // deleted code in: https://crrev.com/c/1782902.

  web_contents()->GetController().GoToOffset(-2);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kNotRestored,
                FROM_HERE);
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::kCacheLimit},
                    FROM_HERE);
}

// Test documents are evicted from the BackForwardCache at some point.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       CacheEvictionWithIncreasedCacheSize) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // The number of document the BackForwardCache can hold per tab.
  size_t kBackForwardCacheLimit = 5;
  web_contents()
      ->GetController()
      .GetBackForwardCache()
      .set_cache_size_limit_for_testing(kBackForwardCacheLimit);

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_a));  // BackForwardCache size is 0.
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  EXPECT_TRUE(NavigateToURL(shell(), url_b));  // BackForwardCache size is 1.
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  for (size_t i = 2; i < kBackForwardCacheLimit; ++i) {
    EXPECT_TRUE(NavigateToURL(shell(), i % 2 ? url_b : url_a));
    // After |i+1| navigations, |i| documents went into the BackForwardCache.
    // When |i| is greater than the BackForwardCache size limit, they are
    // evicted:
    EXPECT_EQ(i >= kBackForwardCacheLimit + 1, delete_observer_rfh_a.deleted());
    EXPECT_EQ(i >= kBackForwardCacheLimit + 2, delete_observer_rfh_b.deleted());
  }
}

// Similar to BackForwardCacheBrowserTest.SubframeSurviveCache*
// Test case: a1(b2) -> c3 -> a1(b2)
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, SubframeSurviveCache1) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  std::vector<RenderFrameDeletedObserver*> rfh_observer;

  // 1) Navigate to a1(b2).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* a1 = current_frame_host();
  RenderFrameHostImpl* b2 = a1->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver a1_observer(a1), b2_observer(b2);
  rfh_observer.insert(rfh_observer.end(), {&a1_observer, &b2_observer});
  EXPECT_TRUE(ExecJs(b2, "window.alive = 'I am alive';"));

  // 2) Navigate to c3.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  RenderFrameHostImpl* c3 = current_frame_host();
  RenderFrameDeletedObserver c3_observer(c3);
  rfh_observer.push_back(&c3_observer);
  ASSERT_THAT(rfh_observer, Each(Not(Deleted())));
  EXPECT_THAT(Elements({a1, b2}), Each(InBackForwardCache()));
  EXPECT_THAT(c3, Not(InBackForwardCache()));

  // 3) Go back to a1(b2).
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ASSERT_THAT(rfh_observer, Each(Not(Deleted())));
  EXPECT_THAT(Elements({a1, b2}), Each(Not(InBackForwardCache())));
  EXPECT_THAT(c3, InBackForwardCache());

  // Even after a new IPC round trip with the renderer, b2 must still be alive.
  EXPECT_EQ("I am alive", EvalJs(b2, "window.alive"));
  EXPECT_FALSE(b2_observer.deleted());

  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kRestored,
                FROM_HERE);
}

// Similar to BackForwardCacheBrowserTest.SubframeSurviveCache*
// Test case: a1(b2) -> b3 -> a1(b2).
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, SubframeSurviveCache2) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  std::vector<RenderFrameDeletedObserver*> rfh_observer;

  // 1) Navigate to a1(b2).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* a1 = current_frame_host();
  RenderFrameHostImpl* b2 = a1->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver a1_observer(a1), b2_observer(b2);
  rfh_observer.insert(rfh_observer.end(), {&a1_observer, &b2_observer});
  EXPECT_TRUE(ExecJs(b2, "window.alive = 'I am alive';"));

  // 2) Navigate to b3.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* b3 = current_frame_host();
  RenderFrameDeletedObserver b3_observer(b3);
  rfh_observer.push_back(&b3_observer);
  ASSERT_THAT(rfh_observer, Each(Not(Deleted())));
  EXPECT_THAT(Elements({a1, b2}), Each(InBackForwardCache()));
  EXPECT_THAT(b3, Not(InBackForwardCache()));

  // 3) Go back to a1(b2).
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ASSERT_THAT(rfh_observer, Each(Not(Deleted())));
  EXPECT_EQ(a1, current_frame_host());
  EXPECT_THAT(Elements({a1, b2}), Each(Not(InBackForwardCache())));
  EXPECT_THAT(b3, InBackForwardCache());

  // Even after a new IPC round trip with the renderer, b2 must still be alive.
  EXPECT_EQ("I am alive", EvalJs(b2, "window.alive"));
  EXPECT_FALSE(b2_observer.deleted());

  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kRestored,
                FROM_HERE);
}

// Similar to BackForwardCacheBrowserTest.tSubframeSurviveCache*
// Test case: a1(b2) -> b3(a4) -> a1(b2) -> b3(a4)
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, SubframeSurviveCache3) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_b(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(a)"));

  std::vector<RenderFrameDeletedObserver*> rfh_observer;

  // 1) Navigate to a1(b2).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* a1 = current_frame_host();
  RenderFrameHostImpl* b2 = a1->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver a1_observer(a1), b2_observer(b2);
  rfh_observer.insert(rfh_observer.end(), {&a1_observer, &b2_observer});
  EXPECT_TRUE(ExecJs(b2, "window.alive = 'I am alive';"));

  // 2) Navigate to b3(a4)
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* b3 = current_frame_host();
  RenderFrameHostImpl* a4 = b3->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver b3_observer(b3), a4_observer(a4);
  rfh_observer.insert(rfh_observer.end(), {&b3_observer, &a4_observer});
  ASSERT_THAT(rfh_observer, Each(Not(Deleted())));
  EXPECT_THAT(Elements({a1, b2}), Each(InBackForwardCache()));
  EXPECT_THAT(Elements({b3, a4}), Each(Not(InBackForwardCache())));
  EXPECT_TRUE(ExecJs(a4, "window.alive = 'I am alive';"));

  // 3) Go back to a1(b2).
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ASSERT_THAT(rfh_observer, Each(Not(Deleted())));
  EXPECT_EQ(a1, current_frame_host());
  EXPECT_THAT(Elements({a1, b2}), Each(Not(InBackForwardCache())));
  EXPECT_THAT(Elements({b3, a4}), Each(InBackForwardCache()));

  // Even after a new IPC round trip with the renderer, b2 must still be alive.
  EXPECT_EQ("I am alive", EvalJs(b2, "window.alive"));
  EXPECT_FALSE(b2_observer.deleted());

  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kRestored,
                FROM_HERE);

  // 4) Go forward to b3(a4).
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ASSERT_THAT(rfh_observer, Each(Not(Deleted())));
  EXPECT_EQ(b3, current_frame_host());
  EXPECT_THAT(Elements({a1, b2}), Each(InBackForwardCache()));
  EXPECT_THAT(Elements({b3, a4}), Each(Not(InBackForwardCache())));

  // Even after a new IPC round trip with the renderer, a4 must still be alive.
  EXPECT_EQ("I am alive", EvalJs(a4, "window.alive"));
  EXPECT_FALSE(a4_observer.deleted());

  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kRestored,
                FROM_HERE);
}

// Similar to BackForwardCacheBrowserTest.SubframeSurviveCache*
// Test case: a1(b2) -> b3 -> a4 -> b5 -> a1(b2).
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, SubframeSurviveCache4) {
  // Increase the cache size so that a1(b2) is still in the cache when we
  // reach b5.
  web_contents()
      ->GetController()
      .GetBackForwardCache()
      .set_cache_size_limit_for_testing(3);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_ab(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  std::vector<RenderFrameDeletedObserver*> rfh_observer;

  // 1) Navigate to a1(b2).
  EXPECT_TRUE(NavigateToURL(shell(), url_ab));
  RenderFrameHostImpl* a1 = current_frame_host();
  RenderFrameHostImpl* b2 = a1->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver a1_observer(a1), b2_observer(b2);
  rfh_observer.insert(rfh_observer.end(), {&a1_observer, &b2_observer});
  EXPECT_TRUE(ExecJs(b2, "window.alive = 'I am alive';"));

  // 2) Navigate to b3.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* b3 = current_frame_host();
  RenderFrameDeletedObserver b3_observer(b3);
  rfh_observer.push_back(&b3_observer);
  ASSERT_THAT(rfh_observer, Each(Not(Deleted())));
  EXPECT_THAT(Elements({a1, b2}), Each(InBackForwardCache()));
  EXPECT_THAT(b3, Not(InBackForwardCache()));

  // 3) Navigate to a4.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* a4 = current_frame_host();
  RenderFrameDeletedObserver a4_observer(a4);
  rfh_observer.push_back(&a4_observer);
  ASSERT_THAT(rfh_observer, Each(Not(Deleted())));

  // 4) Navigate to b5
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* b5 = current_frame_host();
  RenderFrameDeletedObserver b5_observer(b5);
  rfh_observer.push_back(&b5_observer);
  ASSERT_THAT(rfh_observer, Each(Not(Deleted())));
  EXPECT_THAT(Elements({a1, b2, b3, a4}), Each(InBackForwardCache()));
  EXPECT_THAT(b5, Not(InBackForwardCache()));

  // 3) Go back to a1(b2).
  web_contents()->GetController().GoToOffset(-3);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(a1, current_frame_host());
  ASSERT_THAT(rfh_observer, Each(Not(Deleted())));
  EXPECT_THAT(Elements({b3, a4, b5}), Each(InBackForwardCache()));
  EXPECT_THAT(Elements({a1, b2}), Each(Not(InBackForwardCache())));

  // Even after a new IPC round trip with the renderer, b2 must still be alive.
  EXPECT_EQ("I am alive", EvalJs(b2, "window.alive"));
  EXPECT_FALSE(b2_observer.deleted());
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       NavigationsAreFullyCommitted) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // During a navigation, the document being navigated *away from* can either be
  // deleted or stored into the BackForwardCache. The document being navigated
  // *to* can either be new or restored from the BackForwardCache.
  //
  // This test covers every combination:
  //
  //  1. Navigate to a cacheable page (()->A)
  //  2. Navigate to an uncacheable page (A->B)
  //  3. Go Back to a cached page (B->A)
  //  4. Navigate to a cacheable page (A->C)
  //  5. Go Back to a cached page (C->A)
  //
  // +-+-------+----------------+---------------+
  // |#|nav    | curr_document  | dest_document |
  // +-+-------+----------------+---------------|
  // |1|(()->A)| N/A            | new           |
  // |2|(A->B) | cached         | new           |
  // |3|(B->A) | deleted        | restored      |
  // |4|(A->C) | cached         | new           |
  // |5|(C->A) | cached         | restored      |
  // +-+-------+----------------+---------------+
  //
  // As part of these navigations we check that LastCommittedURL was updated,
  // to verify that the frame wasn't simply swapped in without actually
  // committing.

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL(
      "b.com", "/back_forward_cache/page_with_dedicated_worker.html"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1. Navigate to a cacheable page (A).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2. Navigate from a cacheable page to an uncacheable page (A->B).
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url_b);
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  // Page A should be in the cache.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3. Navigate from an uncacheable to a cached page page (B->A).
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url_a);

  // Page B should be deleted (not cached).
  delete_observer_rfh_b.WaitUntilDeleted();

  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kRestored,
                FROM_HERE);

  // 4. Navigate from a cacheable page to a cacheable page (A->C).
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url_c);
  RenderFrameHostImpl* rfh_c = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_c(rfh_c);

  // Page A should be in the cache.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 5. Navigate from a cacheable page to a cached page (C->A).
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url_a);

  // Page C should be in the cache.
  EXPECT_FALSE(delete_observer_rfh_c.deleted());
  EXPECT_TRUE(rfh_c->IsInBackForwardCache());

  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kRestored,
                FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       ProxiesAreStoredAndRestored) {
  // This test makes assumption about where iframe processes live.
  if (!AreAllSitesIsolatedForTesting())
    return;

  ASSERT_TRUE(embedded_test_server()->Start());

  // During a navigation, the document being navigated *away from* can either be
  // deleted or stored into the BackForwardCache. The document being navigated
  // *to* can either be new or restored from the BackForwardCache.
  //
  // This test covers every combination:
  //
  //  1. Navigate to a cacheable page (()->A)
  //  2. Navigate to an uncacheable page (A->B)
  //  3. Go Back to a cached page (B->A)
  //  4. Navigate to a cacheable page (A->C)
  //  5. Go Back to a cached page (C->A)
  //
  // +-+-------+----------------+---------------+
  // |#|nav    | curr_document  | dest_document |
  // +-+-------+----------------+---------------|
  // |1|(()->A)| N/A            | new           |
  // |2|(A->B) | cached         | new           |
  // |3|(B->A) | deleted        | restored      |
  // |4|(A->C) | cached         | new           |
  // |5|(C->A) | cached         | restored      |
  // +-+-------+----------------+---------------+
  //
  // We use pages with cross process iframes to verify that proxy storage and
  // retrieval works well in every possible combination.

  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(i,j)"));
  GURL url_b(embedded_test_server()->GetURL(
      "b.com", "/back_forward_cache/page_with_dedicated_worker.html"));
  GURL url_c(embedded_test_server()->GetURL(
      "c.com", "/cross_site_iframe_factory.html?c(k,l,m)"));

  NavigationControllerImpl& controller = web_contents()->GetController();
  BackForwardCacheImpl& cache = controller.GetBackForwardCache();

  // 1. Navigate to a cacheable page (A).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_EQ(2u, render_frame_host_manager()->GetProxyCount());
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  std::string frame_tree_a = DepictFrameTree(rfh_a->frame_tree_node());

  // 2. Navigate from a cacheable page to an uncacheable page (A->B).
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_EQ(0u, render_frame_host_manager()->GetProxyCount());
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  // Page A should be in the cache.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Verify proxies are stored as well.
  auto* cached_entry = cache.GetEntry(rfh_a->nav_entry_id());
  EXPECT_EQ(2u, cached_entry->proxy_hosts.size());

  // 3. Navigate from an uncacheable to a cached page page (B->A).
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  // Note: We still have a transition proxy that will be used to perform the
  // frame swap. It gets deleted with rfh_b below.
  EXPECT_EQ(3u, render_frame_host_manager()->GetProxyCount());

  // Page B should be deleted (not cached).
  delete_observer_rfh_b.WaitUntilDeleted();
  EXPECT_EQ(2u, render_frame_host_manager()->GetProxyCount());

  // Page A should still have the correct frame tree.
  EXPECT_EQ(frame_tree_a,
            DepictFrameTree(current_frame_host()->frame_tree_node()));

  // 4. Navigate from a cacheable page to a cacheable page (A->C).
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  EXPECT_EQ(3u, render_frame_host_manager()->GetProxyCount());
  RenderFrameHostImpl* rfh_c = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_c(rfh_c);

  // Page A should be in the cache.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Verify proxies are stored as well.
  cached_entry = cache.GetEntry(rfh_a->nav_entry_id());
  EXPECT_EQ(2u, cached_entry->proxy_hosts.size());

  // 5. Navigate from a cacheable page to a cached page (C->A).
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(2u, render_frame_host_manager()->GetProxyCount());

  // Page A should still have the correct frame tree.
  EXPECT_EQ(frame_tree_a,
            DepictFrameTree(current_frame_host()->frame_tree_node()));

  // Page C should be in the cache.
  EXPECT_FALSE(delete_observer_rfh_c.deleted());
  EXPECT_TRUE(rfh_c->IsInBackForwardCache());

  // Verify proxies are stored as well.
  cached_entry = cache.GetEntry(rfh_c->nav_entry_id());
  EXPECT_EQ(3u, cached_entry->proxy_hosts.size());
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       RestoredProxiesAreFunctional) {
  // This test makes assumption about where iframe processes live.
  if (!AreAllSitesIsolatedForTesting())
    return;

  ASSERT_TRUE(embedded_test_server()->Start());

  // Page A is cacheable, while page B is not.
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(z)"));
  GURL url_b(embedded_test_server()->GetURL(
      "b.com", "/back_forward_cache/page_with_dedicated_worker.html"));
  GURL test_url(embedded_test_server()->GetURL("c.com", "/title1.html"));

  NavigationControllerImpl& controller = web_contents()->GetController();

  // 1. Navigate to a cacheable page (A).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // 2. Navigate from a cacheable page to an uncacheable page (A->B).
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3. Navigate from an uncacheable to a cached page page (B->A).
  // This restores the top frame's proxy in the z.com (iframe's) process.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // 4. Verify that the main frame's z.com proxy is still functional.
  RenderFrameHostImpl* iframe =
      rfh_a->frame_tree_node()->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(iframe, "top.location.href = '" + test_url.spec() + "';"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // We expect to have navigated through the proxy.
  EXPECT_EQ(test_url, controller.GetLastCommittedEntry()->GetURL());
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       PageWithDedicatedWorkerNotCached) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL(
          "a.com", "/back_forward_cache/page_with_dedicated_worker.html")));
  RenderFrameDeletedObserver delete_observer_rfh_a(current_frame_host());

  // Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));

  // The page with the unsupported feature should be deleted (not cached).
  delete_observer_rfh_a.WaitUntilDeleted();
}

// TODO(https://crbug.com/154571): Shared workers are not available on Android.
#if defined(OS_ANDROID)
#define MAYBE_PageWithSharedWorkerNotCached \
  DISABLED_PageWithSharedWorkerNotCached
#else
#define MAYBE_PageWithSharedWorkerNotCached PageWithSharedWorkerNotCached
#endif
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       MAYBE_PageWithSharedWorkerNotCached) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL(
          "a.com", "/back_forward_cache/page_with_shared_worker.html")));
  RenderFrameDeletedObserver delete_observer_rfh_a(current_frame_host());

  // Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));

  // The page with the unsupported feature should be deleted (not cached).
  delete_observer_rfh_a.WaitUntilDeleted();

  // Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      FROM_HERE);
  ExpectBlocklistedFeature(
      blink::scheduler::WebSchedulerTrackedFeature::kSharedWorker, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       SubframeWithDisallowedFeatureNotCached) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to a page with an iframe that contains a dedicated worker.
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL(
          "a.com", "/back_forward_cache/dedicated_worker_in_subframe.html")));
  RenderFrameDeletedObserver delete_rfh_a(current_frame_host());

  // Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));

  // The page with the unsupported feature should be deleted (not cached).
  delete_rfh_a.WaitUntilDeleted();

  // Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      FROM_HERE);
  ExpectBlocklistedFeature(
      blink::scheduler::WebSchedulerTrackedFeature::kDedicatedWorkerOrWorklet,
      FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       SubframeWithOngoingNavigationNotCached) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/hung");
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to a page with an iframe.
  TestNavigationObserver navigation_observer1(web_contents());
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/back_forward_cache/page_with_hung_iframe.html"));
  shell()->LoadURL(main_url);
  navigation_observer1.WaitForNavigationFinished();

  RenderFrameHostImpl* main_frame = current_frame_host();
  RenderFrameDeletedObserver frame_deleted_observer(main_frame);
  response.WaitForRequest();

  // Navigate away.
  TestNavigationObserver navigation_observer2(web_contents());
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));
  navigation_observer2.WaitForNavigationFinished();

  // The page with the unsupported feature should be deleted (not cached).
  frame_deleted_observer.WaitUntilDeleted();
}

// Check that unload event handlers are not dispatched when the page goes
// into BackForwardCache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       ConfirmUnloadEventNotFired) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Set unload handler and check the title.
  EXPECT_TRUE(ExecJs(rfh_a,
                     "document.title = 'loaded!';"
                     "window.addEventListener('unload', () => {"
                     "  document.title = 'unloaded!';"
                     "});"));
  {
    base::string16 title_when_loaded = base::UTF8ToUTF16("loaded!");
    TitleWatcher title_watcher(web_contents(), title_when_loaded);
    EXPECT_EQ(title_watcher.WaitAndGetTitle(), title_when_loaded);
  }

  // 3) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());

  // 4) Go back to A and check the title again.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  {
    base::string16 title_when_loaded = base::UTF8ToUTF16("loaded!");
    TitleWatcher title_watcher(web_contents(), title_when_loaded);
    EXPECT_EQ(title_watcher.WaitAndGetTitle(), title_when_loaded);
  }
}

// Flaky on Linux: https://crbug.com/1054194
#if defined(OS_LINUX)
#define MAYBE_DoesNotCacheIfRecordingAudio DISABLED_DoesNotCacheIfRecordingAudio
#else
#define MAYBE_DoesNotCacheIfRecordingAudio DoesNotCacheIfRecordingAudio
#endif
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       MAYBE_DoesNotCacheIfRecordingAudio) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to an empty page.
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Request for audio recording.
  EXPECT_EQ("success", EvalJs(current_frame_host(), R"(
    new Promise(resolve => {
      navigator.mediaDevices.getUserMedia({audio: true})
        .then(m => { resolve("success"); })
        .catch(() => { resolve("error"); });
    });
  )"));

  RenderFrameDeletedObserver deleted(current_frame_host());

  // 2) Navigate away.
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // The page was still recording audio when we navigated away, so it shouldn't
  // have been cached.
  deleted.WaitUntilDeleted();

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kWasGrantedMediaAccess},
      FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DoesNotCacheIfSubframeRecordingAudio) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to a page with an iframe.
  GURL url(embedded_test_server()->GetURL("/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  RenderFrameHostImpl* rfh = current_frame_host();

  // Request for audio recording from the subframe.
  EXPECT_EQ("success", EvalJs(rfh->child_at(0)->current_frame_host(), R"(
    new Promise(resolve => {
      navigator.mediaDevices.getUserMedia({audio: true})
        .then(m => { resolve("success"); })
        .catch(() => { resolve("error"); });
    });
  )"));

  RenderFrameDeletedObserver deleted(current_frame_host());

  // 2) Navigate away.
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // The page was still recording audio when we navigated away, so it shouldn't
  // have been cached.
  deleted.WaitUntilDeleted();

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kWasGrantedMediaAccess},
      FROM_HERE);
}

// TODO(https://crbug.com/1075936) disabled due to flakiness
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DISABLED_DoesNotCacheIfMainFrameStillLoading) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page that doesn't finish loading.
  GURL url(embedded_test_server()->GetURL("a.com", "/main_document"));
  TestNavigationManager navigation_manager(shell()->web_contents(), url);
  shell()->LoadURL(url);

  // The navigation starts.
  EXPECT_TRUE(navigation_manager.WaitForRequestStart());
  navigation_manager.ResumeNavigation();

  // The server sends the first part of the response and waits.
  response.WaitForRequest();
  response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "<html><body> ... ");

  // The navigation finishes while the body is still loading.
  navigation_manager.WaitForNavigationFinished();
  RenderFrameDeletedObserver delete_observer_rfh_a(current_frame_host());

  // 2) Navigate away.
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // The page was still loading when we navigated away, so it shouldn't have
  // been cached.
  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_FALSE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::kLoading},
                    FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DoesNotCacheIfImageStillLoading) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with an image that loads forever.
  GURL url(embedded_test_server()->GetURL("a.com",
                                          "/infinitely_loading_image.html"));
  TestNavigationManager navigation_manager(shell()->web_contents(), url);
  shell()->LoadURL(url);

  // The navigation finishes while the image is still loading.
  navigation_manager.WaitForNavigationFinished();
  // Wait for the document to load DOM to ensure that kLoading is not
  // one of the reasons why the document wasn't cached.
  WaitForDOMContentLoaded(current_frame_host());

  RenderFrameDeletedObserver delete_observer_rfh_a(current_frame_host());

  // 2) Navigate away.
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // The page was still loading when we navigated away, so it shouldn't have
  // been cached.
  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back.
  TestNavigationManager navigation_manager_back(shell()->web_contents(), url);
  web_contents()->GetController().GoBack();
  navigation_manager_back.WaitForNavigationFinished();
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      FROM_HERE);
  ExpectBlocklistedFeature(blink::scheduler::WebSchedulerTrackedFeature::
                               kOutstandingNetworkRequestOthers,
                           FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DoesNotCacheLoadingSubframe) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/controlled");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with an iframe that loads forever.
  GURL url(embedded_test_server()->GetURL(
      "a.com", "/back_forward_cache/controllable_subframe.html"));
  TestNavigationManager navigation_manager(shell()->web_contents(), url);
  shell()->LoadURL(url);

  // The navigation finishes while the iframe is still loading.
  navigation_manager.WaitForNavigationFinished();

  // Wait for the iframe request to arrive, and leave it hanging with no
  // response.
  response.WaitForRequest();

  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate away.
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // The page should not have been added to cache, since it had a subframe that
  // was still loading at the time it was navigated away from.
  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {
          BackForwardCacheMetrics::NotRestoredReason::kLoading,
          BackForwardCacheMetrics::NotRestoredReason::kSubframeIsNavigating,
      },
      FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DoesNotCacheLoadingSubframeOfSubframe) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/controlled");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with an iframe that contains yet another iframe, that
  // hangs while loading.
  GURL url(embedded_test_server()->GetURL(
      "a.com", "/back_forward_cache/controllable_subframe_of_subframe.html"));
  TestNavigationManager navigation_manager(shell()->web_contents(), url);
  shell()->LoadURL(url);

  // The navigation finishes while the iframe within an iframe is still loading.
  navigation_manager.WaitForNavigationFinished();

  // Wait for the innermost iframe request to arrive, and leave it hanging with
  // no response.
  response.WaitForRequest();

  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_rfh_a(rfh_a);

  // 2) Navigate away.
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // The page should not have been added to the cache, since it had an iframe
  // that was still loading at the time it was navigated away from.
  delete_rfh_a.WaitUntilDeleted();

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {
          BackForwardCacheMetrics::NotRestoredReason::kLoading,
          BackForwardCacheMetrics::NotRestoredReason::kSubframeIsNavigating,
      },
      FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, DoesNotCacheIfWebGL) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with WebGL usage
  GURL url(embedded_test_server()->GetURL(
      "example.com", "/back_forward_cache/page_with_webgl.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameDeletedObserver delete_observer_rfh_a(current_frame_host());

  // 2) Navigate away.
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));
  delete_observer_rfh_a.WaitUntilDeleted();

  // The page had an active WebGL context when we navigated away,
  // so it shouldn't have been cached.

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      FROM_HERE);
  ExpectBlocklistedFeature(blink::scheduler::WebSchedulerTrackedFeature::kWebGL,
                           FROM_HERE);
}

// Since blink::mojom::HidService binder is not added in
// content/browser/browser_interface_binders.cc for Android, this test is not
// applicable for this OS.
#if !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, DoesNotCacheIfWebHID) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to an empty page.
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Request for HID devices.
  EXPECT_EQ("success", EvalJs(current_frame_host(), R"(
    new Promise(resolve => {
      navigator.hid.getDevices()
        .then(m => { resolve("success"); })
        .catch(() => { resolve("error"); });
    });
  )"));

  RenderFrameDeletedObserver deleted(current_frame_host());

  // 2) Navigate away.
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // The page uses WebHID so it should be deleted.
  deleted.WaitUntilDeleted();

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      FROM_HERE);
  ExpectBlocklistedFeature(
      blink::scheduler::WebSchedulerTrackedFeature::kWebHID, FROM_HERE);
}
#endif  // !defined(OS_ANDROID)

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DoesNotCacheIfAcquiredWakeLock) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with WakeLock usage.
  GURL url(embedded_test_server()->GetURL("/back_forward_cache/empty.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver deleted(current_frame_host());

  // Acquire WakeLock.
  EXPECT_EQ("DONE", EvalJs(rfh_a, R"(
  new Promise(async resolve => {
    try {
      await navigator.wakeLock.request('screen');
      resolve('DONE');
    } catch (error) {
      resolve('error: request failed');
    }
  });
  )"));

  // 2) Navigate away.
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // The page uses WakeLock so it should be deleted.
  deleted.WaitUntilDeleted();

  // 3) Go back to the page with WakeLock.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      FROM_HERE);
  ExpectBlocklistedFeature(
      blink::scheduler::WebSchedulerTrackedFeature::kWakeLock, FROM_HERE);
}

// TODO(yuzus): By releasing wakelock, the page should become cacheable again.
// Fix and re-enable the rest of this test.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DISABLED_CacheIfReleasedWakeLock) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with WakeLock usage.
  GURL url(embedded_test_server()->GetURL(
      "/back_forward_cache/page_with_wakelock.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver deleted(current_frame_host());

  // Acquire WakeLock.
  EXPECT_EQ("DONE", EvalJs(rfh_a, "requestWakeLock()"));
  // 2) Navigate away.
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // The page uses WakeLock so it should be deleted.
  deleted.WaitUntilDeleted();

  // 3) Go back to the page with WakeLock.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      FROM_HERE);
  ExpectBlocklistedFeature(
      blink::scheduler::WebSchedulerTrackedFeature::kWakeLock, FROM_HERE);

  // Release WakeLock.
  EXPECT_EQ("DONE", EvalJs(current_frame_host(), "releaseWakeLock()"));

  // 4) Navigate away.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 5) Go back to the page with WakeLock.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(current_frame_host(), rfh_a);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // This time the page is restored from cache because WakeLock is released.
  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kRestored,
                FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DoesNotCacheIfWebFileSystem) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with WebFileSystem usage.
  GURL url(embedded_test_server()->GetURL("/fileapi/request_test.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver deleted(rfh_a);

  // 2) Navigate away.
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));
  // The page uses WebFilesystem so it should be deleted.
  deleted.WaitUntilDeleted();

  // 3) Go back to the page with WebFileSystem.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      FROM_HERE);
  ExpectBlocklistedFeature(
      blink::scheduler::WebSchedulerTrackedFeature::kWebFileSystem, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, DoesNotCacheIfHttpError) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL error_url(embedded_test_server()->GetURL("a.com", "/page404.html"));
  GURL url(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to an error page.
  EXPECT_TRUE(NavigateToURL(shell(), error_url));
  EXPECT_EQ(net::HTTP_NOT_FOUND, current_frame_host()->last_http_status_code());
  RenderFrameDeletedObserver delete_rfh_a(current_frame_host());

  // Navigate away.
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // The page did not return 200 (OK), so it shouldn't have been cached.
  delete_rfh_a.WaitUntilDeleted();

  // Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kHTTPStatusNotOK},
      FROM_HERE);
}

class MockAppBannerService : public blink::mojom::AppBannerService {
 public:
  MockAppBannerService() = default;
  ~MockAppBannerService() override = default;

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Bind(mojo::PendingReceiver<blink::mojom::AppBannerService>(
        std::move(handle)));
  }

  mojo::Remote<blink::mojom::AppBannerController>& controller() {
    return controller_;
  }

  void OnBannerPromptRequested(bool) {}

  void SendBannerPromptRequest() {
    blink::mojom::AppBannerController* controller_ptr = controller_.get();
    base::OnceCallback<void(bool)> callback = base::BindOnce(
        &MockAppBannerService::OnBannerPromptRequested, base::Unretained(this));
    controller_ptr->BannerPromptRequest(
        receiver_.BindNewPipeAndPassRemote(),
        event_.BindNewPipeAndPassReceiver(), {"web"},
        base::BindOnce(&MockAppBannerService::OnBannerPromptReply,
                       base::Unretained(this), std::move(callback)));
  }

  void OnBannerPromptReply(base::OnceCallback<void(bool)> callback,
                           blink::mojom::AppBannerPromptReply reply) {
    std::move(callback).Run(reply ==
                            blink::mojom::AppBannerPromptReply::CANCEL);
  }

  // blink::mojom::AppBannerService:
  void DisplayAppBanner() override {}

 private:
  mojo::Receiver<blink::mojom::AppBannerService> receiver_{this};
  mojo::Remote<blink::mojom::AppBannerEvent> event_;
  mojo::Remote<blink::mojom::AppBannerController> controller_;

  DISALLOW_COPY_AND_ASSIGN(MockAppBannerService);
};

class BackForwardCacheBrowserTestWithAppBanner
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    web_contents()->GetMainFrame()->GetRemoteInterfaces()->GetInterface(
        mock_app_banner_service_.controller().BindNewPipeAndPassReceiver());
    BackForwardCacheBrowserTest::SetUpOnMainThread();
  }

  void SendBannerPromptRequest() {
    mock_app_banner_service_.SendBannerPromptRequest();
  }

 private:
  MockAppBannerService mock_app_banner_service_;
};

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithAppBanner,
                       DoesNotCacheIfAppBanner) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to A and request a PWA app banner.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  SendBannerPromptRequest();
  RenderFrameDeletedObserver delete_observer_rfh(current_frame_host());

  // 2) Navigate away. Page A requested a PWA app banner, and thus not cached.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));
  delete_observer_rfh.WaitUntilDeleted();

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      FROM_HERE);
  ExpectBlocklistedFeature(
      blink::scheduler::WebSchedulerTrackedFeature::kAppBanner, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, DoesNotCacheIfWebDatabase) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with WebDatabase usage.
  GURL url(embedded_test_server()->GetURL("/simple_database.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver deleted(rfh_a);

  // 2) Navigate away.
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));
  // The page uses WebDatabase so it should be deleted.
  deleted.WaitUntilDeleted();

  // 3) Go back to the page with WebDatabase.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      FROM_HERE);
  ExpectBlocklistedFeature(
      blink::scheduler::WebSchedulerTrackedFeature::kWebDatabase, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DoesNotCacheIfPageUnreachable) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL error_url(embedded_test_server()->GetURL("a.com", "/empty.html"));
  GURL url(embedded_test_server()->GetURL("b.com", "/title1.html"));

  std::unique_ptr<URLLoaderInterceptor> url_interceptor =
      URLLoaderInterceptor::SetupRequestFailForURL(error_url,
                                                   net::ERR_DNS_TIMED_OUT);

  // Start with a successful navigation to a document.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_EQ(net::HTTP_OK, current_frame_host()->last_http_status_code());

  // Navigate to an error page.
  NavigationHandleObserver observer(shell()->web_contents(), error_url);
  EXPECT_FALSE(NavigateToURL(shell(), error_url));
  EXPECT_TRUE(observer.is_error());
  EXPECT_EQ(net::ERR_DNS_TIMED_OUT, observer.net_error_code());
  EXPECT_EQ(
      GURL(kUnreachableWebDataURL),
      shell()->web_contents()->GetMainFrame()->GetSiteInstance()->GetSiteURL());
  EXPECT_EQ(net::OK, current_frame_host()->last_http_status_code());

  RenderFrameDeletedObserver delete_rfh_a(current_frame_host());

  // Navigate away.
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // The page had a networking error, so it shouldn't have been cached.
  delete_rfh_a.WaitUntilDeleted();

  // Go back.
  web_contents()->GetController().GoBack();
  EXPECT_FALSE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kHTTPStatusNotOK},
      FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DisableBackforwardCacheForTesting) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Disable the BackForwardCache.
  web_contents()->GetController().GetBackForwardCache().DisableForTesting(
      BackForwardCacheImpl::TEST_ASSUMES_NO_CACHING);

  // Navigate to a page that would normally be cacheable.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  RenderFrameDeletedObserver delete_observer_rfh_a(current_frame_host());

  // Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));

  // The page should be deleted (not cached).
  delete_observer_rfh_a.WaitUntilDeleted();
}

// Navigate from A to B, then cause JavaScript execution on A, then go back.
// Test the RenderFrameHost in the cache is evicted by JavaScript.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       EvictionOnJavaScriptExecution) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());

  // 3) Execute JavaScript on A.
  EvictByJavaScript(rfh_a);

  // RenderFrameHost A is evicted from the BackForwardCache:
  delete_observer_rfh_a.WaitUntilDeleted();

  // 4) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kNotRestored,
                FROM_HERE);
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kJavaScriptExecution},
      FROM_HERE);
}

// Similar to BackForwardCacheBrowserTest.EvictionOnJavaScriptExecution.
// Test case: A(B) -> C -> JS on B -> A(B)
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       EvictionOnJavaScriptExecutionIframe) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1) Navigate to A(B).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  // 2) Navigate to C.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  RenderFrameHostImpl* rfh_c = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_c(rfh_c);

  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_FALSE(delete_observer_rfh_c.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  EXPECT_FALSE(rfh_c->IsInBackForwardCache());

  // 3) Execute JavaScript on B.
  //
  EvictByJavaScript(rfh_b);

  // The A(B) page is evicted. So A and B are removed:
  delete_observer_rfh_a.WaitUntilDeleted();
  delete_observer_rfh_b.WaitUntilDeleted();

  // 4) Go back to A(B).
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kNotRestored,
                FROM_HERE);
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kJavaScriptExecution},
      FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       EvictionOnJavaScriptExecutionInAnotherWorld) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Execute JavaScript on A in a new world. This ensures a new world.
  const int32_t kNewWorldId = content::ISOLATED_WORLD_ID_CONTENT_END + 1;
  EXPECT_TRUE(ExecJs(rfh_a, "console.log('hi');",
                     EXECUTE_SCRIPT_DEFAULT_OPTIONS, kNewWorldId));

  // 3) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());

  // 4) Execute JavaScript on A in the new world.
  EXPECT_FALSE(ExecJs(rfh_a, "console.log('hi');",
                      EXECUTE_SCRIPT_DEFAULT_OPTIONS, kNewWorldId));

  // RenderFrameHost A is evicted from the BackForwardCache:
  delete_observer_rfh_a.WaitUntilDeleted();

  // 5) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kNotRestored,
                FROM_HERE);
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kJavaScriptExecution},
      FROM_HERE);
}

// Similar to BackForwardCacheBrowserTest.EvictionOnJavaScriptExecution, but
// cause the race condition of eviction and restoring.
//
// ┌───────┐                 ┌────────┐
// │Browser│                 │Renderer│
// └───┬───┘                 └───┬────┘
// (Store to the bfcache)        │
//     │         Freeze          │
//     │────────────────────────>│
//     │                         │
// (Restore from the bfcache)    │
//     │──┐                      │
//     │  │                      │
//     │EvictFromBackForwardCache│
//     │<────────────────────────│
//     │  │                      │
//     │  │      Resume          │
//     │  └─────────────────────>│
// ┌───┴───┐                 ┌───┴────┐
// │Browser│                 │Renderer│
// └───────┘                 └────────┘
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       EvictionOnJavaScriptExecutionRace) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  EXPECT_TRUE(ExecJs(rfh_a, "window.foo = 'initial document'"));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());

  // 3) Execute JavaScript on A when restoring A.
  // Execute JavaScript after committing but before swapping happens on the
  // renderer.
  ReadyToCommitNavigationCallback host_changed_callback(
      web_contents(), base::BindOnce(
                          [](RenderFrameHostImpl* rfh_a,
                             RenderFrameDeletedObserver* delete_observer_rfh_a,
                             NavigationHandle* navigation_handle) {
                            EXPECT_FALSE(delete_observer_rfh_a->deleted());
                            EXPECT_EQ(rfh_a,
                                      navigation_handle->GetRenderFrameHost());
                            ExecuteScriptAsync(rfh_a, "console.log('hi');");
                          },
                          rfh_a, &delete_observer_rfh_a));

  // Wait for two navigations to finish. The first one is the BackForwardCache
  // navigation, the other is the reload caused by the eviction.
  //
  //   1. BFcache navigation start.
  //   2. BFcache navigation commit & finish.
  //   3. Evict & reload start.
  //   4. Reload commit.
  //   5. Reload finish.
  //
  // Each item is a single task.
  TestNavigationObserver observer(web_contents(),
                                  2 /* number of navigations */);
  observer.set_wait_event(
      TestNavigationObserver::WaitEvent::kNavigationFinished);
  web_contents()->GetController().GoBack();
  observer.WaitForNavigationFinished();

  // A is not destroyed. A is reloaded instead.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  EXPECT_NE("initial document", EvalJs(rfh_a, "window.foo"));

  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kRestored,
                FROM_HERE);
  ExpectEvictedAfterCommitted(
      {
          BackForwardCacheMetrics::EvictedAfterDocumentRestoredReason::
              kRestored,
          BackForwardCacheMetrics::EvictedAfterDocumentRestoredReason::
              kByJavaScript,
      },
      FROM_HERE);
}

// Tests the events are fired when going back from the cache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, Events) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  StartRecordingEvents(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());
  // TODO(yuzus): Post message to the frozen page, and make sure that the
  // messages arrive after the page visibility events, not before them.

  // 3) Go back to A. Confirm that expected events are fired.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_EQ(rfh_a, current_frame_host());
  // visibilitychange events are added twice per each because it is fired for
  // both window and document.
  MatchEventList(
      rfh_a,
      ListValueOf("document.visibilitychange", "window.visibilitychange",
                  "window.pagehide.persisted", "document.freeze",
                  "document.resume", "window.pageshow.persisted",
                  "document.visibilitychange", "window.visibilitychange"));
}

// Tests the events are fired for subframes when going back from the cache.
// Test case: a(b) -> c -> a(b)
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, EventsForSubframes) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1) Navigate to A(B).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);
  StartRecordingEvents(rfh_a);
  StartRecordingEvents(rfh_b);

  // 2) Navigate to C.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  RenderFrameHostImpl* rfh_c = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_c(rfh_c);
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  EXPECT_FALSE(rfh_c->IsInBackForwardCache());
  // TODO(yuzus): Post message to the frozen page, and make sure that the
  // messages arrive after the page visibility events, not before them.

  // 3) Go back to A(B). Confirm that expected events are fired on the subframe.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_FALSE(delete_observer_rfh_c.deleted());
  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());
  EXPECT_TRUE(rfh_c->IsInBackForwardCache());
  // visibilitychange events are added twice per each because it is fired for
  // both window and document.
  MatchEventList(
      rfh_a,
      ListValueOf("document.visibilitychange", "window.visibilitychange",
                  "window.pagehide.persisted", "document.freeze",
                  "document.resume", "window.pageshow.persisted",
                  "document.visibilitychange", "window.visibilitychange"));
  MatchEventList(
      rfh_b,
      ListValueOf("document.visibilitychange", "window.visibilitychange",
                  "window.pagehide.persisted", "document.freeze",
                  "document.resume", "window.pageshow.persisted",
                  "document.visibilitychange", "window.visibilitychange"));
}

// Tests the events are fired when going back from the cache.
// Same as: BackForwardCacheBrowserTest.Events, but with a document-initiated
// navigation. This is a regression test for https://crbug.com/1000324
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       EventsAfterDocumentInitiatedNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  StartRecordingEvents(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(ExecJs(shell(), JsReplace("location = $1;", url_b.spec())));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());
  // TODO(yuzus): Post message to the frozen page, and make sure that the
  // messages arrive after the page visibility events, not before them.

  // 3) Go back to A. Confirm that expected events are fired.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_EQ(rfh_a, current_frame_host());
  // visibilitychange events are added twice per each because it is fired for
  // both window and document.
  MatchEventList(
      rfh_a,
      ListValueOf("document.visibilitychange", "window.visibilitychange",
                  "window.pagehide.persisted", "document.freeze",
                  "document.resume", "window.pageshow.persisted",
                  "document.visibilitychange", "window.visibilitychange"));
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       NavigationCancelledAtWillStartRequest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Cancel all navigation attempts.
  content::TestNavigationThrottleInserter throttle_inserter(
      shell()->web_contents(),
      base::BindLambdaForTesting(
          [&](NavigationHandle* handle) -> std::unique_ptr<NavigationThrottle> {
            auto throttle = std::make_unique<TestNavigationThrottle>(handle);
            throttle->SetResponse(TestNavigationThrottle::WILL_START_REQUEST,
                                  TestNavigationThrottle::SYNCHRONOUS,
                                  NavigationThrottle::CANCEL_AND_IGNORE);
            return throttle;
          }));

  // 3) Go back to A, which will be cancelled by the NavigationThrottle.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // We should still be showing page B.
  EXPECT_EQ(rfh_b, current_frame_host());
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       NavigationCancelledAtWillProcessResponse) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Cancel all navigation attempts.
  content::TestNavigationThrottleInserter throttle_inserter(
      shell()->web_contents(),
      base::BindLambdaForTesting(
          [&](NavigationHandle* handle) -> std::unique_ptr<NavigationThrottle> {
            auto throttle = std::make_unique<TestNavigationThrottle>(handle);
            throttle->SetResponse(TestNavigationThrottle::WILL_PROCESS_RESPONSE,
                                  TestNavigationThrottle::SYNCHRONOUS,
                                  NavigationThrottle::CANCEL_AND_IGNORE);
            return throttle;
          }));

  // 3) Go back to A, which will be cancelled by the NavigationThrottle.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // We should still be showing page B.
  EXPECT_EQ(rfh_b, current_frame_host());
}

// Test the race condition where a document is evicted from the BackForwardCache
// while it is in the middle of being restored.
//
// ┌───────┐                 ┌────────┐
// │Browser│                 │Renderer│
// └───┬───┘                 └───┬────┘
// (Freeze & store the cache)    │
//     │────────────────────────>│
//     │                         │
// (Navigate to cached document) │
//     │──┐                      │
//     │  │                      │
//     │EvictFromBackForwardCache│
//     │<────────────────────────│
//     │  │                      │
//     │  x Navigation cancelled │
//     │    and reissued         │
// ┌───┴───┐                 ┌───┴────┐
// │Browser│                 │Renderer│
// └───────┘                 └────────┘
//
// When the eviction occurs, the in flight NavigationRequest to the cached
// document should be reissued (cancelled and replaced by a normal navigation).
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       ReissuesNavigationIfEvictedDuringNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to page A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to page B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_NE(rfh_a, rfh_b);

  // 3) Start navigation to page A, and cause the document to be evicted during
  // the navigation.
  TestNavigationManager navigation_manager(shell()->web_contents(), url_a);
  web_contents()->GetController().GoBack();

  EXPECT_TRUE(navigation_manager.WaitForRequestStart());
  // Try to execute javascript, this will cause the document we are restoring to
  // get evicted from the cache.
  EvictByJavaScript(rfh_a);

  // The navigation should get reissued, and ultimately finish.
  navigation_manager.WaitForNavigationFinished();

  // rfh_a should have been deleted, and page A navigated to normally.
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  delete_observer_rfh_a.WaitUntilDeleted();
  RenderFrameHostImpl* rfh_a2 = current_frame_host();
  EXPECT_NE(rfh_a2, rfh_b);
  EXPECT_EQ(rfh_a2->GetLastCommittedURL(), url_a);

  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kNotRestored,
                FROM_HERE);
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kJavaScriptExecution},
      FROM_HERE);
}

// Similar to ReissuesNavigationIfEvictedDuringNavigation, except that
// BackForwardCache::Flush is the source of the eviction.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       FlushCacheDuringNavigationToCachedPage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to page A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a1 = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a1(rfh_a1);

  // 2) Navigate to page B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b2 = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b2(rfh_b2);
  EXPECT_FALSE(delete_observer_rfh_a1.deleted());
  EXPECT_TRUE(rfh_a1->IsInBackForwardCache());
  EXPECT_NE(rfh_a1, rfh_b2);

  // 3) Start navigation to page A, and flush the cache during the navigation.
  TestNavigationManager navigation_manager(shell()->web_contents(), url_a);
  web_contents()->GetController().GoBack();

  EXPECT_TRUE(navigation_manager.WaitForRequestStart());

  // Flush the cache, which contains the document being navigated to.
  web_contents()->GetController().GetBackForwardCache().Flush();

  // The navigation should get canceled, then reissued; ultimately resulting in
  // a successful navigation using a new RenderFrameHost.
  navigation_manager.WaitForNavigationFinished();

  // rfh_a should have been deleted, and page A navigated to normally.
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  delete_observer_rfh_a1.WaitUntilDeleted();
  EXPECT_TRUE(rfh_b2->IsInBackForwardCache());
  RenderFrameHostImpl* rfh_a3 = current_frame_host();
  EXPECT_EQ(rfh_a3->GetLastCommittedURL(), url_a);
}

// Test that if the renderer process crashes while a document is in the
// BackForwardCache, it gets evicted.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       EvictsFromCacheIfRendererProcessCrashes) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(delete_observer_rfh_a.deleted());

  // 3) Crash A's renderer process while it is in the cache.
  RenderProcessHost* process = rfh_a->GetProcess();
  RenderProcessHostWatcher crash_observer(
      process, RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
  rfh_a->GetProcess()->Shutdown(0);

  // The cached RenderFrameHost should be destroyed (not kept in the cache).
  crash_observer.Wait();
  delete_observer_rfh_a.WaitUntilDeleted();

  // rfh_b should still be the current frame.
  EXPECT_EQ(current_frame_host(), rfh_b);
  EXPECT_FALSE(delete_observer_rfh_b.deleted());

  // 4) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kNotRestored,
                FROM_HERE);
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kRendererProcessKilled},
      FROM_HERE);
}

// The test is simulating a race condition. The scheduler tracked features are
// updated during the "freeze" event in a way that would have prevented the
// document from entering the BackForwardCache in the first place.
//
// TODO(https://crbug.com/996267): The document should be evicted.
//
// ┌───────┐                     ┌────────┐
// │browser│                     │renderer│
// └───┬───┘                     └────┬───┘
//  (enter cache)                     │
//     │           Freeze()           │
//     │─────────────────────────────>│
//     │                          (onfreeze)
//     │OnSchedulerTrackedFeaturesUsed│
//     │<─────────────────────────────│
//     │                           (frozen)
//     │                              │
// ┌───┴───┐                     ┌────┴───┐
// │browser│                     │renderer│
// └───────┘                     └────────┘
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       SchedulerTrackedFeaturesUpdatedWhileStoring) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // When the page will enter the BackForwardCache, just before being frozen,
  // use a feature that would have been prevented the document from being
  // cached.
  EXPECT_TRUE(ExecJs(rfh_a, R"(
    document.addEventListener('freeze', event => {
      let canvas = document.createElement('canvas');
      document.body.appendChild(canvas);
      gl = canvas.getContext('webgl');
    });
  )"));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // rfh_a should be evicted from the cache and destroyed.
  delete_observer_rfh_a.WaitUntilDeleted();
}

// A fetch request starts during the "freeze" event. The current behavior is to
// send the request anyway. However evicting the page from the BackForwardCache
// might be a better behavior.
//
// ┌───────┐┌────────┐       ┌───────────────┐
// │browser││renderer│       │network service│
// └───┬───┘└───┬────┘       └───────┬───────┘
//     │Freeze()│                    │
//     │───────>│                    │
//     │     (onfreeze)              │
//     │        │CreateLoaderAndStart│
//     │        │───────────────────>│
//     │      (frozen)               │
// ┌───┴───┐┌───┴────┐       ┌───────┴───────┐
// │browser││renderer│       │network service│
// └───────┘└────────┘       └───────────────┘
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, FetchWhileStoring) {
  net::test_server::ControllableHttpResponse fetch_response(
      embedded_test_server(), "/fetch");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // Use "fetch" immediately before being frozen.
  EXPECT_TRUE(ExecJs(rfh_a, R"(
    document.addEventListener('freeze', event => {
      my_fetch = fetch('/fetch', { keepalive: true});
    });
  )"));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  fetch_response.WaitForRequest();
  fetch_response.Send(net::HTTP_OK, "text/html", "TheResponse");
  fetch_response.Done();

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  delete_observer_rfh_a.WaitUntilDeleted();
}

// Disabled on Android, since we have problems starting up the websocket test
// server in the host
#if defined(OS_ANDROID)
#define MAYBE_WebSocketNotCached DISABLED_WebSocketNotCached
#else
#define MAYBE_WebSocketNotCached WebSocketNotCached
#endif
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, MAYBE_WebSocketNotCached) {
  net::SpawnedTestServer ws_server(net::SpawnedTestServer::TYPE_WS,
                                   net::GetWebSocketTestDataDirectory());
  ASSERT_TRUE(ws_server.Start());

  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // Open a WebSocket.
  const char script[] = R"(
      new Promise(resolve => {
        const socket = new WebSocket($1);
        socket.addEventListener('open', () => resolve());
      });)";
  ASSERT_TRUE(ExecJs(
      rfh_a, JsReplace(script, ws_server.GetURL("echo-with-no-extension"))));

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // Confirm A is evicted.
  delete_observer_rfh_a.WaitUntilDeleted();
}

// Only HTTP/HTTPS main document can enter the BackForwardCache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, CacheHTTPDocumentOnly) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(CreateHttpsServer()->Start());

  GURL http_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL https_url(https_server()->GetURL("a.com", "/title1.html"));
  GURL file_url = net::FilePathToFileURL(GetTestFilePath("", "title1.html"));
  GURL data_url = GURL("data:text/html,");
  GURL blank_url = GURL(url::kAboutBlankURL);
  GURL webui_url = GetWebUIURL("gpu");

  enum { STORED, DELETED };
  struct {
    int expectation;
    GURL url;
  } test_cases[] = {
      // Only document with HTTP/HTTPS URLs are allowed to enter the
      // BackForwardCache.
      {STORED, http_url},
      {STORED, https_url},

      // Others aren't allowed.
      {DELETED, file_url},
      {DELETED, data_url},
      {DELETED, webui_url},
      {DELETED, blank_url},
  };

  char hostname[] = "a.unique";
  for (auto& test_case : test_cases) {
    SCOPED_TRACE(testing::Message()
                 << std::endl
                 << "expectation = " << test_case.expectation << std::endl
                 << "url = " << test_case.url << std::endl);

    // 1) Navigate to.
    EXPECT_TRUE(NavigateToURL(shell(), test_case.url));
    RenderFrameHostImpl* rfh = current_frame_host();
    RenderFrameDeletedObserver delete_observer(rfh);

    // 2) Navigate away.
    hostname[0]++;
    GURL reset_url(embedded_test_server()->GetURL(hostname, "/title1.html"));
    EXPECT_TRUE(NavigateToURL(shell(), reset_url));

    if (test_case.expectation == STORED) {
      EXPECT_FALSE(delete_observer.deleted());
      EXPECT_TRUE(rfh->IsInBackForwardCache());
      continue;
    }

    // On Android, navigations to about:blank keeps the same RenderFrameHost.
    // Obviously, it can't enter the BackForwardCache, because it is still used
    // to display the current document.
    if (test_case.url == blank_url &&
        !SiteIsolationPolicy::UseDedicatedProcessesForAllSites()) {
      EXPECT_FALSE(delete_observer.deleted());
      EXPECT_FALSE(rfh->IsInBackForwardCache());
      EXPECT_EQ(rfh, current_frame_host());
      continue;
    }

    delete_observer.WaitUntilDeleted();
  }
}

namespace {

void RegisterServiceWorker(RenderFrameHostImpl* rfh) {
  EXPECT_EQ("success", EvalJs(rfh, R"(
    let controller_changed_promise = new Promise(resolve_controller_change => {
      navigator.serviceWorker.oncontrollerchange = resolve_controller_change;
    });

    new Promise(async resolve => {
      try {
        await navigator.serviceWorker.register(
          "./service-worker.js", {scope: "./"})
      } catch (e) {
        resolve("error: registration has failed");
      }

      await controller_changed_promise;

      if (navigator.serviceWorker.controller) {
        resolve("success");
      } else {
        resolve("error: not controlled by service worker");
      }
    });
  )"));
}

// Returns a unique script for each request, to test service worker update.
std::unique_ptr<net::test_server::HttpResponse> RequestHandlerForUpdateWorker(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != "/back_forward_cache/service-worker.js")
    return std::unique_ptr<net::test_server::HttpResponse>();
  static int counter = 0;
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  const char script[] = R"(
    // counter = $1
    self.addEventListener('activate', function(event) {
      event.waitUntil(self.clients.claim());
    });
  )";
  http_response->set_content(JsReplace(script, counter++));
  http_response->set_content_type("text/javascript");
  http_response->AddCustomHeader("Cache-Control",
                                 "no-cache, no-store, must-revalidate");
  return http_response;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DoesNotCachePagesWithServiceWorkers) {
  ASSERT_TRUE(CreateHttpsServer()->Start());

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(
      shell(),
      https_server()->GetURL("a.com", "/back_forward_cache/empty.html")));

  // Register a service worker.
  RegisterServiceWorker(current_frame_host());

  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver deleted(rfh_a);

  // 2) Navigate away.
  shell()->LoadURL(https_server()->GetURL("b.com", "/title1.html"));

  // The page is controlled by a service worker, so it shouldn't have been
  // cached.
  deleted.WaitUntilDeleted();

  ExpectOutcomeDidNotChange(FROM_HERE);

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      FROM_HERE);
  ExpectBlocklistedFeature(blink::scheduler::WebSchedulerTrackedFeature::
                               kServiceWorkerControlledPage,
                           FROM_HERE);
}

class BackForwardCacheBrowserTestWithVibration
    : public BackForwardCacheBrowserTest,
      public device::mojom::VibrationManager {
 public:
  BackForwardCacheBrowserTestWithVibration() {
    OverrideVibrationManagerBinderForTesting(base::BindRepeating(
        &BackForwardCacheBrowserTestWithVibration::BindVibrationManager,
        base::Unretained(this)));
  }

  ~BackForwardCacheBrowserTestWithVibration() override {
    OverrideVibrationManagerBinderForTesting(base::NullCallback());
  }

  void BindVibrationManager(
      mojo::PendingReceiver<device::mojom::VibrationManager> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  bool TriggerVibrate(RenderFrameHostImpl* rfh,
                      int duration,
                      base::OnceClosure vibrate_done) {
    vibrate_done_ = std::move(vibrate_done);
    bool result;
    std::string script = "domAutomationController.send(navigator.vibrate(" +
                         base::NumberToString(duration) + "))";
    EXPECT_TRUE(ExecuteScriptAndExtractBool(rfh, script, &result));
    return result;
  }

  bool TriggerShortVibrationSequence(RenderFrameHostImpl* rfh,
                                     base::OnceClosure vibrate_done) {
    vibrate_done_ = std::move(vibrate_done);
    bool result;
    std::string script =
        "domAutomationController.send(navigator.vibrate([10] * 1000))";
    EXPECT_TRUE(ExecuteScriptAndExtractBool(rfh, script, &result));
    return result;
  }

  bool IsCancelled() { return cancelled_; }

 private:
  // device::mojom::VibrationManager:
  void Vibrate(int64_t milliseconds, VibrateCallback callback) override {
    cancelled_ = false;
    std::move(callback).Run();
    std::move(vibrate_done_).Run();
  }

  void Cancel(CancelCallback callback) override {
    cancelled_ = true;
    std::move(callback).Run();
  }

  bool cancelled_ = false;
  base::OnceClosure vibrate_done_;
  mojo::Receiver<device::mojom::VibrationManager> receiver_{this};
};

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithVibration,
                       VibrationStopsAfterEnteringCache) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with a long vibration.
  GURL url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  base::RunLoop run_loop;
  RenderFrameHostImpl* rfh_a = current_frame_host();
  ASSERT_TRUE(TriggerVibrate(rfh_a, 10000, run_loop.QuitClosure()));
  EXPECT_FALSE(IsCancelled());

  // 2) Navigate away and expect the vibration to be canceled.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));
  EXPECT_NE(current_frame_host(), rfh_a);
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(IsCancelled());

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kRestored,
                FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithVibration,
                       ShortVibrationSequenceStopsAfterEnteringCache) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with a long vibration.
  GURL url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  base::RunLoop run_loop;
  RenderFrameHostImpl* rfh_a = current_frame_host();
  ASSERT_TRUE(TriggerShortVibrationSequence(rfh_a, run_loop.QuitClosure()));
  EXPECT_FALSE(IsCancelled());

  // 2) Navigate away and expect the vibration to be canceled.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));
  EXPECT_NE(current_frame_host(), rfh_a);
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(IsCancelled());

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kRestored,
                FROM_HERE);
}

class BackForwardCacheBrowserTestWithServiceWorkerEnabled
    : public BackForwardCacheBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  BackForwardCacheBrowserTestWithServiceWorkerEnabled() {}
  ~BackForwardCacheBrowserTestWithServiceWorkerEnabled() override {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableFeatureAndSetParams(features::kBackForwardCache,
                              "service_worker_supported", "true");
    if (GetParam())
      EnableFeatureAndSetParams(features::kServiceWorkerOnUI, "", "");
    else
      DisableFeature(features::kServiceWorkerOnUI);

    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_P(BackForwardCacheBrowserTestWithServiceWorkerEnabled,
                       CachedPagesWithServiceWorkers) {
  CreateHttpsServer();
  SetupCrossSiteRedirector(https_server());
  ASSERT_TRUE(https_server()->Start());

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(
      shell(),
      https_server()->GetURL("a.com", "/back_forward_cache/empty.html")));

  // Register a service worker.
  RegisterServiceWorker(current_frame_host());

  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver deleted(rfh_a);

  // 2) Navigate away.
  EXPECT_TRUE(
      NavigateToURL(shell(), https_server()->GetURL("b.com", "/title1.html")));

  EXPECT_FALSE(deleted.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Go back to A. The navigation should be served from the cache.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_FALSE(deleted.deleted());
  EXPECT_EQ(rfh_a, current_frame_host());
}

IN_PROC_BROWSER_TEST_P(BackForwardCacheBrowserTestWithServiceWorkerEnabled,
                       EvictIfCacheBlocksServiceWorkerVersionActivation) {
  CreateHttpsServer();
  https_server()->RegisterRequestHandler(
      base::BindRepeating(&RequestHandlerForUpdateWorker));
  SetupCrossSiteRedirector(https_server());
  ASSERT_TRUE(https_server()->Start());
  Shell* tab_x = shell();
  Shell* tab_y = CreateBrowser();
  // 1) Navigate to A in tab X.
  EXPECT_TRUE(NavigateToURL(
      tab_x,
      https_server()->GetURL("a.com", "/back_forward_cache/empty.html")));
  // 2) Register a service worker.
  RegisterServiceWorker(current_frame_host());

  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver deleted(rfh_a);
  // 3) Navigate away to B in tab X.
  EXPECT_TRUE(
      NavigateToURL(tab_x, https_server()->GetURL("b.com", "/title1.html")));
  EXPECT_FALSE(deleted.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  // 4) Navigate to A in tab Y.
  EXPECT_TRUE(NavigateToURL(
      tab_y,
      https_server()->GetURL("a.com", "/back_forward_cache/empty.html")));
  // 5) Close tab Y to activate a service worker version.
  // This should evict |rfh_a| from the cache.
  tab_y->Close();
  deleted.WaitUntilDeleted();
  // 6) Navigate to A in tab X.
  tab_x->web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(tab_x->web_contents()));
  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kNotRestored,
                FROM_HERE);
  ExpectNotRestored(
      {
          BackForwardCacheMetrics::NotRestoredReason::
              kServiceWorkerVersionActivation,
      },
      FROM_HERE);
}

IN_PROC_BROWSER_TEST_P(BackForwardCacheBrowserTestWithServiceWorkerEnabled,
                       EvictWithPostMessageToCachedClient) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.RegisterRequestHandler(
      base::BindRepeating(&RequestHandlerForUpdateWorker));
  https_server.AddDefaultHandlers(GetTestDataFilePath());
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());
  Shell* tab_to_execute_service_worker = shell();
  Shell* tab_to_be_bfcached = CreateBrowser();

  // Observe the new WebContents to trace the navigtion ID.
  WebContentsObserver::Observe(tab_to_be_bfcached->web_contents());

  // 1) Navigate to A in |tab_to_execute_service_worker|.
  EXPECT_TRUE(NavigateToURL(
      tab_to_execute_service_worker,
      https_server.GetURL(
          "a.com", "/back_forward_cache/service_worker_post_message.html")));

  // 2) Register a service worker.
  EXPECT_EQ("DONE", EvalJs(tab_to_execute_service_worker,
                           "register('service_worker_post_message.js')"));

  // 3) Navigate to A in |tab_to_be_bfcached|.
  EXPECT_TRUE(NavigateToURL(
      tab_to_be_bfcached,
      https_server.GetURL(
          "a.com", "/back_forward_cache/service_worker_post_message.html")));
  const std::string script_to_store =
      "executeCommandOnServiceWorker('StoreClients')";
  EXPECT_EQ("DONE", EvalJs(tab_to_execute_service_worker, script_to_store));
  RenderFrameHostImpl* rfh =
      static_cast<WebContentsImpl*>(tab_to_be_bfcached->web_contents())
          ->GetFrameTree()
          ->root()
          ->current_frame_host();
  RenderFrameDeletedObserver deleted_observer_rfh(rfh);

  // 4) Navigate away to B in |tab_to_be_bfcached|.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached,
                            https_server.GetURL("b.com", "/title1.html")));
  EXPECT_FALSE(deleted_observer_rfh.deleted());
  EXPECT_TRUE(rfh->IsInBackForwardCache());

  // 5) Trigger client.postMessage via |tab_to_execute_service_worker|. Cache in
  // |tab_to_be_bfcached| will be evicted.
  const std::string script_to_post_message =
      "executeCommandOnServiceWorker('PostMessageToStoredClients')";
  EXPECT_EQ("DONE",
            EvalJs(tab_to_execute_service_worker, script_to_post_message));
  deleted_observer_rfh.WaitUntilDeleted();

  // 6) Go back to A in |tab_to_be_bfcached|.
  tab_to_be_bfcached->web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(tab_to_be_bfcached->web_contents()));
  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kNotRestored,
                FROM_HERE);
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kServiceWorkerPostMessage},
      FROM_HERE);
}

IN_PROC_BROWSER_TEST_P(BackForwardCacheBrowserTestWithServiceWorkerEnabled,
                       EvictOnServiceWorkerClaim) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.RegisterRequestHandler(
      base::BindRepeating(&RequestHandlerForUpdateWorker));
  https_server.AddDefaultHandlers(GetTestDataFilePath());
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());

  Shell* tab_to_be_bfcached = shell();
  Shell* tab_to_execute_service_worker = CreateBrowser();

  // 1) Navigate to A in |tab_to_be_bfcached|.
  EXPECT_TRUE(NavigateToURL(
      tab_to_be_bfcached,
      https_server.GetURL(
          "a.com", "/back_forward_cache/service_worker_registration.html")));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver deleted(rfh_a);

  // 2) Navigate away to B in |tab_to_be_bfcached|.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached,
                            https_server.GetURL("b.com", "/title1.html")));
  EXPECT_FALSE(deleted.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Navigate to A in |tab_to_execute_service_worker|.
  EXPECT_TRUE(NavigateToURL(
      tab_to_execute_service_worker,
      https_server.GetURL(
          "a.com", "/back_forward_cache/service_worker_registration.html")));

  // 4) Register a service worker for |tab_to_execute_service_worker|.
  EXPECT_EQ("DONE", EvalJs(tab_to_execute_service_worker,
                           "register('service_worker_registration.js')"));

  // 5) The service worker calls clients.claim(). |rfh_a| would normally be
  //    claimed but because it's in bfcache, it is evicted from the cache.
  EXPECT_EQ("DONE", EvalJs(tab_to_execute_service_worker, "claim()"));

  // 6) Navigate to A in |tab_to_be_bfcached|.
  tab_to_be_bfcached->web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(tab_to_be_bfcached->web_contents()));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_TRUE(deleted.deleted());
  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kNotRestored,
                FROM_HERE);
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kServiceWorkerClaim},
      FROM_HERE);
}

INSTANTIATE_TEST_SUITE_P(All,
                         BackForwardCacheBrowserTestWithServiceWorkerEnabled,
                         testing::Bool());

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, CachePagesWithBeacon) {
  constexpr char kKeepalivePath[] = "/keepalive";

  net::test_server::ControllableHttpResponse keepalive(embedded_test_server(),
                                                       kKeepalivePath);
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_ping(embedded_test_server()->GetURL("a.com", kKeepalivePath));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  EXPECT_TRUE(
      ExecJs(shell(), JsReplace(R"(navigator.sendBeacon($1, "");)", url_ping)));

  // 2) Navigate to B.
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // Ensure that the keepalive request is sent.
  keepalive.WaitForRequest();
  // Don't actually send the response.

  // Page A should be in the cache.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
}

// Regression test for https://crbug.com/993337.
//
// A note about sharing BrowsingInstances and the BackForwardCache:
//
// We should never keep around more than one main frame that belongs to the same
// BrowsingInstance. When swapping two pages, when one is stored in the
// back-forward cache or one is restored from it, the current code expects the
// two to live in different BrowsingInstances.
//
// History navigation can recreate a page with the same BrowsingInstance as the
// one stored in the back-forward cache. This case must to be handled. When it
// happens, the back-forward cache page is evicted.
//
// Since cache eviction is asynchronous, it's is possible for two main frames
// belonging to the same BrowsingInstance to be alive for a brief period of time
// (the new page being navigated to, and a page in the cache, until it is
// destroyed asynchronously via eviction).
//
// The test below tests that the brief period of time where two main frames are
// alive in the same BrowsingInstance does not cause anything to blow up.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       NavigateToTwoPagesOnSameSite) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url_b3(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A1.
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));

  // 2) Navigate to A2.
  EXPECT_TRUE(NavigateToURL(shell(), url_a2));
  RenderFrameHostImpl* rfh_a2 = current_frame_host();
  RenderFrameDeletedObserver delete_rfh_a2(current_frame_host());

  // 3) Navigate to B3.
  EXPECT_TRUE(NavigateToURL(shell(), url_b3));
  EXPECT_TRUE(rfh_a2->IsInBackForwardCache());
  RenderFrameHostImpl* rfh_b3 = current_frame_host();

  // 4) Do a history navigation back to A1.
  web_contents()->GetController().GoToIndex(0);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_TRUE(rfh_b3->IsInBackForwardCache());

  // Note that the frame for A1 gets created before A2 is deleted from the
  // cache, so there will be a brief period where two the main frames (A1 and
  // A2) are alive in the same BrowsingInstance/SiteInstance, at the same time.
  // That is the scenario this test is covering. This used to cause a CHECK,
  // because the two main frames shared a single RenderViewHost (no longer the
  // case after https://crrev.com/c/1833616).

  // A2 should be evicted from the cache and asynchronously deleted, due to the
  // cache size limit (B3 took its place in the cache).
  delete_rfh_a2.WaitUntilDeleted();
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       NavigateToTwoPagesOnSameSiteWithSubframes) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // This test covers the same scenario as NavigateToTwoPagesOnSameSite, except
  // the pages contain subframes:
  // A1(B) -> A2(B(C)) -> D3 -> A1(B)
  //
  // The subframes shouldn't make a difference, so the expected behavior is the
  // same as NavigateToTwoPagesOnSameSite.
  GURL url_a1(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_a2(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  GURL url_d3(embedded_test_server()->GetURL("d.com", "/title1.html"));

  // 1) Navigate to A1(B).
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));

  // 2) Navigate to A2(B(C)).
  EXPECT_TRUE(NavigateToURL(shell(), url_a2));
  RenderFrameHostImpl* rfh_a2 = current_frame_host();
  RenderFrameDeletedObserver delete_rfh_a2(current_frame_host());

  // 3) Navigate to D3.
  EXPECT_TRUE(NavigateToURL(shell(), url_d3));
  EXPECT_TRUE(rfh_a2->IsInBackForwardCache());
  RenderFrameHostImpl* rfh_d3 = current_frame_host();

  // 4) Do a history navigation back to A1(B).
  web_contents()->GetController().GoToIndex(0);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // D3 takes A2(B(C))'s place in the cache.
  EXPECT_TRUE(rfh_d3->IsInBackForwardCache());
  delete_rfh_a2.WaitUntilDeleted();
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       ConflictingBrowsingInstances) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url_b3(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A1.
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));

  // 2) Navigate to A2.
  EXPECT_TRUE(NavigateToURL(shell(), url_a2));
  RenderFrameHostImpl* rfh_a2 = current_frame_host();
  RenderFrameDeletedObserver delete_rfh_a2(current_frame_host());

  // 3) Navigate to B3.
  EXPECT_TRUE(NavigateToURL(shell(), url_b3));
  EXPECT_TRUE(rfh_a2->IsInBackForwardCache());
  RenderFrameHostImpl* rfh_b3 = current_frame_host();
  // Make B3 ineligible for caching, so that navigating doesn't evict A2
  // due to the cache size limit.
  content::BackForwardCache::DisableForRenderFrameHost(
      rfh_b3, "BackForwardCacheBrowserTest");

  // 4) Do a history navigation back to A1.  At this point, A1 is going to have
  // the same BrowsingInstance as A2. This should cause A2 to get
  // evicted from the BackForwardCache due to its conflicting BrowsingInstance.
  web_contents()->GetController().GoToIndex(0);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(current_frame_host()->GetLastCommittedURL(), url_a1);
  delete_rfh_a2.WaitUntilDeleted();

  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kNotRestored,
                FROM_HERE);
  ExpectNotRestored(
      {
          BackForwardCacheMetrics::NotRestoredReason::
              kRenderFrameHostReused_SameSite,
      },
      FROM_HERE);

  // 5) Go to A2.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kNotRestored,
                FROM_HERE);
  ExpectNotRestored(
      {
          BackForwardCacheMetrics::NotRestoredReason::
              kConflictingBrowsingInstance,
      },
      FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       CanCacheMultiplesPagesOnSameDomain) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b2(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL url_a3(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url_b4(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // Increase the cache size so we're able to store multiple pages for the same
  // site in the cache at once.
  web_contents()
      ->GetController()
      .GetBackForwardCache()
      .set_cache_size_limit_for_testing(3);

  // 1) Navigate to A1.
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));
  RenderFrameHostImpl* rfh_a1 = current_frame_host();

  // 2) Navigate to B2.
  EXPECT_TRUE(NavigateToURL(shell(), url_b2));
  RenderFrameHostImpl* rfh_b2 = current_frame_host();
  EXPECT_TRUE(rfh_a1->IsInBackForwardCache());

  // 3) Navigate to A3.
  EXPECT_TRUE(NavigateToURL(shell(), url_a3));
  RenderFrameHostImpl* rfh_a3 = current_frame_host();
  EXPECT_TRUE(rfh_a1->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b2->IsInBackForwardCache());
  // A1 and A3 shouldn't be treated as the same site instance.
  EXPECT_NE(rfh_a1->GetSiteInstance(), rfh_a3->GetSiteInstance());

  // 4) Navigate to B4.
  // Make sure we can store A1 and A3 in the cache at the same time.
  EXPECT_TRUE(NavigateToURL(shell(), url_b4));
  RenderFrameHostImpl* rfh_b4 = current_frame_host();
  EXPECT_TRUE(rfh_a1->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b2->IsInBackForwardCache());
  EXPECT_TRUE(rfh_a3->IsInBackForwardCache());

  // 5) Go back to A3.
  // Make sure we can restore A3, while A1 remains in the cache.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_TRUE(rfh_a1->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b2->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b4->IsInBackForwardCache());
  EXPECT_EQ(rfh_a3, current_frame_host());
  // B2 and B4 shouldn't be treated as the same site instance.
  EXPECT_NE(rfh_b2->GetSiteInstance(), rfh_b4->GetSiteInstance());

  // 6) Do a history navigation back to A1.
  // Make sure we can restore A1, while coming from A3.
  web_contents()->GetController().GoToIndex(0);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_TRUE(rfh_b2->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b4->IsInBackForwardCache());
  EXPECT_TRUE(rfh_a3->IsInBackForwardCache());
  EXPECT_EQ(rfh_a1, current_frame_host());
}

class GeolocationBackForwardCacheBrowserTest
    : public BackForwardCacheBrowserTest {
 protected:
  GeolocationBackForwardCacheBrowserTest() : geo_override_(0.0, 0.0) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableFeatureAndSetParams(features::kBackForwardCache,
                              "geolocation_supported", "true");

    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }

  device::ScopedGeolocationOverrider geo_override_;
};

// Test that a page which has queried geolocation in the past, but have no
// active geolocation query, can be bfcached.
IN_PROC_BROWSER_TEST_F(GeolocationBackForwardCacheBrowserTest,
                       CacheAfterGeolocationRequest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // Query current position, and wait for the query to complete.
  EXPECT_EQ("received", EvalJs(rfh_a, R"(
      new Promise(resolve => {
        navigator.geolocation.getCurrentPosition(() => resolve('received'));
      });
  )"));

  RenderFrameDeletedObserver deleted(rfh_a);

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // The page has no inflight geolocation request when we navigated away,
  // so it should have been cached.
  EXPECT_FALSE(deleted.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
}

// Test that a page which has an inflight geolocation query can be bfcached,
// and verify that the page does not observe any geolocation while the page
// was inside bfcache.
// The test is flaky on multiple platforms: crbug.com/1033270
IN_PROC_BROWSER_TEST_F(GeolocationBackForwardCacheBrowserTest,
                       DISABLED_CancelGeolocationRequestInFlight) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // Continuously query current geolocation.
  EXPECT_TRUE(ExecJs(rfh_a, R"(
    window.longitude_log = [];
    window.err_log = [];
    window.wait_for_first_position = new Promise(resolve => {
      navigator.geolocation.watchPosition(
        pos => {
          window.longitude_log.push(pos.coords.longitude);
          resolve("resolved");
        },
        err => window.err_log.push(err)
      );
    })
  )"));
  geo_override_.UpdateLocation(0.0, 0.0);
  EXPECT_EQ("resolved", EvalJs(rfh_a, "window.wait_for_first_position"));

  // Pause resolving Geoposition queries to keep the request inflight.
  geo_override_.Pause();
  geo_override_.UpdateLocation(1.0, 1.0);
  EXPECT_EQ(1u, geo_override_.GetGeolocationInstanceCount());

  // 2) Navigate away.
  base::RunLoop loop_until_close;
  geo_override_.SetGeolocationCloseCallback(loop_until_close.QuitClosure());

  RenderFrameDeletedObserver deleted(rfh_a);
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  loop_until_close.Run();

  // The page has no inflight geolocation request when we navigated away,
  // so it should have been cached.
  EXPECT_FALSE(deleted.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Resume resolving Geoposition queries.
  geo_override_.Resume();

  // We update the location while the page is BFCached, but this location should
  // not be observed.
  geo_override_.UpdateLocation(2.0, 2.0);

  // 3) Navigate back to A.

  // The location when navigated back can be observed
  geo_override_.UpdateLocation(3.0, 3.0);

  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());

  // Wait for an update after the user navigates back to A.
  EXPECT_EQ("resolved", EvalJs(rfh_a, R"(
    window.wait_for_position_after_resume = new Promise(resolve => {
      navigator.geolocation.watchPosition(
        pos => {
          window.longitude_log.push(pos.coords.longitude);
          resolve("resolved");
        },
        err => window.err_log.push(err)
      );
    })
  )"));

  EXPECT_LE(0, EvalJs(rfh_a, "longitude_log.indexOf(0.0)").ExtractInt())
      << "Geoposition before the page is put into BFCache should be visible";
  EXPECT_EQ(-1, EvalJs(rfh_a, "longitude_log.indexOf(1.0)").ExtractInt())
      << "Geoposition while the page is put into BFCache should be invisible";
  EXPECT_EQ(-1, EvalJs(rfh_a, "longitude_log.indexOf(2.0)").ExtractInt())
      << "Geoposition while the page is put into BFCache should be invisible";
  EXPECT_LT(0, EvalJs(rfh_a, "longitude_log.indexOf(3.0)").ExtractInt())
      << "Geoposition when the page is restored from BFCache should be visible";
  EXPECT_EQ(0, EvalJs(rfh_a, "err_log.length"))
      << "watchPosition API should have reported no errors";
}

// Test that documents are evicted correctly from BackForwardCache after time to
// live.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, TimedEviction) {
  // Inject mock time task runner to be used in the eviction timer, so we can,
  // check for the functionality we are interested before and after the time to
  // live. We don't replace ThreadTaskRunnerHandle::Get to ensure that it
  // doesn't affect other unrelated callsites.
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();

  web_contents()->GetController().GetBackForwardCache().SetTaskRunnerForTesting(
      task_runner);

  base::TimeDelta time_to_live_in_back_forward_cache =
      BackForwardCacheImpl::GetTimeToLiveInBackForwardCache();
  // This should match the value we set in EnableFeatureAndSetParams.
  EXPECT_EQ(time_to_live_in_back_forward_cache,
            base::TimeDelta::FromSeconds(3600));

  base::TimeDelta delta = base::TimeDelta::FromMilliseconds(1);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();

  // 3) Fast forward to just before eviction is due.
  task_runner->FastForwardBy(time_to_live_in_back_forward_cache - delta);

  // 4) Confirm A is still in BackForwardCache.
  ASSERT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 5) Fast forward to when eviction is due.
  task_runner->FastForwardBy(delta);

  // 6) Confirm A is evicted.
  delete_observer_rfh_a.WaitUntilDeleted();
  EXPECT_EQ(current_frame_host(), rfh_b);

  // 7) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::kTimeout},
                    FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    DisableBackForwardCachePreventsDocumentsFromBeingCached) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  url::Origin origin_a = url::Origin::Create(url_a);
  url::Origin origin_b = url::Origin::Create(url_b);

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  BackForwardCache::DisableForRenderFrameHost(rfh_a, kDisabledReasonForTest);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectDisabledWithReason(kDisabledReasonForTest, FROM_HERE);
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kDisableForRenderFrameHostCalled},
                    FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DisableBackForwardIsNoOpIfRfhIsGone) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  url::Origin origin_a = url::Origin::Create(url_a);
  url::Origin origin_b = url::Origin::Create(url_b);

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  GlobalFrameRoutingId rfh_a_id = rfh_a->GetGlobalFrameRoutingId();
  BackForwardCache::DisableForRenderFrameHost(rfh_a_id, kDisabledReasonForTest);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  delete_observer_rfh_a.WaitUntilDeleted();

  // This should not die
  BackForwardCache::DisableForRenderFrameHost(rfh_a_id, kDisabledReasonForTest);

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectDisabledWithReason(kDisabledReasonForTest, FROM_HERE);
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kDisableForRenderFrameHostCalled},
                    FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DisableBackForwardCacheIframe) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  BackForwardCache::DisableForRenderFrameHost(rfh_b, kDisabledReasonForTest);

  // 2) Navigate to C. A and B are deleted.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  delete_observer_rfh_a.WaitUntilDeleted();
  delete_observer_rfh_b.WaitUntilDeleted();

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kDisableForRenderFrameHostCalled},
                    FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DisableBackForwardEvictsIfAlreadyInCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  url::Origin origin_a = url::Origin::Create(url_a);
  url::Origin origin_b = url::Origin::Create(url_b);

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_a->is_evicted_from_back_forward_cache());

  BackForwardCache::DisableForRenderFrameHost(rfh_a, kDisabledReasonForTest);

  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectDisabledWithReason(kDisabledReasonForTest, FROM_HERE);
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kDisableForRenderFrameHostCalled},
                    FROM_HERE);
}

// Check that during a same-RenderFrameHost cross-document navigation, the
// disabled reasons is still tracked.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DisableForRenderFrameHostPersistsAcrossNavigations) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url_b3(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A1.
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));
  RenderFrameHostImpl* rfh_a1 = current_frame_host();
  RenderFrameDeletedObserver deleted_observer_rfh_a1(rfh_a1);
  // Disable back-forward cache for A.
  BackForwardCache::DisableForRenderFrameHost(rfh_a1, kDisabledReasonForTest);

  // 2) Navigate to A2.
  EXPECT_TRUE(NavigateToURL(shell(), url_a2));
  EXPECT_FALSE(deleted_observer_rfh_a1.deleted());
  EXPECT_EQ(rfh_a1, current_frame_host());

  // 3) Navigate to B3.
  EXPECT_TRUE(NavigateToURL(shell(), url_b3));
  deleted_observer_rfh_a1.WaitUntilDeleted();

  // 4) Go back to A2.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectDisabledWithReason(kDisabledReasonForTest, FROM_HERE);
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kDisableForRenderFrameHostCalled},
                    FROM_HERE);
}

// Confirm that same-document navigation and not history-navigation does not
// record metrics.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, MetricsNotRecorded) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL url_b2(embedded_test_server()->GetURL("b.com", "/title1.html#2"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // 3) Navigate to B#2 (same document navigation).
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), url_b2));

  // 4) Go back to B.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectOutcomeDidNotChange(FROM_HERE);

  // 5) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectOutcomeDidNotChange(FROM_HERE);
}

// Test for functionality of domain specific controls in back-forward cache.
class BackForwardCacheBrowserTestWithDomainControlEnabled
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets the allowed websites for testing, additionally adding the params
    // used by BackForwardCacheBrowserTest.
    std::string allowed_websites =
        "https://a.allowed/back_forward_cache/, "
        "https://b.allowed/back_forward_cache/allowed_path.html";
    EnableFeatureAndSetParams(features::kBackForwardCache, "allowed_websites",
                              allowed_websites);

    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

// Check the RenderFrameHost allowed to enter the BackForwardCache are the ones
// matching with the "allowed_websites" feature params.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithDomainControlEnabled,
                       CachePagesWithMatchedURLs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.allowed", "/back_forward_cache/allowed_path.html"));
  GURL url_b(embedded_test_server()->GetURL(
      "b.allowed", "/back_forward_cache/allowed_path.html?query=bar"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  // 3) Check if rfh_a is stored in back-forward cache, since it matches to
  // the list of allowed urls, it should be stored.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 4) Now go back to the last stored page, which in our case should be A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());

  // 5) Check if rfh_b is stored in back-forward cache, since it matches to
  // the list of allowed urls, it should be stored.
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
}

// We don't want to allow websites which doesn't match "allowed_websites" of
// feature params to be stored in back-forward cache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithDomainControlEnabled,
                       DoNotCachePagesWithUnMatchedURLs) {
  DisableCheckingMetricsForAllSites();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.disallowed", "/back_forward_cache/disallowed_path.html"));
  GURL url_b(embedded_test_server()->GetURL(
      "b.allowed", "/back_forward_cache/disallowed_path.html"));
  GURL url_c(embedded_test_server()->GetURL(
      "c.disallowed", "/back_forward_cache/disallowed_path.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  // 3) Since url of A doesn't match to the the list of allowed urls it should
  // not be stored in back-forward cache.
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  delete_observer_rfh_a.WaitUntilDeleted();

  // 4) Navigate to C.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));

  // 5) Since url of B doesn't match to the the list of allowed urls it should
  // not be stored in back-forward cache.
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  delete_observer_rfh_b.WaitUntilDeleted();

  // 6) Go back to B.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Nothing is recorded when the domain does not match.
  ExpectOutcomeDidNotChange(FROM_HERE);
  ExpectNotRestoredDidNotChange(FROM_HERE);
}

// Check the BackForwardCache is disabled when the WebUSB feature is used.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, WebUSB) {
  // WebUSB requires HTTPS.
  ASSERT_TRUE(CreateHttpsServer()->Start());

  // Main document.
  {
    content::BackForwardCacheDisabledTester tester;
    GURL url(https_server()->GetURL("a.com", "/title1.html"));

    EXPECT_TRUE(NavigateToURL(shell(), url));

    EXPECT_FALSE(current_frame_host()->IsBackForwardCacheDisabled());
    EXPECT_EQ("Found 0 devices", content::EvalJs(current_frame_host(), R"(
        new Promise(async resolve => {
          let devices = await navigator.usb.getDevices();
          resolve("Found " + devices.length + " devices");
        });
    )"));
    EXPECT_TRUE(current_frame_host()->IsBackForwardCacheDisabled());
    EXPECT_TRUE(tester.IsDisabledForFrameWithReason(
        current_frame_host()->GetProcess()->GetID(),
        current_frame_host()->GetRoutingID(), "WebUSB"));
  }

  // Nested document.
  {
    content::BackForwardCacheDisabledTester tester;
    GURL url(https_server()->GetURL("c.com",
                                    "/cross_site_iframe_factory.html?c(d)"));
    EXPECT_TRUE(NavigateToURL(shell(), url));
    RenderFrameHostImpl* rfh_c = current_frame_host();
    RenderFrameHostImpl* rfh_d = rfh_c->child_at(0)->current_frame_host();

    EXPECT_FALSE(rfh_c->IsBackForwardCacheDisabled());
    EXPECT_FALSE(rfh_d->IsBackForwardCacheDisabled());
    EXPECT_EQ("Found 0 devices", content::EvalJs(rfh_c, R"(
        new Promise(async resolve => {
          let devices = await navigator.usb.getDevices();
          resolve("Found " + devices.length + " devices");
        });
    )"));
    EXPECT_TRUE(rfh_c->IsBackForwardCacheDisabled());
    EXPECT_FALSE(rfh_d->IsBackForwardCacheDisabled());
    EXPECT_TRUE(tester.IsDisabledForFrameWithReason(
        rfh_c->GetProcess()->GetID(), rfh_c->GetRoutingID(), "WebUSB"));
  }

  // Worker.
  {
    content::BackForwardCacheDisabledTester tester;
    GURL url(https_server()->GetURL("e.com", "/title1.html"));
    EXPECT_TRUE(NavigateToURL(shell(), url));
    EXPECT_FALSE(current_frame_host()->IsBackForwardCacheDisabled());
    EXPECT_EQ("Found 0 devices", content::EvalJs(current_frame_host(), R"(
        new Promise(async resolve => {
          const worker = new Worker("/back_forward_cache/webusb/worker.js");
          worker.onmessage = message => resolve(message.data);
          worker.postMessage("Run");
        });
    )"));
    EXPECT_TRUE(current_frame_host()->IsBackForwardCacheDisabled());
    EXPECT_TRUE(tester.IsDisabledForFrameWithReason(
        current_frame_host()->GetProcess()->GetID(),
        current_frame_host()->GetRoutingID(), "WebUSB"));
  }

  // Nested worker.
  {
    content::BackForwardCacheDisabledTester tester;
    GURL url(https_server()->GetURL("f.com", "/title1.html"));
    EXPECT_TRUE(NavigateToURL(shell(), url));
    EXPECT_FALSE(current_frame_host()->IsBackForwardCacheDisabled());
    EXPECT_EQ("Found 0 devices", content::EvalJs(current_frame_host(), R"(
        new Promise(async resolve => {
          const worker = new Worker(
            "/back_forward_cache/webusb/nested-worker.js");
          worker.onmessage = message => resolve(message.data);
          worker.postMessage("Run");
        });
    )"));
    EXPECT_TRUE(current_frame_host()->IsBackForwardCacheDisabled());
    EXPECT_TRUE(tester.IsDisabledForFrameWithReason(
        current_frame_host()->GetProcess()->GetID(),
        current_frame_host()->GetRoutingID(), "WebUSB"));
  }
}

#if !defined(OS_ANDROID)
// Check that the back-forward cache is disabled when the Serial API is used.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, Serial) {
  // Serial API requires HTTPS.
  ASSERT_TRUE(CreateHttpsServer()->Start());

  // Main document.
  {
    content::BackForwardCacheDisabledTester tester;
    GURL url(https_server()->GetURL("a.com", "/title1.html"));

    EXPECT_TRUE(NavigateToURL(shell(), url));

    EXPECT_FALSE(current_frame_host()->IsBackForwardCacheDisabled());
    EXPECT_EQ("Found 0 ports", content::EvalJs(current_frame_host(), R"(
        new Promise(async resolve => {
          let ports = await navigator.serial.getPorts();
          resolve("Found " + ports.length + " ports");
        });
    )"));
    EXPECT_TRUE(current_frame_host()->IsBackForwardCacheDisabled());
    EXPECT_TRUE(tester.IsDisabledForFrameWithReason(
        current_frame_host()->GetProcess()->GetID(),
        current_frame_host()->GetRoutingID(), "Serial"));
  }

  // Nested document.
  {
    content::BackForwardCacheDisabledTester tester;
    GURL url(https_server()->GetURL("c.com",
                                    "/cross_site_iframe_factory.html?c(d)"));
    EXPECT_TRUE(NavigateToURL(shell(), url));
    RenderFrameHostImpl* rfh_c = current_frame_host();
    RenderFrameHostImpl* rfh_d = rfh_c->child_at(0)->current_frame_host();

    EXPECT_FALSE(rfh_c->IsBackForwardCacheDisabled());
    EXPECT_FALSE(rfh_d->IsBackForwardCacheDisabled());
    EXPECT_EQ("Found 0 ports", content::EvalJs(rfh_c, R"(
        new Promise(async resolve => {
          let ports = await navigator.serial.getPorts();
          resolve("Found " + ports.length + " ports");
        });
    )"));
    EXPECT_TRUE(rfh_c->IsBackForwardCacheDisabled());
    EXPECT_FALSE(rfh_d->IsBackForwardCacheDisabled());
    EXPECT_TRUE(tester.IsDisabledForFrameWithReason(
        rfh_c->GetProcess()->GetID(), rfh_c->GetRoutingID(), "Serial"));
  }

  // Worker.
  {
    content::BackForwardCacheDisabledTester tester;
    GURL url(https_server()->GetURL("e.com", "/title1.html"));
    EXPECT_TRUE(NavigateToURL(shell(), url));
    EXPECT_FALSE(current_frame_host()->IsBackForwardCacheDisabled());
    EXPECT_EQ("Found 0 ports", content::EvalJs(current_frame_host(), R"(
        new Promise(async resolve => {
          const worker = new Worker("/back_forward_cache/serial/worker.js");
          worker.onmessage = message => resolve(message.data);
          worker.postMessage("Run");
        });
    )"));
    EXPECT_TRUE(current_frame_host()->IsBackForwardCacheDisabled());
    EXPECT_TRUE(tester.IsDisabledForFrameWithReason(
        current_frame_host()->GetProcess()->GetID(),
        current_frame_host()->GetRoutingID(), "Serial"));
  }

  // Nested worker.
  {
    content::BackForwardCacheDisabledTester tester;
    GURL url(https_server()->GetURL("f.com", "/title1.html"));
    EXPECT_TRUE(NavigateToURL(shell(), url));
    EXPECT_FALSE(current_frame_host()->IsBackForwardCacheDisabled());
    EXPECT_EQ("Found 0 ports", content::EvalJs(current_frame_host(), R"(
        new Promise(async resolve => {
          const worker = new Worker(
            "/back_forward_cache/serial/nested-worker.js");
          worker.onmessage = message => resolve(message.data);
          worker.postMessage("Run");
        });
    )"));
    EXPECT_TRUE(current_frame_host()->IsBackForwardCacheDisabled());
    EXPECT_TRUE(tester.IsDisabledForFrameWithReason(
        current_frame_host()->GetProcess()->GetID(),
        current_frame_host()->GetRoutingID(), "Serial"));
  }
}
#endif

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, Encoding) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/back_forward_cache/charset_windows-1250.html"));
  GURL url_b(embedded_test_server()->GetURL(
      "b.com", "/back_forward_cache/charset_utf-8.html"));
  url::Origin origin_a = url::Origin::Create(url_a);
  url::Origin origin_b = url::Origin::Create(url_b);

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  EXPECT_EQ(web_contents()->GetEncoding(), "windows-1250");

  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_EQ(web_contents()->GetEncoding(), "UTF-8");

  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(web_contents()->GetEncoding(), "windows-1250");
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, RestoreWhilePendingCommit) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL url3(embedded_test_server()->GetURL("c.com", "/main_document"));

  // Load a page and navigate away from it, so it is stored in the back-forward
  // cache.
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  RenderFrameHost* rfh1 = current_frame_host();
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  // Try to navigate to a new page, but leave it in a pending state.
  shell()->LoadURL(url3);
  response.WaitForRequest();

  // Navigate back and restore page from the cache, cancelling the previous
  // navigation.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(rfh1, current_frame_host());
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DoesNotCacheCrossSiteHttpPost) {
  SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Note we do a cross-site post because same-site navigations of any kind
  // aren't cached currently.
  GURL form_url(embedded_test_server()->GetURL(
      "a.com", "/form_that_posts_cross_site.html"));
  GURL redirect_target_url(embedded_test_server()->GetURL("x.com", "/echoall"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to the page with form that posts via 307 redirection to
  // |redirect_target_url| (cross-site from |form_url|).
  EXPECT_TRUE(NavigateToURL(shell(), form_url));

  // Submit the form.
  TestNavigationObserver form_post_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(ExecJs(shell(), "document.getElementById('text-form').submit()"));
  form_post_observer.Wait();

  // Verify that we arrived at the expected, redirected location.
  EXPECT_EQ(redirect_target_url,
            shell()->web_contents()->GetLastCommittedURL());
  RenderFrameDeletedObserver delete_observer_rfh(current_frame_host());

  // Navigate away. |redirect_target_url|'s page should not be cached.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  delete_observer_rfh.WaitUntilDeleted();
}

namespace {

const char kResponseWithNoCache[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Cache-Control: no-store\r\n"
    "\r\n"
    "The server speaks HTTP!";

}  // namespace

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       MainFrameWithNoStoreNotCached) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/main_document"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1. Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(web_contents());
  shell()->LoadURL(url_a);
  response.WaitForRequest();
  response.Send(kResponseWithNoCache);
  response.Done();
  observer.Wait();

  // 2. Navigate away and expect frame to be deleted.
  RenderFrameDeletedObserver delete_observer_rfh_a(current_frame_host());
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  delete_observer_rfh_a.WaitUntilDeleted();
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, SubframeWithNoStoreCached) {
  // iframe will try to load title1.html.
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/title1.html");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(web_contents());
  shell()->LoadURL(url_a);
  response.WaitForRequest();
  response.Send(kResponseWithNoCache);
  response.Done();
  observer.Wait();
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(current_frame_host());

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) Navigate back and expect everything to be restored.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_EQ(rfh_a, current_frame_host());
}

// On windows, the expected value is off by ~20ms. In order to get the
// feature out to canary, the test is disabled for WIN.
// TODO(crbug.com/1022191): Fix this for Win.
#if defined(OS_WIN)
#define MAYBE_NavigationStart DISABLED_NavigationStart
#else
#define MAYBE_NavigationStart NavigationStart
#endif
// Make sure we are exposing the duration between back navigation's
// navigationStart and the page's original navigationStart through pageshow
// event's timeStamp, and that we aren't modifying
// performance.timing.navigationStart.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, MAYBE_NavigationStart) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/back_forward_cache/record_navigation_start_time_stamp.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  double initial_page_show_time_stamp =
      EvalJs(shell(), "window.initialPageShowTimeStamp").ExtractDouble();
  EXPECT_EQ(initial_page_show_time_stamp,
            EvalJs(shell(), "window.latestPageShowTimeStamp"));
  double initial_navigation_start =
      EvalJs(shell(), "window.initialNavigationStart").ExtractDouble();

  // 2) Navigate to B. A should be in the back forward cache.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Navigate back and expect everything to be restored.
  NavigationHandleObserver observer(web_contents(), url_a);
  base::TimeTicks time_before_navigation = base::TimeTicks::Now();
  double js_time_before_navigation =
      EvalJs(shell(), "performance.now()").ExtractDouble();
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  base::TimeTicks time_after_navigation = base::TimeTicks::Now();
  double js_time_after_navigation =
      EvalJs(shell(), "performance.now()").ExtractDouble();

  // The navigation start time should be between the time we saved just before
  // calling GoBack() and the time we saved just after calling GoBack().
  base::TimeTicks back_navigation_start = observer.navigation_start();
  EXPECT_LT(time_before_navigation, back_navigation_start);
  EXPECT_GT(time_after_navigation, back_navigation_start);

  // Check JS values. window.initialNavigationStart should not change.
  EXPECT_EQ(initial_navigation_start,
            EvalJs(shell(), "window.initialNavigationStart"));
  // performance.timing.navigationStart should not change.
  EXPECT_EQ(initial_navigation_start,
            EvalJs(shell(), "performance.timing.navigationStart"));
  // window.initialPageShowTimeStamp should not change.
  EXPECT_EQ(initial_page_show_time_stamp,
            EvalJs(shell(), "window.initialPageShowTimeStamp"));
  // window.latestPageShowTimeStamp should be updated with the timestamp of the
  // last pageshow event, which occurs after the page is restored. This should
  // be greater than the initial pageshow event's timestamp.
  double latest_page_show_time_stamp =
      EvalJs(shell(), "window.latestPageShowTimeStamp").ExtractDouble();
  EXPECT_LT(initial_page_show_time_stamp, latest_page_show_time_stamp);

  // |latest_page_show_time_stamp| should be the duration between initial
  // navigation start and |back_navigation_start|. Note that since
  // performance.timing.navigationStart returns a 64-bit integer instead of
  // double, we might be losing somewhere between 0 to 1 milliseconds of
  // precision, hence the usage of EXPECT_NEAR.
  EXPECT_NEAR(
      (back_navigation_start - base::TimeTicks::UnixEpoch()).InMillisecondsF(),
      latest_page_show_time_stamp + initial_navigation_start, 1.0);
  // Expect that the back navigation start value calculated from the JS results
  // are between time taken before & after navigation, just like
  // |before_navigation_start|.
  EXPECT_LT(js_time_before_navigation, latest_page_show_time_stamp);
  EXPECT_GT(js_time_after_navigation, latest_page_show_time_stamp);
}

// Do a same document navigation and make sure we do not fire the
// DidFirstVisuallyNonEmptyPaint again
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    DoesNotFireDidFirstVisuallyNonEmptyPaintForSameDocumentNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a_1(embedded_test_server()->GetURL(
      "a.com", "/accessibility/html/a-name.html"));
  GURL url_a_2(embedded_test_server()->GetURL(
      "a.com", "/accessibility/html/a-name.html#id"));

  EXPECT_TRUE(NavigateToURL(shell(), url_a_1));
  WaitForFirstVisuallyNonEmptyPaint(shell()->web_contents());

  FirstVisuallyNonEmptyPaintObserver observer(web_contents());
  EXPECT_TRUE(NavigateToURL(shell(), url_a_2));
  // Make sure the bfcache restore code does not fire the event during commit
  // navigation.
  EXPECT_FALSE(observer.did_fire());
  EXPECT_TRUE(web_contents()->CompletedFirstVisuallyNonEmptyPaint());
}

// Make sure we fire DidFirstVisuallyNonEmptyPaint when restoring from bf-cache.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    FiresDidFirstVisuallyNonEmptyPaintWhenRestoredFromCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  WaitForFirstVisuallyNonEmptyPaint(shell()->web_contents());
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  ASSERT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  WaitForFirstVisuallyNonEmptyPaint(shell()->web_contents());

  // 3) Navigate to back to A.
  FirstVisuallyNonEmptyPaintObserver observer(web_contents());
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  // Make sure the bfcache restore code does fire the event during commit
  // navigation.
  EXPECT_TRUE(web_contents()->CompletedFirstVisuallyNonEmptyPaint());
  EXPECT_TRUE(observer.did_fire());
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       SetsThemeColorWhenRestoredFromCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/theme_color.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  WaitForFirstVisuallyNonEmptyPaint(web_contents());
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  EXPECT_EQ(web_contents()->GetThemeColor(), 0xFFFF0000u);

  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  WaitForFirstVisuallyNonEmptyPaint(web_contents());
  ASSERT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_EQ(web_contents()->GetThemeColor(), base::nullopt);

  ThemeColorObserver observer(web_contents());
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_TRUE(observer.did_fire());
  EXPECT_EQ(web_contents()->GetThemeColor(), 0xFFFF0000u);
}

// Check that an audio suspends when the page goes to the cache and can resume
// after restored.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, AudioSuspendAndResume) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  EXPECT_TRUE(ExecJs(rfh_a, R"(
    var audio = document.createElement('audio');
    document.body.appendChild(audio);

    audio.testObserverEvents = [];
    let event_list = [
      'canplaythrough',
      'pause',
      'play',
      'error',
    ];
    for (event_name of event_list) {
      let result = event_name;
      audio.addEventListener(event_name, event => {
        document.title = result;
        audio.testObserverEvents.push(result);
      });
    }

    audio.src = 'media/bear-opus.ogg';

    var timeOnFrozen = 0.0;
    audio.addEventListener('pause', () => {
      timeOnFrozen = audio.currentTime;
    });
  )"));

  // Load the media.
  {
    TitleWatcher title_watcher(shell()->web_contents(),
                               base::ASCIIToUTF16("canplaythrough"));
    title_watcher.AlsoWaitForTitle(base::ASCIIToUTF16("error"));
    EXPECT_EQ(base::ASCIIToUTF16("canplaythrough"),
              title_watcher.WaitAndGetTitle());
  }

  EXPECT_TRUE(ExecJs(rfh_a, R"(
    new Promise(async resolve => {
      audio.play();
      while (audio.currentTime === 0)
        await new Promise(r => setTimeout(r, 1));
      resolve();
    });
  )"));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Navigate back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());

  // Check that the media position is not changed when the page is in cache.
  double duration1 = EvalJs(rfh_a, "timeOnFrozen;").ExtractDouble();
  double duration2 = EvalJs(rfh_a, "audio.currentTime;").ExtractDouble();
  EXPECT_LE(0.0, duration2 - duration1);
  EXPECT_GT(0.01, duration2 - duration1);

  // Resume the media.
  EXPECT_TRUE(ExecJs(rfh_a, "audio.play();"));

  // Confirm that the media pauses automatically when going to the cache.
  // TODO(hajimehoshi): Confirm that this media automatically resumes if
  // autoplay attribute exists.
  EXPECT_EQ(ListValueOf("canplaythrough", "play", "pause", "play"),
            EvalJs(rfh_a, "audio.testObserverEvents"));
}

// Check that a video suspends when the page goes to the cache and can resume
// after restored.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, VideoSuspendAndResume) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  EXPECT_TRUE(ExecJs(rfh_a, R"(
    var video = document.createElement('video');
    document.body.appendChild(video);

    video.testObserverEvents = [];
    let event_list = [
      'canplaythrough',
      'pause',
      'play',
      'error',
    ];
    for (event_name of event_list) {
      let result = event_name;
      video.addEventListener(event_name, event => {
        document.title = result;
        video.testObserverEvents.push(result);
      });
    }

    video.src = 'media/bear.webm';

    var timeOnFrozen = 0.0;
    video.addEventListener('pause', () => {
      timeOnFrozen = video.currentTime;
    });
  )"));

  // Load the media.
  {
    TitleWatcher title_watcher(shell()->web_contents(),
                               base::ASCIIToUTF16("canplaythrough"));
    title_watcher.AlsoWaitForTitle(base::ASCIIToUTF16("error"));
    EXPECT_EQ(base::ASCIIToUTF16("canplaythrough"),
              title_watcher.WaitAndGetTitle());
  }

  EXPECT_TRUE(ExecJs(rfh_a, R"(
    new Promise(async resolve => {
      video.play();
      while (video.currentTime == 0)
        await new Promise(r => setTimeout(r, 1));
      resolve();
    });
  )"));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Navigate back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());

  // Check that the media position is not changed when the page is in cache.
  double duration1 = EvalJs(rfh_a, "timeOnFrozen;").ExtractDouble();
  double duration2 = EvalJs(rfh_a, "video.currentTime;").ExtractDouble();
  EXPECT_LE(0.0, duration2 - duration1);
  EXPECT_GT(0.01, duration2 - duration1);

  // Resume the media.
  EXPECT_TRUE(ExecJs(rfh_a, "video.play();"));

  // Confirm that the media pauses automatically when going to the cache.
  // TODO(hajimehoshi): Confirm that this media automatically resumes if
  // autoplay attribute exists.
  EXPECT_EQ(ListValueOf("canplaythrough", "play", "pause", "play"),
            EvalJs(rfh_a, "video.testObserverEvents"));
}

class SensorBackForwardCacheBrowserTest : public BackForwardCacheBrowserTest {
 protected:
  SensorBackForwardCacheBrowserTest() {
    SensorProviderProxyImpl::OverrideSensorProviderBinderForTesting(
        base::BindRepeating(
            &SensorBackForwardCacheBrowserTest::BindSensorProvider,
            base::Unretained(this)));
  }

  ~SensorBackForwardCacheBrowserTest() override {
    SensorProviderProxyImpl::OverrideSensorProviderBinderForTesting(
        base::NullCallback());
  }

  void SetUpOnMainThread() override {
    provider_ = std::make_unique<device::FakeSensorProvider>();
    provider_->SetAccelerometerData(1.0, 2.0, 3.0);

    BackForwardCacheBrowserTest::SetUpOnMainThread();
  }

  std::unique_ptr<device::FakeSensorProvider> provider_;

 private:
  void BindSensorProvider(
      mojo::PendingReceiver<device::mojom::SensorProvider> receiver) {
    provider_->Bind(std::move(receiver));
  }
};

IN_PROC_BROWSER_TEST_F(SensorBackForwardCacheBrowserTest,
                       AccelerometerNotCached) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  EXPECT_TRUE(ExecJs(rfh_a, R"(
    new Promise(resolve => {
      const sensor = new Accelerometer();
      sensor.addEventListener('reading', () => { resolve(); });
      sensor.start();
    })
  )"));

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // - Page A should not be in the cache.
  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(SensorBackForwardCacheBrowserTest, OrientationCached) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  EXPECT_TRUE(ExecJs(rfh_a, R"(
    new Promise(resolve => {
      window.addEventListener("deviceorientation", () => { resolve(); }, true)
    })
  )"));

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_THAT(rfh_a, InBackForwardCache());
}

// Tests that the orientation sensor's events are not delivered to a page in the
// back-forward cache.
//
// This sets some JS functions in the pages to enable the sensors, capture and
// validate the events. The a-page should only receive events with alpha=0, the
// b-page is allowed to receive any alpha value. The test captures 3 events in
// the a-page, then navigates to the b-page and changes the reading to have
// alpha=1. While on the b-page it captures 3 more events. If the a-page is
// still receiving events it should receive one or more of these. Finally it
// resets the reasing back to have alpha=0 and navigates back to the a-page and
// catpures 3 more events and verifies that all events on the a-page have
// alpha=1.
// Flaky on Mac and Linux ASAN/TSAN. https://crbug.com/1029238
#if defined(OS_MACOSX) || (defined(OS_LINUX) && (defined(ADDRESS_SANITIZER) || \
                                                 defined(THREAD_SANITIZER)))
#define MAYBE_SensorPausedWhileCached DISABLED_SensorPausedWhileCached
#else
#define MAYBE_SensorPausedWhileCached SensorPausedWhileCached
#endif
IN_PROC_BROWSER_TEST_F(SensorBackForwardCacheBrowserTest,
                       MAYBE_SensorPausedWhileCached) {
  ASSERT_TRUE(CreateHttpsServer()->Start());
  GURL url_a(https_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

  provider_->SetRelativeOrientationSensorData(0, 0, 0);

  // JS to cause a page to listen to, capture and validate orientation events.
  const std::string sensor_js = R"(
    // Collects events that have happened so far.
    var events = [];
    // If set, will be called by handleEvent.
    var pendingResolve = null;

    // Handles one event, pushing it to |events| and calling |pendingResolve| if
    // set.
    function handleEvent(event) {
      events.push(event);
      if (pendingResolve !== null) {
        pendingResolve('event');
        pendingResolve = null;
      }
    }

    // Returns a promise that will resolve when the events array has at least
    // |eventCountMin| elements. Returns the number of elements.
    function waitForEventsPromise(eventCountMin) {
      if (events.length >= eventCountMin) {
        return Promise.resolve(events.length);
      }
      return new Promise(resolve => {
        pendingResolve = resolve;
      }).then(() => waitForEventsPromise(eventCountMin));
    }

    // Pretty print an orientation event.
    function eventToString(event) {
      return `${event.alpha} ${event.beta} ${event.gamma}`;
    }

    // Ensure that that |expectedAlpha| matches the alpha of all events.
    function validateEvents(expectedAlpha = null) {
      if (expectedAlpha !== null) {
        let count = 0;
        for (event of events) {
          count++;
          if (Math.abs(event.alpha - expectedAlpha) > 0.01) {
            return `fail - ${count}/${events.length}: ` +
                `${expectedAlpha} != ${event.alpha} (${eventToString(event)})`;
          }
        }
      }
      return 'pass';
    }

    window.addEventListener('deviceorientation', handleEvent);
  )";

  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  ASSERT_TRUE(ExecJs(rfh_a, sensor_js));

  // Collect 3 orientation events.
  ASSERT_EQ(1, EvalJs(rfh_a, "waitForEventsPromise(1)"));
  provider_->UpdateRelativeOrientationSensorData(0, 0, 0.2);
  ASSERT_EQ(2, EvalJs(rfh_a, "waitForEventsPromise(2)"));
  provider_->UpdateRelativeOrientationSensorData(0, 0, 0.4);
  ASSERT_EQ(3, EvalJs(rfh_a, "waitForEventsPromise(3)"));
  // We should have 3 events with alpha=0.
  ASSERT_EQ("pass", EvalJs(rfh_a, "validateEvents(0)"));

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImpl* rfh_b = current_frame_host();

  ASSERT_FALSE(delete_observer_rfh_a.deleted());
  ASSERT_THAT(rfh_a, InBackForwardCache());
  ASSERT_NE(rfh_a, rfh_b);

  ASSERT_TRUE(ExecJs(rfh_b, sensor_js));

  // Collect 3 orientation events.
  provider_->SetRelativeOrientationSensorData(1, 0, 0);
  ASSERT_EQ(1, EvalJs(rfh_b, "waitForEventsPromise(1)"));
  provider_->UpdateRelativeOrientationSensorData(1, 0, 0.2);
  ASSERT_EQ(2, EvalJs(rfh_b, "waitForEventsPromise(2)"));
  provider_->UpdateRelativeOrientationSensorData(1, 0, 0.4);
  ASSERT_EQ(3, EvalJs(rfh_b, "waitForEventsPromise(3)"));
  // We should have 3 events with alpha=1.
  ASSERT_EQ("pass", EvalJs(rfh_b, "validateEvents()"));

  // 3) Go back to A.
  provider_->UpdateRelativeOrientationSensorData(0, 0, 0);
  web_contents()->GetController().GoBack();
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ASSERT_EQ(rfh_a, current_frame_host());

  // Collect 3 orientation events.
  provider_->UpdateRelativeOrientationSensorData(0, 0, 0);
  // There are 2 processes so, it's possible that more events crept in. So we
  // capture how many there are at this point and uses to wait for at least 3
  // more.
  int count = EvalJs(rfh_a, "waitForEventsPromise(4)").ExtractInt();
  provider_->UpdateRelativeOrientationSensorData(0, 0, 0.2);
  count++;
  ASSERT_EQ(count, EvalJs(rfh_a, base::StringPrintf("waitForEventsPromise(%d)",
                                                    count)));
  provider_->UpdateRelativeOrientationSensorData(0, 0, 0.4);
  count++;
  ASSERT_EQ(count, EvalJs(rfh_a, base::StringPrintf("waitForEventsPromise(%d)",
                                                    count)));

  // We should have the earlier 3 plus another 3 events with alpha=0.
  ASSERT_EQ("pass", EvalJs(rfh_a, "validateEvents(0)"));
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       AllowedFeaturesForSubframesDoNotEvict) {
  // The main purpose of this test is to check that when a state of a subframe
  // is updated, CanStoreDocument is still called for the main frame - otherwise
  // we would always evict the document, even when the feature is allowed as
  // CanStoreDocument always returns false for non-main frames.

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  // 2) Navigate to C.
  ASSERT_TRUE(NavigateToURL(shell(), url_c));

  // 3) No-op feature update on a subframe while in cache, should be no-op.
  ASSERT_FALSE(delete_observer_rfh_b.deleted());
  static_cast<blink::mojom::LocalFrameHost*>(rfh_b)
      ->DidChangeActiveSchedulerTrackedFeatures(0);

  // 4) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(current_frame_host(), rfh_a);

  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kRestored,
                FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       EvictIfCachedIsNoopIfNotCached) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_FALSE(BackForwardCache::EvictIfCached(
      current_frame_host()->GetGlobalFrameRoutingId(), kDisabledReasonForTest));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectOutcome(BackForwardCacheMetrics::HistoryNavigationOutcome::kRestored,
                FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, EvictIfCachedDoesEvict) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameDeletedObserver delete_observer_rfh_a(current_frame_host());
  GlobalFrameRoutingId id_a = current_frame_host()->GetGlobalFrameRoutingId();

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(BackForwardCache::EvictIfCached(id_a, kDisabledReasonForTest));

  // 3) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectDisabledWithReason(kDisabledReasonForTest, FROM_HERE);
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kDisableForRenderFrameHostCalled},
                    FROM_HERE);
}

// Test for functionality of memory controls in back-forward cache for low
// memory devices.
class BackForwardCacheBrowserTestForLowMemoryDevices
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Set the value of memory threshold more than the physical memory and check
    // if back-forward cache is disabled or not.
    std::string memory_threshold =
        base::NumberToString(base::SysInfo::AmountOfPhysicalMemoryMB() + 1);
    EnableFeatureAndSetParams(features::kBackForwardCacheMemoryControl,
                              "memory_threshold_for_back_forward_cache_in_mb",
                              memory_threshold);

    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

// Navigate from A to B and go back.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestForLowMemoryDevices,
                       DisableBFCacheForLowEndDevices) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Ensure that the trial starts inactive.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));

  EXPECT_FALSE(IsBackForwardCacheEnabled());

  // Ensure that we do not activate the trial when querying bfcache status,
  // which is protected by low-memory setting.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) A shouldn't be stored in back-forward cache because the physical
  // memory is less than the memory threshold.
  delete_observer_rfh_a.WaitUntilDeleted();

  // Nothing is recorded when the memory is less than the threshold value.
  ExpectOutcomeDidNotChange(FROM_HERE);
  ExpectNotRestoredDidNotChange(FROM_HERE);

  // Ensure that the trial still hasn't been activated.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));
}

// Test for functionality of memory controls in back-forward cache for high
// memory devices.
class BackForwardCacheBrowserTestForHighMemoryDevices
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Set the value of memory threshold less than the physical memory and check
    // if back-forward cache is enabled or not.
    std::string memory_threshold =
        base::NumberToString(base::SysInfo::AmountOfPhysicalMemoryMB() - 1);
    EnableFeatureAndSetParams(features::kBackForwardCacheMemoryControl,
                              "memory_threshold_for_back_forward_cache_in_mb",
                              memory_threshold);

    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

// Navigate from A to B and go back.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestForHighMemoryDevices,
                       EnableBFCacheForHighMemoryDevices) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) A should be stored in back-forward cache because the physical memory is
  // greater than the memory threshold.
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    EvictingDocumentsInRelatedSiteInstancesDoesNotRestartNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a1(embedded_test_server()->GetURL("a.com", "/title1.html#part1"));
  GURL url_a2(embedded_test_server()->GetURL("a.com", "/title1.html#part2"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A1.
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));

  // 2) Navigate to A2.
  EXPECT_TRUE(NavigateToURL(shell(), url_a2));

  // 3) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 4) Go back to A2, but do not wait for the navigation to commit.
  web_contents()->GetController().GoBack();

  // 5) Go back to A1.
  // This will attempt to evict A2 from the cache because
  // their navigation entries have related site instances, while a navigation
  // to A2 is in flight. Ensure that we do not try to restart it as it should
  // be superseded by a navigation to A1.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(url_a1, web_contents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       RTCPeerConnectionNotCached) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  EXPECT_TRUE(ExecJs(rfh_a, "new RTCPeerConnection()"));

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // - Page A should not be in the cache.
  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      FROM_HERE);
  ExpectBlocklistedFeature(
      blink::scheduler::WebSchedulerTrackedFeature::kWebRTC, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, WebLocksNotCached) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // Wait for the page to acquire a lock and ensure that it continues to do so.
  EXPECT_TRUE(ExecJs(rfh_a, R"(
    const never_resolved = new Promise(resolve => {});
    new Promise(continue_test => {
      navigator.locks.request('test', async () => {
        continue_test();
        await never_resolved;
      });
    })
  )"));

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // - Page A should not be in the cache.
  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      FROM_HERE);
  ExpectBlocklistedFeature(
      blink::scheduler::WebSchedulerTrackedFeature::kWebLocks, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       CanUseCacheWhenNavigatingAwayToErrorPage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL error_url(embedded_test_server()->GetURL("b.com", "/empty.html"));
  auto url_interceptor = URLLoaderInterceptor::SetupRequestFailForURL(
      error_url, net::ERR_DNS_TIMED_OUT);

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // 2) Navigate to an error page and expect the old page to be stored in
  // bfcache.
  EXPECT_FALSE(NavigateToURL(shell(), error_url));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Navigate back and expect the page to be restored from bfcache.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
}

// A class to help with waiting for at least one javascript dialog to be
// requested.
//
// On creation or RestartObserving, it uses set_dialog_request_callback to
// capture any future dialog request. Calling WaitForAppModalDialog() will
// either return immediately because a dialog has already been called or it will
// wait, processing events until one is requested.
class DialogObserver {
 public:
  explicit DialogObserver(Shell* shell) : shell_(shell) {}

  void RestartObserving() {
    dialog_requested_ = false;
    ShellJavaScriptDialogManager* dialog_manager =
        static_cast<ShellJavaScriptDialogManager*>(
            shell_->GetJavaScriptDialogManager(shell_->web_contents()));
    dialog_manager->set_dialog_request_callback(
        base::BindLambdaForTesting([&]() { dialog_requested_ = true; }));
  }

  bool WasDialogRequested() { return dialog_requested_; }

  void WaitForAppModalDialog() {
    if (!dialog_requested_) {
      content::WaitForAppModalDialog(shell_);
      dialog_requested_ = true;
    }
  }

 private:
  bool dialog_requested_ = false;
  Shell* shell_;
};

// Start an inifite dialogs in JS, yielding after each. The first dialog should
// be dismissed by navigation. The later dialogs should be handled gracefully
// and not appear while in BFCache. Finally, when the page comes out of BFCache,
// dialogs should appear again.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       CanUseCacheWhenPageAlertsInTimeoutLoop) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  DialogObserver dialog_observer(shell());

  EXPECT_TRUE(ExecJs(rfh_a, R"(
    function alertLoop() {
      setTimeout(alertLoop, 0);
      window.alert("alert");
    }
    // Don't block this script.
    setTimeout(alertLoop, 0);
  )"));

  dialog_observer.WaitForAppModalDialog();

  // Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();

  ASSERT_FALSE(delete_observer_rfh_a.deleted());
  ASSERT_THAT(rfh_a, InBackForwardCache());
  ASSERT_NE(rfh_a, rfh_b);

  dialog_observer.RestartObserving();

  // Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());

  // The page should still be requesting dialogs in a loop. Wait for one to be
  // requested.
  dialog_observer.WaitForAppModalDialog();
}

// UnloadOldFrame will clear all dialogs. We test that further requests for
// dialogs coming from JS do not result in the creation of a dialog. This test
// posts some dialog creation JS to the render from inside the
// CommitNavigationCallback task. This JS is then able to post a task back to
// the renders to show a dialog. By the time this task runs, we the
// RenderFrameHostImpl's is_active() should be false.
//
// This test is not perfect, it can pass simply because the renderer thread does
// not run the JS in time. Ideally it would block until the renderer posts the
// request for a dialog but it's possible to do that without creating a nested
// message loop and if we do that, we risk processing the dialog request.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DialogsCancelledAndSuppressedWhenCached) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // Let's us know whether the following callback ran. Not strictly necessary
  // since it really should run.
  bool posted_dialog_js = false;
  // Create a callback that will be called during the DidCommitNavigation task.
  WillEnterBackForwardCacheCallbackForTesting
      will_enter_back_forward_cache_callback =
          base::BindLambdaForTesting([&]() {
            // Post a dialog, it should not result in a dialog being created.
            ExecuteScriptAsync(rfh_a, R"(window.alert("alert");)");
            posted_dialog_js = true;
          });
  rfh_a->render_view_host()->SetWillEnterBackForwardCacheCallbackForTesting(
      will_enter_back_forward_cache_callback);

  DialogObserver dialog_observer(shell());

  // Try show another dialog. It should work.
  ExecuteScriptAsync(rfh_a, R"(window.alert("alert");)");
  dialog_observer.WaitForAppModalDialog();

  dialog_observer.RestartObserving();

  // Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();

  ASSERT_FALSE(delete_observer_rfh_a.deleted());
  ASSERT_THAT(rfh_a, InBackForwardCache());
  ASSERT_NE(rfh_a, rfh_b);
  // Test that the JS was run and that it didn't result in a dialog.
  ASSERT_TRUE(posted_dialog_js);
  ASSERT_FALSE(dialog_observer.WasDialogRequested());

  // Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());

  // Try show another dialog. It should work.
  ExecuteScriptAsync(rfh_a, R"(window.alert("alert");)");
  dialog_observer.WaitForAppModalDialog();
}

namespace {

class ExecJsInDidFinishNavigation : public WebContentsObserver {
 public:
  ExecJsInDidFinishNavigation(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (!navigation_handle->IsInMainFrame() ||
        !navigation_handle->HasCommitted() ||
        navigation_handle->IsSameDocument()) {
      return;
    }

    ExecuteScriptAsync(navigation_handle->GetRenderFrameHost(),
                       "var foo = 42;");
  }
};

}  // namespace

// This test checks that the message posted from DidFinishNavigation
// (ExecuteScriptAsync) is received after the message restoring the page from
// the back-forward cache (PageMsg_RestorePageFromBackForwardCache).
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       MessageFromDidFinishNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  EXPECT_TRUE(ExecJs(rfh_a, "window.alive = 'I am alive';"));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  ExecJsInDidFinishNavigation observer(shell()->web_contents());

  // 3) Go back to A. Expect the page to be restored from the cache.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ("I am alive", EvalJs(rfh_a, "window.alive"));

  // Make sure that the javascript execution requested from DidFinishNavigation
  // did not result in eviction. If the document was evicted, the document
  // would be reloaded - check that it didn't happen and the tab is not
  // loading.
  EXPECT_FALSE(web_contents()->IsLoading());

  EXPECT_EQ(rfh_a, current_frame_host());
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, WebMidiNotCached) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // - Wait until requestMIDIAccess() promise is resolved.
  EXPECT_TRUE(ExecJs(rfh_a, "navigator.requestMIDIAccess()"));

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // - Page A should not be in the cache.
  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      FROM_HERE);
  ExpectBlocklistedFeature(
      blink::scheduler::WebSchedulerTrackedFeature::kRequestedMIDIPermission,
      FROM_HERE);
}

#if defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       ChildImportanceTestForBackForwardCachedPagesTest) {
  web_contents()->SetMainFrameImportance(ChildProcessImportance::MODERATE);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  ASSERT_FALSE(delete_observer_rfh_a.deleted());

  // 3) Verify the importance of page after entering back-forward cache to be
  // "NORMAL".
  EXPECT_EQ(ChildProcessImportance::NORMAL,
            rfh_a->GetProcess()->GetEffectiveImportance());

  // 4) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // 5) Verify the importance was restored correctly after page leaves
  // back-forward cache.
  EXPECT_EQ(ChildProcessImportance::MODERATE,
            rfh_a->GetProcess()->GetEffectiveImportance());
}
#endif

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       PresentationConnectionClosed) {
  ASSERT_TRUE(CreateHttpsServer()->Start());
  GURL url_a(https_server()->GetURL(
      "a.com", "/back_forward_cache/presentation_controller.html"));

  // Navigate to A (presentation controller page).
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  auto* rfh_a = current_frame_host();
  // Start a presentation connection in A.
  MockPresentationServiceDelegate mock_presentation_service_delegate;
  auto& presentation_service = rfh_a->GetPresentationServiceForTesting();
  presentation_service.SetControllerDelegateForTesting(
      &mock_presentation_service_delegate);
  EXPECT_CALL(mock_presentation_service_delegate, StartPresentation(_, _, _));
  EXPECT_TRUE(ExecJs(rfh_a, "presentationRequest.start().then(setConnection)"));

  // Send a mock connection to the renderer.
  MockPresentationConnection mock_controller_connection;
  mojo::Receiver<PresentationConnection> controller_connection_receiver(
      &mock_controller_connection);
  mojo::Remote<PresentationConnection> receiver_connection;
  const std::string presentation_connection_id = "foo";
  presentation_service.OnStartPresentationSucceeded(
      presentation_service.start_presentation_request_id_,
      PresentationConnectionResult::New(
          blink::mojom::PresentationInfo::New(GURL("fake-url"),
                                              presentation_connection_id),
          controller_connection_receiver.BindNewPipeAndPassRemote(),
          receiver_connection.BindNewPipeAndPassReceiver()));

  // Navigate to B, make sure that the connection started in A is closed.
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));
  EXPECT_CALL(
      mock_controller_connection,
      DidClose(blink::mojom::PresentationConnectionCloseReason::WENT_AWAY));
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Navigate back to A. Ensure that connection state has been updated
  // accordingly.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_EQ(presentation_connection_id, EvalJs(rfh_a, "connection.id"));
  EXPECT_EQ("closed", EvalJs(rfh_a, "connection.state"));
  EXPECT_TRUE(EvalJs(rfh_a, "connectionClosed").ExtractBool());

  // Try to start another connection, should successfully reach the browser side
  // PresentationServiceDelegate.
  EXPECT_CALL(mock_presentation_service_delegate,
              ReconnectPresentation(_, presentation_connection_id, _, _));
  EXPECT_TRUE(ExecJs(rfh_a, "presentationRequest.reconnect(connection.id)"));
  base::RunLoop().RunUntilIdle();

  // Reset |presentation_service|'s controller delegate so that it won't try to
  // call Reset() on it on destruction time.
  presentation_service.OnDelegateDestroyed();
}

namespace {

// Subclass of FrameServiceBase for test.
class EchoImpl final : public FrameServiceBase<mojom::Echo> {
 public:
  EchoImpl(RenderFrameHost* render_frame_host,
           mojo::PendingReceiver<mojom::Echo> receiver,
           bool* deleted)
      : FrameServiceBase(render_frame_host, std::move(receiver)),
        deleted_(deleted) {}
  ~EchoImpl() final { *deleted_ = true; }

  // mojom::Echo implementation
  void EchoString(const std::string& input, EchoStringCallback callback) final {
    std::move(callback).Run(input);
  }

 private:
  bool* deleted_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, FrameServiceBase) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  mojo::Remote<mojom::Echo> echo_remote;
  bool echo_deleted = false;
  new EchoImpl(rfh_a, echo_remote.BindNewPipeAndPassReceiver(), &echo_deleted);

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // - Page A should be in the cache.
  ASSERT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(echo_deleted);

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_FALSE(echo_deleted);

  // 4) Prevent caching and navigate to B.
  BackForwardCache::DisableForRenderFrameHost(rfh_a, "test");
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  delete_observer_rfh_a.WaitUntilDeleted();
  EXPECT_TRUE(echo_deleted);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, OutstandingFetchNotCached) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/fetch");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  // Ensure that there are no lingering requests from page load itself.
  EXPECT_FALSE(rfh_a->scheduler_tracked_features() &
               (1ull << static_cast<size_t>(
                    blink::scheduler::WebSchedulerTrackedFeature::
                        kOutstandingNetworkRequestFetch)));

  // 2) Create a fetch() request.
  EXPECT_TRUE(ExecJs(rfh_a, "fetch('/fetch');"));
  response.WaitForRequest();

  // 3) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // 4) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      FROM_HERE);
  ExpectBlocklistedFeature(blink::scheduler::WebSchedulerTrackedFeature::
                               kOutstandingNetworkRequestFetch,
                           FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, OutstandingXHRNotCached) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/xhr");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  // Ensure that there are no lingering requests from page load itself.
  EXPECT_FALSE(rfh_a->scheduler_tracked_features() &
               (1ull << static_cast<size_t>(
                    blink::scheduler::WebSchedulerTrackedFeature::
                        kOutstandingNetworkRequestXHR)));

  // 2) Create a XMLHttpRequest.
  EXPECT_TRUE(ExecJs(rfh_a, R"(
    var req = new XMLHttpRequest();
    req.open("GET", "/xhr");
    req.send();
  )"));
  response.WaitForRequest();

  // 3) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // 4) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      FROM_HERE);
  ExpectBlocklistedFeature(blink::scheduler::WebSchedulerTrackedFeature::
                               kOutstandingNetworkRequestXHR,
                           FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, NotFetchedScriptNotCached) {
  net::test_server::ControllableHttpResponse response(
      embedded_test_server(),
      "/back_forward_cache/script-which-does-not-exist.js");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/back_forward_cache/page_with_nonexistent_script.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  TestNavigationObserver navigation_observer1(web_contents());
  shell()->LoadURL(url_a);
  navigation_observer1.WaitForNavigationFinished();
  response.WaitForRequest();

  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  TestNavigationObserver navigation_observer2(web_contents());
  shell()->LoadURL(url_b);
  navigation_observer2.WaitForNavigationFinished();

  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  ExpectNotRestored(
      {BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures},
      FROM_HERE);
  ExpectBlocklistedFeature(blink::scheduler::WebSchedulerTrackedFeature::
                               kOutstandingNetworkRequestOthers,
                           FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, PageshowMetrics) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const char kHistogramName[] =
      "BackForwardCache.MainFrameHasPageshowListenersOnRestore";

  const GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  const GURL url2(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to the page.
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(ExecJs(current_frame_host(), R"(
    window.foo = 42;
  )"));

  // 2) Navigate away and back.
  EXPECT_TRUE(NavigateToURL(shell(), url2));
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // As we don't get an explicit ACK when the page is restored (yet), force
  // a round-trip to the renderer to effectively flush the queue.
  EXPECT_EQ(42, EvalJs(current_frame_host(), "window.foo"));

  // Expect the back-forward restore without pageshow to be detected.
  content::FetchHistogramsFromChildProcesses();
  EXPECT_THAT(histogram_tester_.GetAllSamples(kHistogramName),
              ElementsAre(base::Bucket(0, 1)));

  EXPECT_TRUE(ExecJs(current_frame_host(), R"(
    window.addEventListener("pageshow", () => {});
  )"));

  // 3) Navigate away and back (again).
  EXPECT_TRUE(NavigateToURL(shell(), url2));
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // As we don't get an explicit ACK when the page is restored (yet), force
  // a round-trip to the renderer to effectively flush the queue.
  EXPECT_EQ(42, EvalJs(current_frame_host(), "window.foo"));

  // Expect the back-forward restore with pageshow to be detected.
  content::FetchHistogramsFromChildProcesses();
  EXPECT_THAT(histogram_tester_.GetAllSamples(kHistogramName),
              ElementsAre(base::Bucket(0, 1), base::Bucket(1, 1)));
}

// Navigate from A(B) to C and check IsCurrent status for RenderFrameHost A
// and B before and after entering back-forward cache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, CheckIsCurrent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1) Navigate to A(B).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();

  EXPECT_TRUE(rfh_a->IsCurrent());
  EXPECT_TRUE(rfh_b->IsCurrent());

  // 2) Navigate to C.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());

  EXPECT_FALSE(rfh_a->IsCurrent());
  EXPECT_FALSE(rfh_b->IsCurrent());
}

// Test that LifecycleState is updated correctly when page enters and restores
// back from BackForwardCache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       CheckLifecycleStateTransition) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to A and check the LifecycleState of A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  EXPECT_EQ(RenderFrameHostImpl::LifecycleState::kActive,
            rfh_a->lifecycle_state());

  // 2) Navigate to B, now A enters BackForwardCache. Check the LifecycleState
  // of both RenderFrameHost A and B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleState::kInBackForwardCache,
            rfh_a->lifecycle_state());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleState::kActive,
            rfh_b->lifecycle_state());

  // 3) Go back to A and check again the LifecycleState of both RenderFrameHost
  // A and B.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(RenderFrameHostImpl::LifecycleState::kActive,
            rfh_a->lifecycle_state());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleState::kInBackForwardCache,
            rfh_b->lifecycle_state());
}
}  // namespace content
