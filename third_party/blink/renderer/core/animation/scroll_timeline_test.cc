// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/scroll_timeline.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

using ScrollTimelineTest = RenderingTest;

TEST_F(ScrollTimelineTest,
       AttachingAndDetachingAnimationCausesCompositingUpdate) {
  EnableCompositing();

  SetBodyInnerHTML(R"HTML(
    <style>#scroller { overflow: scroll; width: 100px; height: 100px; }</style>
    <div id='scroller'></div>
  )HTML");

  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  ASSERT_TRUE(scroller);

  // Invariant: the scroller is not composited by default.
  EXPECT_EQ(DocumentLifecycle::kPaintClean,
            GetDocument().Lifecycle().GetState());
  EXPECT_EQ(kNotComposited, scroller->Layer()->GetCompositingState());

  // Create the ScrollTimeline. This shouldn't cause the scrollSource to need
  // compositing, as it isn't attached to any animation yet.
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(GetElementById("scroller"));
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(DocumentLifecycle::kPaintClean,
            GetDocument().Lifecycle().GetState());
  EXPECT_EQ(kNotComposited, scroller->Layer()->GetCompositingState());

  // Now attach an animation. This should require a compositing update.
  scroll_timeline->AttachAnimation();

  UpdateAllLifecyclePhasesForTest();
  EXPECT_NE(scroller->Layer()->GetCompositingState(), kNotComposited);

  // Now detach an animation. This should again require a compositing update.
  scroll_timeline->DetachAnimation();

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(scroller->Layer()->GetCompositingState(), kNotComposited);
}

TEST_F(ScrollTimelineTest, CurrentTimeIsNullIfScrollSourceIsNotScrollable) {
  SetBodyInnerHTML(R"HTML(
    <style>#scroller { width: 100px; height: 100px; }</style>
    <div id='scroller'></div>
  )HTML");

  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  ASSERT_TRUE(scroller);

  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(GetElementById("scroller"));
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  bool current_time_is_null = false;
  scroll_timeline->currentTime(current_time_is_null);
  EXPECT_TRUE(current_time_is_null);
}

TEST_F(ScrollTimelineTest,
       CurrentTimeIsNullIfScrollOffsetIsBeyondStartAndEndScrollOffset) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; width: 100px; height: 100px; }
      #spacer { height: 1000px; }
    </style>
    <div id='scroller'>
      <div id ='spacer'></div>
    </div>
  )HTML");

  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  ASSERT_TRUE(scroller);
  ASSERT_TRUE(scroller->HasOverflowClip());
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(GetElementById("scroller"));
  options->setStartScrollOffset("10px");
  options->setEndScrollOffset("90px");
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  bool current_time_is_null = false;
  scrollable_area->SetScrollOffset(ScrollOffset(0, 5), kProgrammaticScroll);
  scroll_timeline->currentTime(current_time_is_null);
  EXPECT_TRUE(current_time_is_null);

  current_time_is_null = true;
  scrollable_area->SetScrollOffset(ScrollOffset(0, 50), kProgrammaticScroll);
  scroll_timeline->currentTime(current_time_is_null);
  EXPECT_FALSE(current_time_is_null);

  current_time_is_null = false;
  scrollable_area->SetScrollOffset(ScrollOffset(0, 100), kProgrammaticScroll);
  scroll_timeline->currentTime(current_time_is_null);
  EXPECT_TRUE(current_time_is_null);
}

TEST_F(ScrollTimelineTest,
       CurrentTimeIsNullIfEndScrollOffsetIsLessThanStartScrollOffset) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; width: 100px; height: 100px; }
      #spacer { height: 1000px; }
    </style>
    <div id='scroller'>
      <div id ='spacer'></div>
    </div>
  )HTML");

  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  ASSERT_TRUE(scroller);
  ASSERT_TRUE(scroller->HasOverflowClip());
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(GetElementById("scroller"));
  options->setStartScrollOffset("80px");
  options->setEndScrollOffset("40px");
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  bool current_time_is_null = false;
  scrollable_area->SetScrollOffset(ScrollOffset(0, 50), kProgrammaticScroll);
  scroll_timeline->currentTime(current_time_is_null);
  EXPECT_TRUE(current_time_is_null);
}
}  //  namespace blink
