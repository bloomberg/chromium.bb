// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"

#include "base/feature_list.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/trace_event_analyzer.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/paint_preview/buildflags/buildflags.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/hit_test_region_observer.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/performance/largest_contentful_paint_type.h"
#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif
#include "ui/events/test/event_generator.h"

#if BUILDFLAG(ENABLE_PAINT_PREVIEW)
// `gn check` doesn't recognize that this is included conditionally, with the
// same condition as the dependencies.
#include "components/paint_preview/browser/paint_preview_client.h"  // nogncheck
#endif

using trace_analyzer::Query;
using trace_analyzer::TraceAnalyzer;
using trace_analyzer::TraceEvent;
using trace_analyzer::TraceEventVector;
using ukm::builders::PageLoad;

namespace {

void ValidateCandidate(int expected_size, const TraceEvent& event) {
  ASSERT_TRUE(event.HasDictArg("data"));
  base::Value::Dict data = event.GetKnownArgAsDict("data");

  const absl::optional<int> traced_size = data.FindInt("size");
  ASSERT_TRUE(traced_size.has_value());
  EXPECT_EQ(traced_size.value(), expected_size);

  const absl::optional<bool> traced_main_frame_flag =
      data.FindBool("isMainFrame");
  ASSERT_TRUE(traced_main_frame_flag.has_value());
  EXPECT_TRUE(traced_main_frame_flag.value());
}

int GetCandidateIndex(const TraceEvent& event) {
  base::Value::Dict data = event.GetKnownArgAsDict("data");
  absl::optional<int> candidate_idx = data.FindInt("candidateIndex");
  DCHECK(candidate_idx.has_value()) << "couldn't find 'candidateIndex'";

  return candidate_idx.value();
}

bool compare_candidate_index(const TraceEvent* lhs, const TraceEvent* rhs) {
  return GetCandidateIndex(*lhs) < GetCandidateIndex(*rhs);
}

void ValidateTraceEvents(std::unique_ptr<TraceAnalyzer> analyzer) {
  TraceEventVector events;
  analyzer->FindEvents(Query::EventNameIs("largestContentfulPaint::Candidate"),
                       &events);
  EXPECT_EQ(3ul, events.size());
  std::sort(events.begin(), events.end(), compare_candidate_index);

  // LCP_0 uses green-16x16.png, of size 16 x 16.
  ValidateCandidate(16 * 16, *events[0]);
  // LCP_1 uses blue96x96.png, of size 96 x 96.
  ValidateCandidate(96 * 96, *events[1]);
  // LCP_2 uses green-256x256.png, of size 16 x 16.
  ValidateCandidate(256 * 256, *events[2]);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(MetricIntegrationTest, LargestContentfulPaint) {
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  Start();
  StartTracing({"loading"});
  Load("/largest_contentful_paint.html");

  // The test harness serves files from something like http://example.com:34777
  // but the port number can vary. Extract the 'window.origin' property so we
  // can compare encountered URLs to expected values.
  const std::string window_origin =
      EvalJs(web_contents(), "window.origin").ExtractString();
  const std::string image_1_url_expected =
      base::StrCat({window_origin, "/images/green-16x16.png"});
  const std::string image_2_url_expected =
      base::StrCat({window_origin, "/images/blue96x96.png"});
  const std::string image_3_url_expected =
      base::StrCat({window_origin, "/images/green-256x256.png"});
  const std::string expected_url[3] = {
      image_1_url_expected, image_2_url_expected, image_3_url_expected};

  // Verify that the JS API yielded three LCP reports. Note that, as we resolve
  // https://github.com/WICG/largest-contentful-paint/issues/41, this test may
  // need to be updated to reflect new semantics.
  const std::string test_name[3] = {"test_first_image()", "test_larger_image()",
                                    "test_largest_image()"};
  absl::optional<double> lcp_timestamps[3];
  for (size_t i = 0; i < 3; i++) {
    waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                   TimingField::kLargestContentfulPaint);

    // std::string function_name = base::StringPrintf("run_test%zu()", i);
    content::EvalJsResult result = EvalJs(web_contents(), test_name[i]);
    EXPECT_EQ("", result.error);
    const auto& list = result.value.GetListDeprecated();
    EXPECT_EQ(1u, list.size());
    const std::string* url = list[0].FindStringPath("url");
    EXPECT_TRUE(url);
    EXPECT_EQ(*url, expected_url[i]);
    lcp_timestamps[i] = list[0].FindDoublePath("time");
    EXPECT_TRUE(lcp_timestamps[i].has_value());

    waiter->Wait();
  }

  EXPECT_LT(lcp_timestamps[0], lcp_timestamps[1])
      << "The first LCP report should be before the second";
  EXPECT_LT(lcp_timestamps[1], lcp_timestamps[2])
      << "The second LCP report should be before the third";

  // Need to navigate away from the test html page to force metrics to get
  // flushed/synced.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Check Trace Events.
  ValidateTraceEvents(StopTracingAndAnalyze());

  // Check UKM.
  // Since UKM rounds to an integer while the JS API returns a coarsened double,
  // we'll assert that the UKM and JS values are within 1.2 of each other.
  // Comparing with strict equality could round incorrectly and introduce
  // flakiness into the test.
  ExpectUKMPageLoadMetricNear(
      PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2Name,
      lcp_timestamps[2].value(), 1.2);
  ExpectUKMPageLoadMetricNear(
      PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2_MainFrameName,
      lcp_timestamps[2].value(), 1.2);

  // Check UMA.
  // Similar to UKM, rounding could introduce flakiness, so use helper to
  // compare near.
  ExpectUniqueUMAPageLoadMetricNear(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2",
      lcp_timestamps[2].value());
  ExpectUniqueUMAPageLoadMetricNear(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.MainFrame",
      lcp_timestamps[2].value());
}

IN_PROC_BROWSER_TEST_F(MetricIntegrationTest,
                       LargestContentfulPaint_SubframeInput) {
  Start();
  Load("/lcp_subframe_input.html");
  auto* sub = ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  EXPECT_EQ(EvalJs(sub, "test_step_1()").value.GetString(), "green-16x16.png");

  content::SimulateMouseClickAt(web_contents(), 0,
                                blink::WebMouseEvent::Button::kLeft,
                                gfx::Point(100, 100));

  EXPECT_EQ(EvalJs(sub, "test_step_2()").value.GetString(), "green-16x16.png");
}

#if BUILDFLAG(ENABLE_PAINT_PREVIEW)
IN_PROC_BROWSER_TEST_F(MetricIntegrationTest,
                       LargestContentfulPaintPaintPreview) {
  Start();
  StartTracing({"loading"});
  Load("/largest_contentful_paint_paint_preview.html");

  content::EvalJsResult lcp_before_paint_preview =
      EvalJs(web_contents(), "block_for_next_lcp()");
  EXPECT_EQ("", lcp_before_paint_preview.error);

  paint_preview::PaintPreviewClient::CreateForWebContents(
      web_contents());  // Is a singleton.
  auto* client =
      paint_preview::PaintPreviewClient::FromWebContents(web_contents());

  paint_preview::PaintPreviewClient::PaintPreviewParams params(
      paint_preview::RecordingPersistence::kMemoryBuffer);
  params.inner.clip_rect = gfx::Rect(0, 0, 1, 1);
  params.inner.is_main_frame = true;
  params.inner.capture_links = false;
  params.inner.max_capture_size = 50 * 1024 * 1024;
  params.inner.max_decoded_image_size_bytes = 50 * 1024 * 1024;
  params.inner.skip_accelerated_content = true;

  base::RunLoop run_loop;
  client->CapturePaintPreview(
      params, web_contents()->GetPrimaryMainFrame(),
      base::BindOnce(
          [](base::OnceClosure callback, base::UnguessableToken,
             paint_preview::mojom::PaintPreviewStatus,
             std::unique_ptr<paint_preview::CaptureResult>) {
            std::move(callback).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();

  content::EvalJsResult lcp_after_paint_preview =
      EvalJs(web_contents(), "trigger_repaint_and_block_for_next_lcp()");
  EXPECT_EQ("", lcp_after_paint_preview.error);

  // When PaintPreview creates new LCP candidates, we compare the short text and
  // the long text here, which will fail. But in order to consistently get the
  // new LCP candidate in that case, we always add a medium text in
  // `trigger_repaint_and_block_for_next_lcp`. So use a soft comparison here
  // that would permit the medium text, but not the long text.
  EXPECT_LT(lcp_after_paint_preview.value.GetDouble(),
            2 * lcp_before_paint_preview.value.GetDouble());
}
#endif

class PageViewportInLCPTest : public MetricIntegrationTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    feature_list_.InitWithFeatures(
        {blink::features::kUsePageViewportInLCP} /*enabled*/, {} /*disabled*/);
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageViewportInLCPTest, FullSizeImageInIframe) {
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  waiter->AddSubFrameExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                     TimingField::kLargestContentfulPaint);
  Start();
  StartTracing({"loading"});
  Load("/full_size_image.html");
  double lcpTime = EvalJs(web_contents(), "waitForLCP()").ExtractDouble();

  waiter->Wait();
  // Navigate away to force metrics recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // |lcpTime| is computed from 3 different JS timestamps, so use an epsilon of
  // 2 to account for coarsening and UKM integer rounding.
  ExpectUKMPageLoadMetricNear(
      PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2Name, lcpTime,
      2.0);
  ExpectUniqueUMAPageLoadMetricNear(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2", lcpTime);
}

class IsAnimatedLCPTest : public MetricIntegrationTest {
 public:
  void test_is_animated(const char* html_name,
                        blink::LargestContentfulPaintType flag_set,
                        bool expected,
                        unsigned entries = 1) {
    auto waiter =
        std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
            web_contents());
    waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                   TimingField::kLargestContentfulPaint);
    waiter->AddMinimumCompleteResourcesExpectation(entries);
    Start();
    Load(html_name);
    EXPECT_EQ(EvalJs(web_contents()->GetPrimaryMainFrame(), "run_test()").error,
              "");

    // Need to navigate away from the test html page to force metrics to get
    // flushed/synced.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
    waiter->Wait();
    ExpectUKMPageLoadMetricFlagSet(
        PageLoad::kPaintTiming_LargestContentfulPaintTypeName,
        LargestContentfulPaintTypeToUKMFlags(flag_set), expected);
  }
};

IN_PROC_BROWSER_TEST_F(IsAnimatedLCPTest, LargestContentfulPaint_IsAnimated) {
  test_is_animated("/is_animated.html",
                   blink::LargestContentfulPaintType::kAnimatedImage,
                   /*expected=*/true);
}

IN_PROC_BROWSER_TEST_F(IsAnimatedLCPTest,
                       LargestContentfulPaint_IsNotAnimated) {
  test_is_animated("/non_animated.html",
                   blink::LargestContentfulPaintType::kAnimatedImage,
                   /*expected=*/false);
}

IN_PROC_BROWSER_TEST_F(
    IsAnimatedLCPTest,
    LargestContentfulPaint_AnimatedImageWithLargerTextFirst) {
  test_is_animated("/animated_image_with_larger_text_first.html",
                   blink::LargestContentfulPaintType::kAnimatedImage,
                   /*expected=*/false);
}

class MouseoverLCPTest : public MetricIntegrationTest {
 public:
  void test_mouseover(const char* html_name,
                      blink::LargestContentfulPaintType flag_set,
                      std::string entries,
                      std::string entries2,
                      int x1,
                      int y1,
                      int x2,
                      int y2,
                      bool expected) {
    auto waiter =
        std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
            web_contents());
    waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                   TimingField::kLargestContentfulPaint);
    waiter->AddMinimumCompleteResourcesExpectation(2);
    Start();
    Load(html_name);
    EXPECT_EQ(
        EvalJs(web_contents()->GetPrimaryMainFrame(), "run_test(1)").error, "");

