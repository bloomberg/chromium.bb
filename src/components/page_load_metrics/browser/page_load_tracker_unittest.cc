// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/page_load_tracker.h"

#include "base/containers/flat_map.h"
#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_content_test_harness.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace page_load_metrics {

namespace {

const char kTestUrl[] = "https://a.test/";

struct PageLoadMetricsObserverEvents final {
  bool was_ready_to_commit_next_navigation = false;
  bool was_started = false;
  bool was_fenced_frames_started = false;
  bool was_prerender_started = false;
  bool was_committed = false;
  bool was_sub_frame_deleted = false;
  bool was_prerendered_page_activated = false;
  size_t sub_frame_navigation_count = 0;
};

class TestPageLoadMetricsObserver final : public PageLoadMetricsObserver {
 public:
  TestPageLoadMetricsObserver(raw_ptr<PageLoadMetricsObserverEvents> events)
      : events_(events) {}

  void StopObservingOnPrerender() { stop_on_prerender_ = true; }
  void StopObservingOnFencedFrames() { stop_on_fenced_frames_ = true; }

 private:
  void ReadyToCommitNextNavigation(
      content::NavigationHandle* navigation_handle) override {
    EXPECT_FALSE(events_->was_ready_to_commit_next_navigation);
    events_->was_ready_to_commit_next_navigation = true;
  }
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override {
    EXPECT_FALSE(events_->was_started);
    events_->was_started = true;
    return CONTINUE_OBSERVING;
  }
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override {
    EXPECT_FALSE(events_->was_fenced_frames_started);
    events_->was_fenced_frames_started = true;
    return stop_on_fenced_frames_ ? STOP_OBSERVING : CONTINUE_OBSERVING;
  }
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override {
    EXPECT_FALSE(events_->was_prerender_started);
    events_->was_prerender_started = true;
    return stop_on_prerender_ ? STOP_OBSERVING : CONTINUE_OBSERVING;
  }
  ObservePolicy OnCommit(
      content::NavigationHandle* navigation_handle) override {
    EXPECT_FALSE(events_->was_committed);
    events_->was_committed = true;
    return CONTINUE_OBSERVING;
  }
  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle) override {
    events_->sub_frame_navigation_count++;
  }
  void OnSubFrameDeleted(int frame_tree_node_id) override {
    events_->was_sub_frame_deleted = true;
  }
  void DidActivatePrerenderedPage(
      content::NavigationHandle* navigation_handle) override {
    EXPECT_FALSE(events_->was_prerendered_page_activated);
    events_->was_prerendered_page_activated = true;

    EXPECT_NE(ukm::kInvalidSourceId, GetDelegate().GetPageUkmSourceId());
  }

  bool stop_on_prerender_ = false;
  bool stop_on_fenced_frames_ = false;

  // Event records should be owned outside this class as this instance will be
  // automatically destructed on STOP_OBSERVING, and so on.
  raw_ptr<PageLoadMetricsObserverEvents> events_;
};

class PageLoadTrackerTest : public PageLoadMetricsObserverContentTestHarness {
 public:
  PageLoadTrackerTest()
      : observer_(new TestPageLoadMetricsObserver(&events_)) {}

 protected:
  void SetTargetUrl(const std::string& url) { target_url_ = GURL(url); }
  const PageLoadMetricsObserverEvents& GetEvents() const { return events_; }
  ukm::SourceId GetObservedUkmSourceIdFor(const std::string& url) {
    return ukm_source_ids_[url];
  }

  void StopObservingOnPrerender() { observer_->StopObservingOnPrerender(); }
  void StopObservingOnFencedFrames() {
    observer_->StopObservingOnFencedFrames();
  }

 private:
  void RegisterObservers(PageLoadTracker* tracker) override {
    ukm_source_ids_.emplace(tracker->GetUrl().spec(),
                            tracker->GetPageUkmSourceIdForTesting());

    if (tracker->GetUrl() != target_url_)
      return;

    EXPECT_FALSE(is_observer_passed_);
    tracker->AddObserver(std::unique_ptr<PageLoadMetricsObserver>(observer_));
    is_observer_passed_ = true;
  }

  base::flat_map<std::string, ukm::SourceId> ukm_source_ids_;

  PageLoadMetricsObserverEvents events_;
  raw_ptr<TestPageLoadMetricsObserver> observer_;
  bool is_observer_passed_ = false;

  GURL target_url_;
};

