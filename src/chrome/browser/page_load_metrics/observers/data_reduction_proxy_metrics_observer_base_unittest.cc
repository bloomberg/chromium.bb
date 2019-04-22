// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/data_reduction_proxy_metrics_observer_base.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/optional.h"
#include "base/process/kill.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/loader/chrome_navigation_data.h"
#include "chrome/browser/page_load_metrics/metrics_web_contents_observer.h"
#include "chrome/browser/page_load_metrics/observers/data_reduction_proxy_metrics_observer_test_utils.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "chrome/common/page_load_metrics/test/page_load_metrics_test_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/data_reduction_proxy/content/browser/data_reduction_proxy_page_load_timing.h"
#include "components/data_reduction_proxy/content/browser/data_reduction_proxy_pingback_client_impl.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/previews/content/previews_user_data.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/ip_endpoint.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"
#include "third_party/blink/public/platform/web_input_event.h"

namespace data_reduction_proxy {

namespace {

// DataReductionProxyMetricsObserver responsible for modifying data about the
// navigation in OnCommit. It is also responsible for using a passed in
// DataReductionProxyPingbackClient instead of the default.
class TestDataReductionProxyMetricsObserverBase
    : public DataReductionProxyMetricsObserverBase {
 public:
  TestDataReductionProxyMetricsObserverBase(
      content::WebContents* web_contents,
      TestPingbackClient* pingback_client,
      bool data_reduction_proxy_used,
      bool cached_data_reduction_proxy_used,
      bool lite_page_used,
      bool black_listed)
      : web_contents_(web_contents),
        pingback_client_(pingback_client),
        data_reduction_proxy_used_(data_reduction_proxy_used),
        cached_data_reduction_proxy_used_(cached_data_reduction_proxy_used),
        lite_page_used_(lite_page_used),
        black_listed_(black_listed) {}

  ~TestDataReductionProxyMetricsObserverBase() override {}

  // DataReductionProxyMetricsObserverBase:
  ObservePolicy OnCommitCalled(content::NavigationHandle* navigation_handle,
                               ukm::SourceId source_id) override {
    DataReductionProxyData* data =
        DataForNavigationHandle(web_contents_, navigation_handle);
    data->set_used_data_reduction_proxy(data_reduction_proxy_used_);
    data->set_was_cached_data_reduction_proxy_response(
        cached_data_reduction_proxy_used_);
    data->set_request_url(GURL(kDefaultTestUrl));
    data->set_lite_page_received(lite_page_used_);

    auto* previews_data = PreviewsDataForNavigationHandle(navigation_handle);
    previews_data->set_black_listed_for_lite_page(black_listed_);

    return DataReductionProxyMetricsObserverBase::OnCommitCalled(
        navigation_handle, source_id);
  }

  DataReductionProxyPingbackClient* GetPingbackClient() const override {
    return pingback_client_;
  }

  void RequestProcessDump(
      base::ProcessId pid,
      memory_instrumentation::MemoryInstrumentation::RequestGlobalDumpCallback
          callback) override {
    memory_instrumentation::mojom::GlobalMemoryDumpPtr global_dump(
        memory_instrumentation::mojom::GlobalMemoryDump::New());

    memory_instrumentation::mojom::ProcessMemoryDumpPtr pmd(
        memory_instrumentation::mojom::ProcessMemoryDump::New());
    pmd->pid = pid;
    pmd->process_type = memory_instrumentation::mojom::ProcessType::RENDERER;
    pmd->os_dump = memory_instrumentation::mojom::OSMemDump::New();
    pmd->os_dump->private_footprint_kb = kMemoryKb;

    global_dump->process_dumps.push_back(std::move(pmd));
    std::move(callback).Run(true,
                            memory_instrumentation::GlobalMemoryDump::MoveFrom(
                                std::move(global_dump)));
  }

 private:
  content::WebContents* web_contents_;
  TestPingbackClient* pingback_client_;
  bool data_reduction_proxy_used_;
  bool cached_data_reduction_proxy_used_;
  bool lite_page_used_;
  bool black_listed_;

  DISALLOW_COPY_AND_ASSIGN(TestDataReductionProxyMetricsObserverBase);
};

class DataReductionProxyMetricsObserverBaseTest
    : public DataReductionProxyMetricsObserverTestBase {
 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        std::make_unique<TestDataReductionProxyMetricsObserverBase>(
            web_contents(), pingback_client(), data_reduction_proxy_used(),
            cached_data_reduction_proxy_used(), is_using_lite_page(),
            black_listed()));
  }
};

}  // namespace

