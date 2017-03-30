// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/PerformanceNavigationTiming.h"

#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class PerformanceNavigationTimingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    m_pageHolder = DummyPageHolder::create(IntSize(800, 600));
  }

  AtomicString getNavigationType(NavigationType type, Document* document) {
    return PerformanceNavigationTiming::getNavigationType(type, document);
  }

  std::unique_ptr<DummyPageHolder> m_pageHolder;
};

TEST_F(PerformanceNavigationTimingTest, GetNavigationType) {
  m_pageHolder->page().setVisibilityState(PageVisibilityStatePrerender, false);
  AtomicString returnedType =
      getNavigationType(NavigationTypeBackForward, &m_pageHolder->document());
  EXPECT_EQ(returnedType, "prerender");

  m_pageHolder->page().setVisibilityState(PageVisibilityStateHidden, false);
  returnedType =
      getNavigationType(NavigationTypeBackForward, &m_pageHolder->document());
  EXPECT_EQ(returnedType, "back_forward");

  m_pageHolder->page().setVisibilityState(PageVisibilityStateVisible, false);
  returnedType = getNavigationType(NavigationTypeFormResubmitted,
                                   &m_pageHolder->document());
  EXPECT_EQ(returnedType, "navigate");
}
}  // namespace blink
