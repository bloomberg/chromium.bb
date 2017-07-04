// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/DocumentLoadTiming.h"

#include "core/loader/DocumentLoader.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class DocumentLoadTimingTest : public ::testing::Test {};

TEST_F(DocumentLoadTimingTest, ensureValidNavigationStartAfterEmbedder) {
  std::unique_ptr<DummyPageHolder> dummy_page = DummyPageHolder::Create();
  DocumentLoadTiming timing(*(dummy_page->GetDocument().Loader()));

  double delta = -1000;
  double embedder_navigation_start = MonotonicallyIncreasingTime() + delta;
  timing.SetNavigationStart(embedder_navigation_start);

  double real_wall_time = CurrentTime();
  double adjusted_wall_time =
      timing.MonotonicTimeToPseudoWallTime(timing.NavigationStart());

  EXPECT_NEAR(adjusted_wall_time, real_wall_time + delta, .001);
}

TEST_F(DocumentLoadTimingTest, correctTimingDeltas) {
  std::unique_ptr<DummyPageHolder> dummy_page = DummyPageHolder::Create();
  DocumentLoadTiming timing(*(dummy_page->GetDocument().Loader()));

  double navigation_start_delta = -456;
  double current_monotonic_time = MonotonicallyIncreasingTime();
  double embedder_navigation_start =
      current_monotonic_time + navigation_start_delta;

  timing.SetNavigationStart(embedder_navigation_start);

  // Super quick load! Expect the wall time reported by this event to be
  // dominated by the navigationStartDelta, but similar to currentTime().
  timing.MarkLoadEventEnd();
  double real_wall_load_event_end = CurrentTime();
  double adjusted_load_event_end =
      timing.MonotonicTimeToPseudoWallTime(timing.LoadEventEnd());

  EXPECT_NEAR(adjusted_load_event_end, real_wall_load_event_end, .001);

  double adjusted_navigation_start =
      timing.MonotonicTimeToPseudoWallTime(timing.NavigationStart());
  EXPECT_NEAR(adjusted_load_event_end - adjusted_navigation_start,
              -navigation_start_delta, .001);
}

}  // namespace blink
