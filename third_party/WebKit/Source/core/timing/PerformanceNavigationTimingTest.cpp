// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/PerformanceNavigationTiming.h"

#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/common/page/page_visibility_state.mojom-blink.h"

namespace blink {

class PerformanceNavigationTimingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
  }

  AtomicString GetNavigationType(NavigationType type, Document* document) {
    return PerformanceNavigationTiming::GetNavigationType(type, document);
  }

  std::unique_ptr<DummyPageHolder> page_holder_;
};

TEST_F(PerformanceNavigationTimingTest, GetNavigationType) {
  page_holder_->GetPage().SetVisibilityState(
      mojom::PageVisibilityState::kPrerender, false);
  AtomicString returned_type = GetNavigationType(kNavigationTypeBackForward,
                                                 &page_holder_->GetDocument());
  EXPECT_EQ(returned_type, "prerender");

  page_holder_->GetPage().SetVisibilityState(
      mojom::PageVisibilityState::kHidden, false);
  returned_type = GetNavigationType(kNavigationTypeBackForward,
                                    &page_holder_->GetDocument());
  EXPECT_EQ(returned_type, "back_forward");

  page_holder_->GetPage().SetVisibilityState(
      mojom::PageVisibilityState::kVisible, false);
  returned_type = GetNavigationType(kNavigationTypeFormResubmitted,
                                    &page_holder_->GetDocument());
  EXPECT_EQ(returned_type, "navigate");
}
}  // namespace blink