    // We should wait for the main frame's hit-test data to be ready before
    // sending the mouse events below to avoid flakiness.
    content::WaitForHitTestData(web_contents()->GetPrimaryMainFrame());
    // Ensure the compositor thread is aware of the mouse events.
    content::MainThreadFrameObserver frame_observer(GetRenderWidgetHost());
    frame_observer.Wait();

    // Simulate a mouse move event which will generate a mouse over event.
    EXPECT_TRUE(
        ExecJs(web_contents(),
               "chrome.gpuBenchmarking.pointerActionSequence( "
               "[{ source: 'mouse', actions: [ { name: 'pointerMove', x: " +
                   base::NumberToString(x1) +
                   ", y: " + base::NumberToString(y1) + " }, ] }], ()=>{});"));

    // Wait for a second image to load and for LCP entry to be there.
    EXPECT_EQ(EvalJs(web_contents()->GetPrimaryMainFrame(),
                     "run_test(/*entries_expected= */" + entries + ")")
                  .error,
              "");

    if (x1 != x2 || y1 != y2) {
      // Wait for 600ms before the second mouse move, as our heuristics wait for
      // 500ms after a mousemove event on an LCP image.
      constexpr auto kWaitTime = base::Milliseconds(600);
      base::PlatformThread::Sleep(kWaitTime);
      // TODO(1289726): Here we should call MoveMouseTo() a second time, but
      // currently a second mouse move call is not dispatching the event as it
      // should. So instead, we dispatch the event directly.
      EXPECT_EQ(
          EvalJs(web_contents()->GetPrimaryMainFrame(), "dispatch_mouseover()")
              .error,
          "");

      // Wait for a third image (potentially) to load and for LCP entry to be
      // there.
      EXPECT_EQ(EvalJs(web_contents()->GetPrimaryMainFrame(),
                       "run_test(/*entries_expected= */" + entries2 + ")")
                    .error,
                "");
    }
    waiter->Wait();