TEST_F(PageLoadTrackerTest, PrimaryPageType) {
  // Target URL to monitor the tracker via the test observer.
  SetTargetUrl(kTestUrl);

  // Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Check observer behaviors.
  EXPECT_TRUE(GetEvents().was_started);
  EXPECT_FALSE(GetEvents().was_fenced_frames_started);
  EXPECT_FALSE(GetEvents().was_prerender_started);
  EXPECT_TRUE(GetEvents().was_committed);

  // Check metrics.
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kPageLoadTrackerPageType,
      internal::PageLoadTrackerPageType::kPrimaryPage, 1);

  // Navigate out.
  tester()->NavigateToUntrackedUrl();

  // Check observer behaviors.
  EXPECT_TRUE(GetEvents().was_ready_to_commit_next_navigation);

  // Check ukm::SourceId.
  EXPECT_NE(ukm::kInvalidSourceId, GetObservedUkmSourceIdFor(kTestUrl));
}

TEST_F(PageLoadTrackerTest, EventForwarding) {
  // Target URL to monitor the tracker via the test observer.
  SetTargetUrl(kTestUrl);
  StopObservingOnFencedFrames();

  // Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Add a fenced frame.
  content::RenderFrameHost* fenced_frame_root =
      content::RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
          ->AppendFencedFrame();
  {
    const char kFencedFramesUrl[] = "https://a.test/fenced_frames";
    auto simulator = content::NavigationSimulator::CreateRendererInitiated(
        GURL(kFencedFramesUrl), fenced_frame_root);
    ASSERT_NE(nullptr, simulator);
    simulator->Commit();
  }

  // Check observer behaviors.
  // The navigation and frame deletion in the FencedFrames should be observed as
  // sub-frame events.
  EXPECT_TRUE(GetEvents().was_started);
  EXPECT_FALSE(GetEvents().was_fenced_frames_started);
  EXPECT_FALSE(GetEvents().was_prerender_started);
  EXPECT_TRUE(GetEvents().was_committed);
  EXPECT_FALSE(GetEvents().was_sub_frame_deleted);
  EXPECT_EQ(1u, GetEvents().sub_frame_navigation_count);

  // Navigate out.
  {
    const char kFencedFramesNavigationUrl[] = "https://b.test/fenced_frames";
    auto simulator = content::NavigationSimulator::CreateRendererInitiated(
        GURL(kFencedFramesNavigationUrl), fenced_frame_root);
    ASSERT_NE(nullptr, simulator);
    simulator->Commit();
  }

  // Check observer behaviors again after the render frame's deletion.
  // TODO(https://crbug.com/1301880): RenderFrameDeleted() doesn't seem called
  // and following check fails. Revisit this issue later to clarify the
  // expectations.
  // EXPECT_TRUE(GetEvents().was_sub_frame_deleted);
  EXPECT_EQ(2u, GetEvents().sub_frame_navigation_count);
}

TEST_F(PageLoadTrackerTest, PrerenderPageType) {
  // Target URL to monitor the tracker via the test observer.
  const char kPrerenderingUrl[] = "https://a.test/prerender";
  SetTargetUrl(kPrerenderingUrl);

  // Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Add a prerender page.
  content::WebContentsTester::For(web_contents())
      ->AddPrerenderAndCommitNavigation(GURL(kPrerenderingUrl));

  // Check observer behaviors.
  EXPECT_FALSE(GetEvents().was_started);
  EXPECT_FALSE(GetEvents().was_fenced_frames_started);
  EXPECT_TRUE(GetEvents().was_prerender_started);
  EXPECT_TRUE(GetEvents().was_committed);

  // Check metrics.
  tester()->histogram_tester().ExpectBucketCount(
      internal::kPageLoadTrackerPageType,
      internal::PageLoadTrackerPageType::kPrimaryPage, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kPageLoadTrackerPageType,
      internal::PageLoadTrackerPageType::kPrerenderPage, 1);

  // Check ukm::SourceId.
  EXPECT_NE(ukm::kInvalidSourceId, GetObservedUkmSourceIdFor(kTestUrl));
  EXPECT_EQ(ukm::kInvalidSourceId, GetObservedUkmSourceIdFor(kPrerenderingUrl));
}

