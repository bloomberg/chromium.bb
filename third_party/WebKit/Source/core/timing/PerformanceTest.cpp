// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/Performance.h"

#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class PerformanceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    m_pageHolder = DummyPageHolder::create(IntSize(800, 600));
    m_pageHolder->document().setURL(KURL(KURL(), "https://example.com"));
    m_performance = Performance::create(&m_pageHolder->frame());
  }

  bool observingLongTasks() { return m_performance->observingLongTasks(); }

  void addLongTaskObserver() {
    // simulate with filter options.
    m_performance->m_observerFilterOptions |= PerformanceEntry::LongTask;
  }

  void removeLongTaskObserver() {
    // simulate with filter options.
    m_performance->m_observerFilterOptions = PerformanceEntry::Invalid;
  }

  Persistent<Performance> m_performance;
  std::unique_ptr<DummyPageHolder> m_pageHolder;
};

TEST_F(PerformanceTest, LongTaskObserverInstrumentation) {
  m_performance->updateLongTaskInstrumentation();
  EXPECT_FALSE(observingLongTasks());

  // Adding LongTask observer (with filer option) enables instrumentation.
  addLongTaskObserver();
  m_performance->updateLongTaskInstrumentation();
  EXPECT_TRUE(observingLongTasks());

  // Removing LongTask observer disables instrumentation.
  removeLongTaskObserver();
  m_performance->updateLongTaskInstrumentation();
  EXPECT_FALSE(observingLongTasks());
}
}