TEST_F(DataReductionProxyMetricsObserverBaseTest, OnCompletePingback) {
  ResetTest();
  // Verify that when data reduction proxy was used the correct timing
  // information is sent to SendPingback.
  RunTestAndNavigateToUntrackedUrl(true, false, false);
  ValidateTimes();

  ResetTest();
  // Verify that when data reduction proxy was used but first image paint is
  // unset, the correct timing information is sent to SendPingback.
  timing()->paint_timing->first_image_paint = base::nullopt;
  RunTestAndNavigateToUntrackedUrl(true, false, false);
  ValidateTimes();

  ResetTest();
  // Verify that when data reduction proxy was used but first contentful paint
  // is unset, SendPingback is not called.
  timing()->paint_timing->first_contentful_paint = base::nullopt;
  RunTestAndNavigateToUntrackedUrl(true, false, false);
  ValidateTimes();

  ResetTest();
  // Verify that when data reduction proxy was used but first meaningful paint
  // is unset, SendPingback is not called.
  timing()->paint_timing->first_meaningful_paint = base::nullopt;
  RunTestAndNavigateToUntrackedUrl(true, false, false);
  ValidateTimes();

  ResetTest();
  // Verify that when data reduction proxy was used but load event start is
  // unset, SendPingback is not called.
  timing()->document_timing->load_event_start = base::nullopt;
  RunTestAndNavigateToUntrackedUrl(true, false, false);
  ValidateTimes();
  ValidateLoFiInPingback(false);

  ResetTest();
  // Verify that when an opt out occurs, that it is reported in the pingback.
  timing()->document_timing->load_event_start = base::nullopt;
  RunTest(true, true, true, false);
  observer()->BroadcastEventToObservers(PreviewsUITabHelper::OptOutEventKey());
  NavigateToUntrackedUrl();
  ValidateTimes();
  ValidateLoFiInPingback(false);

  ResetTest();
  std::unique_ptr<DataReductionProxyData> data =
      std::make_unique<DataReductionProxyData>();
  data->set_used_data_reduction_proxy(true);
  data->set_request_url(GURL(kDefaultTestUrl));
  data->set_lofi_received(true);

  // Verify LoFi is tracked when a LoFi response is received.
  page_load_metrics::ExtraRequestCompleteInfo resource = {
      GURL(kResourceUrl),
      net::IPEndPoint(),
      -1 /* frame_tree_node_id */,
      true /*was_cached*/,
      1024 * 40 /* raw_body_bytes */,
      0 /* original_network_content_length */,
      std::move(data),
      content::ResourceType::RESOURCE_TYPE_SCRIPT,
      0,
      {} /* load_timing()info */};

  RunTest(true, false, false, false);
  SimulateLoadedResource(resource);
  NavigateToUntrackedUrl();
  ValidateTimes();
  ValidateLoFiInPingback(true);
  ValidateBlackListInPingback(false);

  ResetTest();
  RunTest(true, false, false, true);
  NavigateToUntrackedUrl();
  ValidateBlackListInPingback(true);

  ResetTest();
  // Verify that when data reduction proxy was not used, SendPingback is not
  // called.
  RunTestAndNavigateToUntrackedUrl(false, false, false);
  EXPECT_FALSE(pingback_client_->send_pingback_called());

  ResetTest();
  cached_data_reduction_proxy_used_ = true;
  RunTestAndNavigateToUntrackedUrl(false, false, false);
  EXPECT_TRUE(pingback_client_->send_pingback_called());
  cached_data_reduction_proxy_used_ = false;

  ResetTest();
  // Verify that when the holdback experiment is enabled, a pingback is sent.
  base::FieldTrialList field_trial_list(nullptr);
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "DataCompressionProxyHoldback", "Enabled"));
  RunTestAndNavigateToUntrackedUrl(true, false, false);
  EXPECT_TRUE(pingback_client_->send_pingback_called());
}

