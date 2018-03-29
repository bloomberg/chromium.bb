// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/timing/WindowPerformance.h"

#include "core/frame/PerformanceMonitor.h"
#include "core/loader/DocumentLoadTiming.h"
#include "core/loader/DocumentLoader.h"
#include "core/testing/DummyPageHolder.h"
#include "core/timing/DOMWindowPerformance.h"
#include "platform/testing/HistogramTester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

static const int kTimeOrigin = 500;

namespace {

const char kStartMarkForMeasureHistogram[] =
    "Performance.PerformanceMeasurePassedInParameter.StartMark";
const char kEndMarkForMeasureHistogram[] =
    "Performance.PerformanceMeasurePassedInParameter.EndMark";

class FakeTimer {
 public:
  FakeTimer(double init_time) {
    g_mock_time = init_time;
    original_time_function_ =
        WTF::SetTimeFunctionsForTesting(GetMockTimeInSeconds);
  }

  ~FakeTimer() { WTF::SetTimeFunctionsForTesting(original_time_function_); }

  static double GetMockTimeInSeconds() { return g_mock_time; }

  void AdvanceTimer(double duration) { g_mock_time += duration; }

 private:
  TimeFunction original_time_function_;
  static double g_mock_time;
};

double FakeTimer::g_mock_time = 1000.;

}  // namespace

class WindowPerformanceTest : public testing::Test {
 protected:
  void SetUp() override {
    page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
    page_holder_->GetDocument().SetURL(KURL("https://example.com"));
    performance_ =
        WindowPerformance::Create(page_holder_->GetDocument().domWindow());
    performance_->time_origin_ = TimeTicksFromSeconds(kTimeOrigin);

    // Create another dummy page holder and pretend this is the iframe.
    another_page_holder_ = DummyPageHolder::Create(IntSize(400, 300));
    another_page_holder_->GetDocument().SetURL(KURL("https://iframed.com/bar"));
  }

  bool ObservingLongTasks() {
    return PerformanceMonitor::InstrumentingMonitor(
        performance_->GetExecutionContext());
  }

  void AddLongTaskObserver() {
    // simulate with filter options.
    performance_->observer_filter_options_ |= PerformanceEntry::kLongTask;
  }

  void RemoveLongTaskObserver() {
    // simulate with filter options.
    performance_->observer_filter_options_ = PerformanceEntry::kInvalid;
  }

  void SimulateDidProcessLongTask() {
    auto* monitor = GetFrame()->GetPerformanceMonitor();
    monitor->WillExecuteScript(GetDocument());
    monitor->DidExecuteScript();
    monitor->DidProcessTask(0, 1);
  }

  LocalFrame* GetFrame() const { return &page_holder_->GetFrame(); }

  Document* GetDocument() const { return &page_holder_->GetDocument(); }

  LocalFrame* AnotherFrame() const { return &another_page_holder_->GetFrame(); }

  Document* AnotherDocument() const {
    return &another_page_holder_->GetDocument();
  }

  String SanitizedAttribution(ExecutionContext* context,
                              bool has_multiple_contexts,
                              LocalFrame* observer_frame) {
    return WindowPerformance::SanitizedAttribution(
               context, has_multiple_contexts, observer_frame)
        .first;
  }

  Persistent<WindowPerformance> performance_;
  std::unique_ptr<DummyPageHolder> page_holder_;
  std::unique_ptr<DummyPageHolder> another_page_holder_;
};

TEST_F(WindowPerformanceTest, LongTaskObserverInstrumentation) {
  performance_->UpdateLongTaskInstrumentation();
  EXPECT_FALSE(ObservingLongTasks());

  // Adding LongTask observer (with filer option) enables instrumentation.
  AddLongTaskObserver();
  performance_->UpdateLongTaskInstrumentation();
  EXPECT_TRUE(ObservingLongTasks());

  // Removing LongTask observer disables instrumentation.
  RemoveLongTaskObserver();
  performance_->UpdateLongTaskInstrumentation();
  EXPECT_FALSE(ObservingLongTasks());
}

TEST_F(WindowPerformanceTest, SanitizedLongTaskName) {
  // Unable to attribute, when no execution contents are available.
  EXPECT_EQ("unknown", SanitizedAttribution(nullptr, false, GetFrame()));

  // Attribute for same context (and same origin).
  EXPECT_EQ("self", SanitizedAttribution(GetDocument(), false, GetFrame()));

  // Unable to attribute, when multiple script execution contents are involved.
  EXPECT_EQ("multiple-contexts",
            SanitizedAttribution(GetDocument(), true, GetFrame()));
}

TEST_F(WindowPerformanceTest, SanitizedLongTaskName_CrossOrigin) {
  // Unable to attribute, when no execution contents are available.
  EXPECT_EQ("unknown", SanitizedAttribution(nullptr, false, GetFrame()));

  // Attribute for same context (and same origin).
  EXPECT_EQ("cross-origin-unreachable",
            SanitizedAttribution(AnotherDocument(), false, GetFrame()));
}