TEST_F(PageLoadTrackerTest, FencedFramesPageType) {
  // Target URL to monitor the tracker via the test observer.
  const char kFencedFramesUrl[] = "https://a.test/fenced_frames";
  SetTargetUrl(kFencedFramesUrl);

  // Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Add a fenced frame.
  content::RenderFrameHost* fenced_frame_root =
      content::RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
          ->AppendFencedFrame();
  {
    auto simulator = content::NavigationSimulator::CreateRendererInitiated(
        GURL(kFencedFramesUrl), fenced_frame_root);
    ASSERT_NE(nullptr, simulator);
    simulator->Commit();
  }

  // Check observer
  // behaviors.
  EXPECT_FALSE(GetEvents().was_started);
  EXPECT_TRUE(GetEvents().was_fenced_frames_started);
  EXPECT_FALSE(GetEvents().was_prerender_started);
  EXPECT_TRUE(GetEvents().was_committed);

  // Check metrics.
  tester()->histogram_tester().ExpectBucketCount(
      internal::kPageLoadTrackerPageType,
      internal::PageLoadTrackerPageType::kPrimaryPage, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kPageLoadTrackerPageType,
      internal::PageLoadTrackerPageType::kFencedFramesPage, 1);

  // Check ukm::SourceId.
  EXPECT_NE(ukm::kInvalidSourceId, GetObservedUkmSourceIdFor(kTestUrl));
  EXPECT_EQ(GetObservedUkmSourceIdFor(kTestUrl),
            GetObservedUkmSourceIdFor(kFencedFramesUrl));

  // Navigate out.
  {
    auto simulator = content::NavigationSimulator::CreateRendererInitiated(
        GURL(kTestUrl), fenced_frame_root);
    ASSERT_NE(nullptr, simulator);
    simulator->Commit();
  }

  // Check observer behaviors.
  EXPECT_TRUE(GetEvents().was_ready_to_commit_next_navigation);
}

TEST_F(PageLoadTrackerTest, StopObservingOnPrerender) {
  // Target URL to monitor the tracker via the test observer.
  const char kPrerenderingUrl[] = "https://a.test/prerender";
  SetTargetUrl(kPrerenderingUrl);
  StopObservingOnPrerender();

  // Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Add a prerender page.
  content::WebContentsTester::For(web_contents())
      ->AddPrerenderAndCommitNavigation(GURL(kPrerenderingUrl));

  // Check observer behaviors.
  EXPECT_FALSE(GetEvents().was_started);
  EXPECT_FALSE(GetEvents().was_fenced_frames_started);
  EXPECT_TRUE(GetEvents().was_prerender_started);
  EXPECT_FALSE(GetEvents().was_committed);
}

TEST_F(PageLoadTrackerTest, StopObservingOnFencedFrames) {
  // Target URL to monitor the tracker via the test observer.
  const char kFencedFramesUrl[] = "https://a.test/fenced_frames";
  SetTargetUrl(kFencedFramesUrl);
  StopObservingOnFencedFrames();

  // Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Add a fenced frame.
  content::RenderFrameHost* fenced_frame_root =
      content::RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
          ->AppendFencedFrame();
  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      GURL(kFencedFramesUrl), fenced_frame_root);
  ASSERT_NE(nullptr, simulator);
  simulator->Commit();

  // Check observer behaviors.
  EXPECT_FALSE(GetEvents().was_started);
  EXPECT_TRUE(GetEvents().was_fenced_frames_started);
  EXPECT_FALSE(GetEvents().was_prerender_started);
  EXPECT_FALSE(GetEvents().was_committed);
}

TEST_F(PageLoadTrackerTest, ResumeOnPrerenderActivation) {
  // Target URL to monitor the tracker via the test observer.
  const char kPrerenderingUrl[] = "https://a.test/prerender";
  SetTargetUrl(kPrerenderingUrl);

  // Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Add a prerender page.
  content::WebContentsTester::For(web_contents())
      ->AddPrerenderAndCommitNavigation(GURL(kPrerenderingUrl));

  // Check observer behaviors.
  EXPECT_FALSE(GetEvents().was_started);
  EXPECT_FALSE(GetEvents().was_fenced_frames_started);
  EXPECT_TRUE(GetEvents().was_prerender_started);
  EXPECT_TRUE(GetEvents().was_committed);
  EXPECT_FALSE(GetEvents().was_prerendered_page_activated);

  // Activate the prerendered page.
  content::WebContentsTester::For(web_contents())
      ->ActivatePrerenderedPage(GURL(kPrerenderingUrl));

  EXPECT_TRUE(GetEvents().was_prerendered_page_activated);
}

}  // namespace

}  // namespace page_load_metrics