TEST_F(DataReductionProxyMetricsObserverBaseTest, TouchScrollEventCount) {
  struct TestCase {
    std::vector<FakeInputEvent> events;
    uint32_t want_touch;
    uint32_t want_scroll;
  };
  const TestCase test_cases[] = {
      {
          // Test zero value.
          {},
          0 /* want_touch */,
          0 /* want_scroll */,
      },
      {
          // Test all inputs, should only count the ones we care about.
          {
              FakeInputEvent(blink::WebInputEvent::kMouseDown),
              FakeInputEvent(blink::WebInputEvent::kMouseUp),
              FakeInputEvent(blink::WebInputEvent::kMouseMove),
              FakeInputEvent(blink::WebInputEvent::kMouseEnter),
              FakeInputEvent(blink::WebInputEvent::kMouseLeave),
              FakeInputEvent(blink::WebInputEvent::kContextMenu),
              FakeInputEvent(blink::WebInputEvent::kMouseWheel),
              FakeInputEvent(blink::WebInputEvent::kRawKeyDown),
              FakeInputEvent(blink::WebInputEvent::kKeyDown),
              FakeInputEvent(blink::WebInputEvent::kKeyUp),
              FakeInputEvent(blink::WebInputEvent::kChar),
              FakeInputEvent(blink::WebInputEvent::kGestureScrollBegin),
              FakeInputEvent(blink::WebInputEvent::kGestureScrollEnd),
              FakeInputEvent(blink::WebInputEvent::kGestureScrollUpdate),
              FakeInputEvent(blink::WebInputEvent::kGestureFlingStart),
              FakeInputEvent(blink::WebInputEvent::kGestureFlingCancel),
              FakeInputEvent(blink::WebInputEvent::kGesturePinchBegin),
              FakeInputEvent(blink::WebInputEvent::kGesturePinchEnd),
              FakeInputEvent(blink::WebInputEvent::kGesturePinchUpdate),
              FakeInputEvent(blink::WebInputEvent::kGestureTapDown),
              FakeInputEvent(blink::WebInputEvent::kGestureShowPress),
              FakeInputEvent(blink::WebInputEvent::kGestureTap),
              FakeInputEvent(blink::WebInputEvent::kGestureTapCancel),
              FakeInputEvent(blink::WebInputEvent::kGestureLongPress),
              FakeInputEvent(blink::WebInputEvent::kGestureLongTap),
              FakeInputEvent(blink::WebInputEvent::kGestureTwoFingerTap),
              FakeInputEvent(blink::WebInputEvent::kGestureTapUnconfirmed),
              FakeInputEvent(blink::WebInputEvent::kGestureDoubleTap),
              FakeInputEvent(blink::WebInputEvent::kTouchStart),
              FakeInputEvent(blink::WebInputEvent::kTouchMove),
              FakeInputEvent(blink::WebInputEvent::kTouchEnd),
              FakeInputEvent(blink::WebInputEvent::kTouchCancel),
              FakeInputEvent(blink::WebInputEvent::kTouchScrollStarted),
              FakeInputEvent(blink::WebInputEvent::kPointerDown),
              FakeInputEvent(blink::WebInputEvent::kPointerUp),
              FakeInputEvent(blink::WebInputEvent::kPointerMove),
              FakeInputEvent(blink::WebInputEvent::kPointerCancel),
              FakeInputEvent(blink::WebInputEvent::kPointerCausedUaAction),

          },
          2 /* want_touch */,
          3 /* want_scroll */,
      },
      {
          // Test all inputs, with the ones we care about repeated.
          {
              FakeInputEvent(blink::WebInputEvent::kMouseDown),
              FakeInputEvent(blink::WebInputEvent::kMouseUp),
              FakeInputEvent(blink::WebInputEvent::kMouseMove),
              FakeInputEvent(blink::WebInputEvent::kMouseEnter),
              FakeInputEvent(blink::WebInputEvent::kMouseLeave),
              FakeInputEvent(blink::WebInputEvent::kContextMenu),
              FakeInputEvent(blink::WebInputEvent::kMouseWheel),
              FakeInputEvent(blink::WebInputEvent::kRawKeyDown),
              FakeInputEvent(blink::WebInputEvent::kKeyDown),
              FakeInputEvent(blink::WebInputEvent::kKeyUp),
              FakeInputEvent(blink::WebInputEvent::kChar),
              FakeInputEvent(blink::WebInputEvent::kGestureScrollBegin),
              FakeInputEvent(blink::WebInputEvent::kGestureScrollEnd),
              FakeInputEvent(blink::WebInputEvent::kGestureScrollUpdate),
              FakeInputEvent(blink::WebInputEvent::kGestureFlingStart),
              FakeInputEvent(blink::WebInputEvent::kGestureFlingCancel),
              FakeInputEvent(blink::WebInputEvent::kGesturePinchBegin),
              FakeInputEvent(blink::WebInputEvent::kGesturePinchEnd),
              FakeInputEvent(blink::WebInputEvent::kGesturePinchUpdate),
              FakeInputEvent(blink::WebInputEvent::kGestureTapDown),
              FakeInputEvent(blink::WebInputEvent::kGestureShowPress),
              FakeInputEvent(blink::WebInputEvent::kGestureTap),
              FakeInputEvent(blink::WebInputEvent::kGestureTapCancel),
              FakeInputEvent(blink::WebInputEvent::kGestureLongPress),
              FakeInputEvent(blink::WebInputEvent::kGestureLongTap),
              FakeInputEvent(blink::WebInputEvent::kGestureTwoFingerTap),
              FakeInputEvent(blink::WebInputEvent::kGestureTapUnconfirmed),
              FakeInputEvent(blink::WebInputEvent::kGestureDoubleTap),
              FakeInputEvent(blink::WebInputEvent::kTouchStart),
              FakeInputEvent(blink::WebInputEvent::kTouchMove),
              FakeInputEvent(blink::WebInputEvent::kTouchEnd),
              FakeInputEvent(blink::WebInputEvent::kTouchCancel),
              FakeInputEvent(blink::WebInputEvent::kTouchScrollStarted),
              FakeInputEvent(blink::WebInputEvent::kPointerDown),
              FakeInputEvent(blink::WebInputEvent::kPointerUp),
              FakeInputEvent(blink::WebInputEvent::kPointerMove),
              FakeInputEvent(blink::WebInputEvent::kPointerCancel),
              FakeInputEvent(blink::WebInputEvent::kPointerCausedUaAction),
              // Repeat.
              FakeInputEvent(blink::WebInputEvent::kMouseDown),
              FakeInputEvent(blink::WebInputEvent::kGestureTap),
              FakeInputEvent(blink::WebInputEvent::kMouseWheel),
              FakeInputEvent(blink::WebInputEvent::kGestureScrollUpdate),
              FakeInputEvent(blink::WebInputEvent::kGestureFlingStart),
          },
          4 /* want_touch */,
          6 /* want_scroll */,
      },
  };

  for (const TestCase& test_case : test_cases) {
    ResetTest();
    RunTest(true, false, false, false);

    for (const blink::WebInputEvent& event : test_case.events)
      SimulateInputEvent(event);

    NavigateToUntrackedUrl();
    EXPECT_EQ(pingback_client_->timing()->touch_count, test_case.want_touch);
    EXPECT_EQ(pingback_client_->timing()->scroll_count, test_case.want_scroll);
  }
}

TEST_F(DataReductionProxyMetricsObserverBaseTest,
       ProcessIdSentOnRendererCrash) {
  ResetTest();
  RunTest(true, false, false, false);
  std::unique_ptr<DataReductionProxyData> data =
      std::make_unique<DataReductionProxyData>();
  data->set_used_data_reduction_proxy(true);
  data->set_request_url(GURL(kDefaultTestUrl));
  SimulateRendererCrash();

  // When the renderer crashes, the pingback should report that.
  ValidateRendererCrash(true);

  ResetTest();
  RunTest(true, false, false, false);
  data = std::make_unique<DataReductionProxyData>();
  data->set_used_data_reduction_proxy(true);
  data->set_request_url(GURL(kDefaultTestUrl));
  NavigateToUntrackedUrl();

  // When the renderer does not crash, the pingback should report that.
  ValidateRendererCrash(false);
}

}  // namespace data_reduction_proxy