// https://crbug.com/706798: Checks that after navigation that have replaced the
// window object, calls to not garbage collected yet WindowPerformance belonging
// to the old window do not cause a crash.
TEST_F(WindowPerformanceTest, NavigateAway) {
  AddLongTaskObserver();
  performance_->UpdateLongTaskInstrumentation();
  EXPECT_TRUE(ObservingLongTasks());

  // Simulate navigation commit.
  DocumentInit init = DocumentInit::Create().WithFrame(GetFrame());
  GetDocument()->Shutdown();
  GetFrame()->SetDOMWindow(LocalDOMWindow::Create(*GetFrame()));
  GetFrame()->DomWindow()->InstallNewDocument(AtomicString(), init, false);

  // m_performance is still alive, and should not crash when notified.
  SimulateDidProcessLongTask();
}

// Checks that WindowPerformance object and its fields (like PerformanceTiming)
// function correctly after transition to another document in the same window.
// This happens when a page opens a new window and it navigates to a same-origin
// document.
TEST(PerformanceLifetimeTest, SurviveContextSwitch) {
  std::unique_ptr<DummyPageHolder> page_holder =
      DummyPageHolder::Create(IntSize(800, 600));

  WindowPerformance* perf =
      DOMWindowPerformance::performance(*page_holder->GetFrame().DomWindow());
  PerformanceTiming* timing = perf->timing();

  auto* document_loader = page_holder->GetFrame().Loader().GetDocumentLoader();
  ASSERT_TRUE(document_loader);
  document_loader->GetTiming().SetNavigationStart(CurrentTimeTicks());

  EXPECT_EQ(&page_holder->GetFrame(), perf->GetFrame());
  EXPECT_EQ(&page_holder->GetFrame(), timing->GetFrame());
  auto navigation_start = timing->navigationStart();
  EXPECT_NE(0U, navigation_start);

  // Simulate changing the document while keeping the window.
  page_holder->GetDocument().Shutdown();
  page_holder->GetFrame().DomWindow()->InstallNewDocument(
      AtomicString(),
      DocumentInit::Create().WithFrame(&page_holder->GetFrame()), false);

  EXPECT_EQ(perf, DOMWindowPerformance::performance(
                      *page_holder->GetFrame().DomWindow()));
  EXPECT_EQ(timing, perf->timing());
  EXPECT_EQ(&page_holder->GetFrame(), perf->GetFrame());
  EXPECT_EQ(&page_holder->GetFrame(), timing->GetFrame());
  EXPECT_EQ(navigation_start, timing->navigationStart());
}

// Make sure the output entries with the same timestamps follow the insertion
// order. (http://crbug.com/767560)
TEST_F(WindowPerformanceTest, EnsureEntryListOrder) {
  V8TestingScope scope;
  FakeTimer timer(kTimeOrigin);

  DummyExceptionStateForTesting exception_state;
  timer.AdvanceTimer(2);
  for (int i = 0; i < 8; i++) {
    performance_->mark(scope.GetScriptState(), String::Number(i),
                       exception_state);
  }
  timer.AdvanceTimer(2);
  for (int i = 8; i < 17; i++) {
    performance_->mark(scope.GetScriptState(), String::Number(i),
                       exception_state);
  }
  PerformanceEntryVector entries = performance_->getEntries();
  EXPECT_EQ(17U, entries.size());
  for (int i = 0; i < 8; i++) {
    EXPECT_EQ(String::Number(i), entries[i]->name());
    EXPECT_NEAR(2000, entries[i]->startTime(), 0.005);
  }
  for (int i = 8; i < 17; i++) {
    EXPECT_EQ(String::Number(i), entries[i]->name());
    EXPECT_NEAR(4000, entries[i]->startTime(), 0.005);
  }
}

TEST_F(WindowPerformanceTest, ParameterHistogramForMeasure) {
  HistogramTester histogram_tester;
  DummyExceptionStateForTesting exception_state;

  histogram_tester.ExpectTotalCount(kStartMarkForMeasureHistogram, 0);
  histogram_tester.ExpectTotalCount(kEndMarkForMeasureHistogram, 0);

  performance_->measure("testMark", "unloadEventStart", "unloadEventEnd",
                        exception_state);

  histogram_tester.ExpectBucketCount(
      kStartMarkForMeasureHistogram,
      static_cast<int>(Performance::kUnloadEventStart), 1);
  histogram_tester.ExpectBucketCount(
      kEndMarkForMeasureHistogram,
      static_cast<int>(Performance::kUnloadEventEnd), 1);

  performance_->measure("testMark", "domInteractive", "[object Object]",
                        exception_state);

  histogram_tester.ExpectBucketCount(
      kStartMarkForMeasureHistogram,
      static_cast<int>(Performance::kDomInteractive), 1);
  histogram_tester.ExpectBucketCount(
      kEndMarkForMeasureHistogram, static_cast<int>(Performance::kObjectObject),
      1);

  performance_->measure("testMark", "[object Object]", "[object Object]",
                        exception_state);

  histogram_tester.ExpectBucketCount(
      kStartMarkForMeasureHistogram,
      static_cast<int>(Performance::kObjectObject), 1);
  histogram_tester.ExpectBucketCount(
      kEndMarkForMeasureHistogram, static_cast<int>(Performance::kObjectObject),
      2);
}

}  // namespace blink