    // Need to navigate away from the test html page to force metrics to get
    // flushed/synced.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
    ExpectUKMPageLoadMetricFlagSet(
        PageLoad::kPaintTiming_LargestContentfulPaintTypeName,
        LargestContentfulPaintTypeToUKMFlags(flag_set), expected);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MetricIntegrationTest::SetUpCommandLine(command_line);

    // chrome.gpuBenchmarking.pointerActionSequence can be used on all
    // platforms.
    command_line->AppendSwitch(cc::switches::kEnableGpuBenchmarking);
  }
};

IN_PROC_BROWSER_TEST_F(MouseoverLCPTest,
                       LargestContentfulPaint_MouseoverOverLCPImage) {
  test_mouseover("/mouseover.html",
                 blink::LargestContentfulPaintType::kAfterMouseover,
                 /*entries=*/"2",
                 /*entries2=*/"2",
                 /*x1=*/10, /*y1=*/10,
                 /*x2=*/10, /*y2=*/10,
                 /*expected=*/true);
}

IN_PROC_BROWSER_TEST_F(MouseoverLCPTest,
                       LargestContentfulPaint_MouseoverOverLCPImageReplace) {
  test_mouseover("/mouseover.html?replace",
                 blink::LargestContentfulPaintType::kAfterMouseover,
                 /*entries=*/"2",
                 /*entries2=*/"2",
                 /*x1=*/10, /*y1=*/10,
                 /*x2=*/10, /*y2=*/10,
                 /*expected=*/true);
}

IN_PROC_BROWSER_TEST_F(MouseoverLCPTest,
                       LargestContentfulPaint_MouseoverOverBody) {
  test_mouseover("/mouseover.html",
                 blink::LargestContentfulPaintType::kAfterMouseover,
                 /*entries=*/"2",
                 /*entries2=*/"2",
                 /*x1=*/30, /*y1=*/10,
                 /*x2=*/30, /*y2=*/10,
                 /*expected=*/false);
}

IN_PROC_BROWSER_TEST_F(MouseoverLCPTest,
                       LargestContentfulPaint_MouseoverOverLCPImageThenBody) {
  test_mouseover("/mouseover.html?dispatch",
                 blink::LargestContentfulPaintType::kAfterMouseover,
                 /*entries=*/"2",
                 /*entries2=*/"3",
                 /*x1=*/10, /*y1=*/10,
                 /*x2=*/30, /*y2=*/10,
                 /*expected=*/false);
}
