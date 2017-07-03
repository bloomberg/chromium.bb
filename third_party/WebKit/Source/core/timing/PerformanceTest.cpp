// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/Performance.h"

#include "core/frame/PerformanceMonitor.h"
#include "core/loader/DocumentLoadTiming.h"
#include "core/loader/DocumentLoader.h"
#include "core/testing/DummyPageHolder.h"
#include "core/timing/DOMWindowPerformance.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class PerformanceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
    page_holder_->GetDocument().SetURL(KURL(NullURL(), "https://example.com"));
    performance_ = Performance::Create(&page_holder_->GetFrame());

    // Create another dummy page holder and pretend this is the iframe.
    another_page_holder_ = DummyPageHolder::Create(IntSize(400, 300));
    another_page_holder_->GetDocument().SetURL(
        KURL(NullURL(), "https://iframed.com/bar"));
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
    monitor->DidProcessTask(nullptr, 0, 1);
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
    return Performance::SanitizedAttribution(context, has_multiple_contexts,
                                             observer_frame)
        .first;
  }

  Persistent<Performance> performance_;
  std::unique_ptr<DummyPageHolder> page_holder_;
  std::unique_ptr<DummyPageHolder> another_page_holder_;
};

TEST_F(PerformanceTest, LongTaskObserverInstrumentation) {
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

TEST_F(PerformanceTest, SanitizedLongTaskName) {
  // Unable to attribute, when no execution contents are available.
  EXPECT_EQ("unknown", SanitizedAttribution(nullptr, false, GetFrame()));

  // Attribute for same context (and same origin).
  EXPECT_EQ("self", SanitizedAttribution(GetDocument(), false, GetFrame()));

  // Unable to attribute, when multiple script execution contents are involved.
  EXPECT_EQ("multiple-contexts",
            SanitizedAttribution(GetDocument(), true, GetFrame()));
}

TEST_F(PerformanceTest, SanitizedLongTaskName_CrossOrigin) {
  // Unable to attribute, when no execution contents are available.
  EXPECT_EQ("unknown", SanitizedAttribution(nullptr, false, GetFrame()));

  // Attribute for same context (and same origin).
  EXPECT_EQ("cross-origin-unreachable",
            SanitizedAttribution(AnotherDocument(), false, GetFrame()));
}

// https://crbug.com/706798: Checks that after navigation that have replaced the
// window object, calls to not garbage collected yet Performance belonging to
// the old window do not cause a crash.
TEST_F(PerformanceTest, NavigateAway) {
  AddLongTaskObserver();
  performance_->UpdateLongTaskInstrumentation();
  EXPECT_TRUE(ObservingLongTasks());

  // Simulate navigation commit.
  DocumentInit init(NullURL(), GetFrame());
  GetDocument()->Shutdown();
  GetFrame()->SetDOMWindow(LocalDOMWindow::Create(*GetFrame()));
  GetFrame()->DomWindow()->InstallNewDocument(AtomicString(), init);

  // m_performance is still alive, and should not crash when notified.
  SimulateDidProcessLongTask();
}

// Checks that Performance object and its fields (like PerformanceTiming)
// function correctly after transition to another document in the same window.
// This happens when a page opens a new window and it navigates to a same-origin
// document.
TEST(PerformanceLifetimeTest, SurviveContextSwitch) {
  std::unique_ptr<DummyPageHolder> page_holder =
      DummyPageHolder::Create(IntSize(800, 600));

  Performance* perf =
      DOMWindowPerformance::performance(*page_holder->GetFrame().DomWindow());
  PerformanceTiming* timing = perf->timing();

  auto* document_loader = page_holder->GetFrame().Loader().GetDocumentLoader();
  ASSERT_TRUE(document_loader);
  document_loader->GetTiming().SetNavigationStart(
      MonotonicallyIncreasingTime());

  EXPECT_EQ(&page_holder->GetFrame(), perf->GetFrame());
  EXPECT_EQ(&page_holder->GetFrame(), timing->GetFrame());
  auto navigation_start = timing->navigationStart();
  EXPECT_NE(0U, navigation_start);

  // Simulate changing the document while keeping the window.
  page_holder->GetDocument().Shutdown();
  page_holder->GetFrame().DomWindow()->InstallNewDocument(
      AtomicString(), DocumentInit(NullURL(), &page_holder->GetFrame()));

  EXPECT_EQ(perf, DOMWindowPerformance::performance(
                      *page_holder->GetFrame().DomWindow()));
  EXPECT_EQ(timing, perf->timing());
  EXPECT_EQ(&page_holder->GetFrame(), perf->GetFrame());
  EXPECT_EQ(&page_holder->GetFrame(), timing->GetFrame());
  EXPECT_EQ(navigation_start, timing->navigationStart());
}
}
