// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintController.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/ClipPathDisplayItem.h"
#include "platform/graphics/paint/ClipPathRecorder.h"
#include "platform/graphics/paint/ClipRecorder.h"
#include "platform/graphics/paint/CompositingRecorder.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/SubsequenceRecorder.h"
#include "platform/testing/FakeDisplayItemClient.h"
#include "platform/testing/PaintPropertyTestHelpers.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

using blink::testing::CreateOpacityOnlyEffect;
using blink::testing::DefaultPaintChunkProperties;
using testing::UnorderedElementsAre;

namespace blink {

class PaintControllerTestBase : public ::testing::Test {
 public:
  PaintControllerTestBase() : paint_controller_(PaintController::Create()) {}

  IntRect VisualRect(const PaintArtifact& paint_artifact, size_t index) {
    return paint_artifact.GetDisplayItemList().VisualRect(index);
  }

 protected:
  PaintController& GetPaintController() { return *paint_controller_; }

  int NumCachedNewItems() const {
    return paint_controller_->num_cached_new_items_;
  }

#ifndef NDEBUG
  int NumSequentialMatches() const {
    return paint_controller_->num_sequential_matches_;
  }
  int NumOutOfOrderMatches() const {
    return paint_controller_->num_out_of_order_matches_;
  }
  int NumIndexedItems() const { return paint_controller_->num_indexed_items_; }
#endif

 private:
  std::unique_ptr<PaintController> paint_controller_;
};

const DisplayItem::Type kForegroundDrawingType =
    static_cast<DisplayItem::Type>(DisplayItem::kDrawingPaintPhaseFirst + 4);
const DisplayItem::Type kBackgroundDrawingType =
    DisplayItem::kDrawingPaintPhaseFirst;
const DisplayItem::Type kClipType = DisplayItem::kClipFirst;

class TestDisplayItem final : public DisplayItem {
 public:
  TestDisplayItem(const FakeDisplayItemClient& client, Type type)
      : DisplayItem(client, type, sizeof(*this)) {}

  void Replay(GraphicsContext&) const final { NOTREACHED(); }
  void AppendToWebDisplayItemList(const IntRect&,
                                  WebDisplayItemList*) const final {
    NOTREACHED();
  }
};

#ifndef NDEBUG
#define TRACE_DISPLAY_ITEMS(i, expected, actual)             \
  String trace = String::Format("%d: ", (int)i) +            \
                 "Expected: " + (expected).AsDebugString() + \
                 " Actual: " + (actual).AsDebugString();     \
  SCOPED_TRACE(trace.Utf8().Data());
#else
#define TRACE_DISPLAY_ITEMS(i, expected, actual)
#endif

#define EXPECT_DISPLAY_LIST(actual, expectedSize, ...)                     \
  do {                                                                     \
    EXPECT_EQ((size_t)expectedSize, actual.size());                        \
    if (expectedSize != actual.size())                                     \
      break;                                                               \
    const TestDisplayItem expected[] = {__VA_ARGS__};                      \
    for (size_t index = 0;                                                 \
         index < std::min<size_t>(actual.size(), expectedSize); index++) { \
      TRACE_DISPLAY_ITEMS(index, expected[index], actual[index]);          \
      EXPECT_EQ(expected[index].Client(), actual[index].Client());         \
      EXPECT_EQ(expected[index].GetType(), actual[index].GetType());       \
    }                                                                      \
  } while (false);

void DrawRect(GraphicsContext& context,
              const FakeDisplayItemClient& client,
              DisplayItem::Type type,
              const FloatRect& bounds) {
  if (DrawingRecorder::UseCachedDrawingIfPossible(context, client, type))
    return;
  DrawingRecorder drawing_recorder(context, client, type, bounds);
  IntRect rect(0, 0, 10, 10);
  context.DrawRect(rect);
}

void DrawClippedRect(GraphicsContext& context,
                     const FakeDisplayItemClient& client,
                     DisplayItem::Type clip_type,
                     DisplayItem::Type drawing_type,
                     const FloatRect& bound) {
  ClipRecorder clip_recorder(context, client, clip_type, IntRect(1, 1, 9, 9));
  DrawRect(context, client, drawing_type, bound);
}

enum TestConfigurations {
  kSPv2 = 1 << 0,
  kUnderInvalidationChecking = 1 << 1,
};

// Tests using this class will be tested with under-invalidation-checking
// enabled and disabled.
class PaintControllerTest
    : public PaintControllerTestBase,
      public ::testing::WithParamInterface<TestConfigurations>,
      private ScopedSlimmingPaintV2ForTest,
      private ScopedPaintUnderInvalidationCheckingForTest {
 public:
  PaintControllerTest()
      : ScopedSlimmingPaintV2ForTest(GetParam() & kSPv2),
        ScopedPaintUnderInvalidationCheckingForTest(GetParam() &
                                                    kUnderInvalidationChecking),
        root_paint_property_client_("root"),
        root_paint_chunk_id_(root_paint_property_client_,
                             DisplayItem::kUninitializedType) {}

  FakeDisplayItemClient root_paint_property_client_;
  PaintChunk::Id root_paint_chunk_id_;
};

INSTANTIATE_TEST_CASE_P(All,
                        PaintControllerTest,
                        ::testing::Values(kSPv2, kUnderInvalidationChecking));

TEST_P(PaintControllerTest, NestedRecorders) {
  GraphicsContext context(GetPaintController());
  FakeDisplayItemClient client("client", LayoutRect(100, 100, 200, 200));
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  DrawClippedRect(context, client, kClipType, kBackgroundDrawingType,
                  FloatRect(100, 100, 200, 200));
  GetPaintController().CommitNewDisplayItems();

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 1,
                        TestDisplayItem(client, kBackgroundDrawingType));

    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                UnorderedElementsAre(FloatRect(LayoutRect::InfiniteIntRect())));
  } else {
    EXPECT_DISPLAY_LIST(
        GetPaintController().GetDisplayItemList(), 3,
        TestDisplayItem(client, kClipType),
        TestDisplayItem(client, kBackgroundDrawingType),
        TestDisplayItem(client, DisplayItem::ClipTypeToEndClipType(kClipType)));
  }
}

TEST_P(PaintControllerTest, UpdateBasic) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 300, 300));
  FakeDisplayItemClient second("second", LayoutRect(100, 100, 200, 200));
  GraphicsContext context(GetPaintController());
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  DrawRect(context, first, kBackgroundDrawingType,
           FloatRect(100, 100, 300, 300));
  DrawRect(context, second, kBackgroundDrawingType,
           FloatRect(100, 100, 200, 200));
  DrawRect(context, first, kForegroundDrawingType,
           FloatRect(100, 100, 300, 300));

  EXPECT_EQ(0, NumCachedNewItems());

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 3,
                      TestDisplayItem(first, kBackgroundDrawingType),
                      TestDisplayItem(second, kBackgroundDrawingType),
                      TestDisplayItem(first, kForegroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                UnorderedElementsAre(FloatRect(LayoutRect::InfiniteIntRect())));

    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  DrawRect(context, first, kBackgroundDrawingType,
           FloatRect(100, 100, 300, 300));
  DrawRect(context, first, kForegroundDrawingType,
           FloatRect(100, 100, 300, 300));

  EXPECT_EQ(2, NumCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(2, NumSequentialMatches());
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(1, NumIndexedItems());
#endif

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(first, kBackgroundDrawingType),
                      TestDisplayItem(first, kForegroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(
        GetPaintController().PaintChunks()[0].raster_invalidation_rects,
        UnorderedElementsAre(FloatRect(
            100, 100, 200, 200)));  // |second| disappeared from the chunk.
  }
}

TEST_P(PaintControllerTest, UpdateSwapOrder) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient second("second", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient unaffected("unaffected", LayoutRect(300, 300, 10, 10));
  GraphicsContext context(GetPaintController());
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  DrawRect(context, first, kBackgroundDrawingType,
           FloatRect(100, 100, 100, 100));
  DrawRect(context, first, kForegroundDrawingType,
           FloatRect(100, 100, 100, 100));
  DrawRect(context, second, kBackgroundDrawingType,
           FloatRect(100, 100, 50, 200));
  DrawRect(context, second, kForegroundDrawingType,
           FloatRect(100, 100, 50, 200));
  DrawRect(context, unaffected, kBackgroundDrawingType,
           FloatRect(300, 300, 10, 10));
  DrawRect(context, unaffected, kForegroundDrawingType,
           FloatRect(300, 300, 10, 10));
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 6,
                      TestDisplayItem(first, kBackgroundDrawingType),
                      TestDisplayItem(first, kForegroundDrawingType),
                      TestDisplayItem(second, kBackgroundDrawingType),
                      TestDisplayItem(second, kForegroundDrawingType),
                      TestDisplayItem(unaffected, kBackgroundDrawingType),
                      TestDisplayItem(unaffected, kForegroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }
  DrawRect(context, second, kBackgroundDrawingType,
           FloatRect(100, 100, 50, 200));
  DrawRect(context, second, kForegroundDrawingType,
           FloatRect(100, 100, 50, 200));
  DrawRect(context, first, kBackgroundDrawingType,
           FloatRect(100, 100, 100, 100));
  DrawRect(context, first, kForegroundDrawingType,
           FloatRect(100, 100, 100, 100));
  DrawRect(context, unaffected, kBackgroundDrawingType,
           FloatRect(300, 300, 10, 10));
  DrawRect(context, unaffected, kForegroundDrawingType,
           FloatRect(300, 300, 10, 10));

  EXPECT_EQ(6, NumCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(5, NumSequentialMatches());  // second, first foreground, unaffected
  EXPECT_EQ(1, NumOutOfOrderMatches());  // first
  EXPECT_EQ(2, NumIndexedItems());       // first
#endif

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 6,
                      TestDisplayItem(second, kBackgroundDrawingType),
                      TestDisplayItem(second, kForegroundDrawingType),
                      TestDisplayItem(first, kBackgroundDrawingType),
                      TestDisplayItem(first, kForegroundDrawingType),
                      TestDisplayItem(unaffected, kBackgroundDrawingType),
                      TestDisplayItem(unaffected, kForegroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                UnorderedElementsAre(
                    FloatRect(100, 100, 50, 200)));  // Bounds of |second|.
  }
}

TEST_P(PaintControllerTest, UpdateSwapOrderWithInvalidation) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient second("second", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient unaffected("unaffected", LayoutRect(300, 300, 10, 10));
  GraphicsContext context(GetPaintController());
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  DrawRect(context, first, kBackgroundDrawingType,
           FloatRect(100, 100, 100, 100));
  DrawRect(context, first, kForegroundDrawingType,
           FloatRect(100, 100, 100, 100));
  DrawRect(context, second, kBackgroundDrawingType,
           FloatRect(100, 100, 50, 200));
  DrawRect(context, second, kForegroundDrawingType,
           FloatRect(100, 100, 50, 200));
  DrawRect(context, unaffected, kBackgroundDrawingType,
           FloatRect(300, 300, 10, 10));
  DrawRect(context, unaffected, kForegroundDrawingType,
           FloatRect(300, 300, 10, 10));
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 6,
                      TestDisplayItem(first, kBackgroundDrawingType),
                      TestDisplayItem(first, kForegroundDrawingType),
                      TestDisplayItem(second, kBackgroundDrawingType),
                      TestDisplayItem(second, kForegroundDrawingType),
                      TestDisplayItem(unaffected, kBackgroundDrawingType),
                      TestDisplayItem(unaffected, kForegroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  first.SetDisplayItemsUncached();
  DrawRect(context, second, kBackgroundDrawingType,
           FloatRect(100, 100, 50, 200));
  DrawRect(context, second, kForegroundDrawingType,
           FloatRect(100, 100, 50, 200));
  DrawRect(context, first, kBackgroundDrawingType,
           FloatRect(100, 100, 100, 100));
  DrawRect(context, first, kForegroundDrawingType,
           FloatRect(100, 100, 100, 100));
  DrawRect(context, unaffected, kBackgroundDrawingType,
           FloatRect(300, 300, 10, 10));
  DrawRect(context, unaffected, kForegroundDrawingType,
           FloatRect(300, 300, 10, 10));

  EXPECT_EQ(4, NumCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(4, NumSequentialMatches());  // second, unaffected
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(2, NumIndexedItems());
#endif

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 6,
                      TestDisplayItem(second, kBackgroundDrawingType),
                      TestDisplayItem(second, kForegroundDrawingType),
                      TestDisplayItem(first, kBackgroundDrawingType),
                      TestDisplayItem(first, kForegroundDrawingType),
                      TestDisplayItem(unaffected, kBackgroundDrawingType),
                      TestDisplayItem(unaffected, kForegroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                UnorderedElementsAre(
                    FloatRect(100, 100, 100, 100),    // Old bounds of |first|.
                    FloatRect(100, 100, 100, 100)));  // New bounds of |first|.
    // No need to invalidate raster of |second|, because the client (|first|)
    // which swapped order with it has been invalidated.
  }
}

TEST_P(PaintControllerTest, UpdateNewItemInMiddle) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient second("second", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient third("third", LayoutRect(125, 100, 200, 50));
  GraphicsContext context(GetPaintController());
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  DrawRect(context, first, kBackgroundDrawingType,
           FloatRect(100, 100, 100, 100));
  DrawRect(context, second, kBackgroundDrawingType,
           FloatRect(100, 100, 50, 200));
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(first, kBackgroundDrawingType),
                      TestDisplayItem(second, kBackgroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  DrawRect(context, first, kBackgroundDrawingType,
           FloatRect(100, 100, 100, 100));
  DrawRect(context, third, kBackgroundDrawingType,
           FloatRect(125, 100, 200, 50));
  DrawRect(context, second, kBackgroundDrawingType,
           FloatRect(100, 100, 50, 200));

  EXPECT_EQ(2, NumCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(2, NumSequentialMatches());  // first, second
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(0, NumIndexedItems());
#endif

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 3,
                      TestDisplayItem(first, kBackgroundDrawingType),
                      TestDisplayItem(third, kBackgroundDrawingType),
                      TestDisplayItem(second, kBackgroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(
        GetPaintController().PaintChunks()[0].raster_invalidation_rects,
        UnorderedElementsAre(FloatRect(
            125, 100, 200, 50)));  // |third| newly appeared in the chunk.
  }
}

TEST_P(PaintControllerTest, UpdateInvalidationWithPhases) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient second("second", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient third("third", LayoutRect(300, 100, 50, 50));
  GraphicsContext context(GetPaintController());
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  DrawRect(context, first, kBackgroundDrawingType,
           FloatRect(100, 100, 100, 100));
  DrawRect(context, second, kBackgroundDrawingType,
           FloatRect(100, 100, 50, 200));
  DrawRect(context, third, kBackgroundDrawingType, FloatRect(300, 100, 50, 50));
  DrawRect(context, first, kForegroundDrawingType,
           FloatRect(100, 100, 100, 100));
  DrawRect(context, second, kForegroundDrawingType,
           FloatRect(100, 100, 50, 200));
  DrawRect(context, third, kForegroundDrawingType, FloatRect(300, 100, 50, 50));
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 6,
                      TestDisplayItem(first, kBackgroundDrawingType),
                      TestDisplayItem(second, kBackgroundDrawingType),
                      TestDisplayItem(third, kBackgroundDrawingType),
                      TestDisplayItem(first, kForegroundDrawingType),
                      TestDisplayItem(second, kForegroundDrawingType),
                      TestDisplayItem(third, kForegroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  second.SetDisplayItemsUncached();
  DrawRect(context, first, kBackgroundDrawingType,
           FloatRect(100, 100, 100, 100));
  DrawRect(context, second, kBackgroundDrawingType,
           FloatRect(100, 100, 50, 200));
  DrawRect(context, third, kBackgroundDrawingType, FloatRect(300, 100, 50, 50));
  DrawRect(context, first, kForegroundDrawingType,
           FloatRect(100, 100, 100, 100));
  DrawRect(context, second, kForegroundDrawingType,
           FloatRect(100, 100, 50, 200));
  DrawRect(context, third, kForegroundDrawingType, FloatRect(300, 100, 50, 50));

  EXPECT_EQ(4, NumCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(4, NumSequentialMatches());
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(2, NumIndexedItems());
#endif

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 6,
                      TestDisplayItem(first, kBackgroundDrawingType),
                      TestDisplayItem(second, kBackgroundDrawingType),
                      TestDisplayItem(third, kBackgroundDrawingType),
                      TestDisplayItem(first, kForegroundDrawingType),
                      TestDisplayItem(second, kForegroundDrawingType),
                      TestDisplayItem(third, kForegroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                UnorderedElementsAre(
                    FloatRect(100, 100, 50, 200),    // Old bounds of |second|.
                    FloatRect(100, 100, 50, 200)));  // New bounds of |second|.
  }
}

TEST_P(PaintControllerTest, UpdateAddFirstOverlap) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 150, 150));
  FakeDisplayItemClient second("second", LayoutRect(200, 200, 50, 50));
  GraphicsContext context(GetPaintController());
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  DrawRect(context, second, kBackgroundDrawingType,
           FloatRect(200, 200, 50, 50));
  DrawRect(context, second, kForegroundDrawingType,
           FloatRect(200, 200, 50, 50));
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(second, kBackgroundDrawingType),
                      TestDisplayItem(second, kForegroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  first.SetDisplayItemsUncached();
  second.SetDisplayItemsUncached();
  second.SetVisualRect(LayoutRect(150, 150, 100, 100));
  DrawRect(context, first, kBackgroundDrawingType,
           FloatRect(100, 100, 150, 150));
  DrawRect(context, first, kForegroundDrawingType,
           FloatRect(100, 100, 150, 150));
  DrawRect(context, second, kBackgroundDrawingType,
           FloatRect(150, 150, 100, 100));
  DrawRect(context, second, kForegroundDrawingType,
           FloatRect(150, 150, 100, 100));
  EXPECT_EQ(0, NumCachedNewItems());
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 4,
                      TestDisplayItem(first, kBackgroundDrawingType),
                      TestDisplayItem(first, kForegroundDrawingType),
                      TestDisplayItem(second, kBackgroundDrawingType),
                      TestDisplayItem(second, kForegroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                UnorderedElementsAre(
                    FloatRect(100, 100, 150,
                              150),  // |first| newly appeared in the chunk.
                    FloatRect(200, 200, 50, 50),      // Old bounds of |second|.
                    FloatRect(150, 150, 100, 100)));  // New bounds of |second|.

    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  DrawRect(context, second, kBackgroundDrawingType,
           FloatRect(150, 150, 100, 100));
  DrawRect(context, second, kForegroundDrawingType,
           FloatRect(150, 150, 100, 100));

  EXPECT_EQ(2, NumCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(2, NumSequentialMatches());
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(2, NumIndexedItems());
#endif

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(second, kBackgroundDrawingType),
                      TestDisplayItem(second, kForegroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(
        GetPaintController().PaintChunks()[0].raster_invalidation_rects,
        UnorderedElementsAre(FloatRect(
            100, 100, 150, 150)));  // |first| disappeared from the chunk.
  }
}

TEST_P(PaintControllerTest, UpdateAddLastOverlap) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 150, 150));
  FakeDisplayItemClient second("second", LayoutRect(200, 200, 50, 50));
  GraphicsContext context(GetPaintController());
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  DrawRect(context, first, kBackgroundDrawingType,
           FloatRect(100, 100, 150, 150));
  DrawRect(context, first, kForegroundDrawingType,
           FloatRect(100, 100, 150, 150));
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(first, kBackgroundDrawingType),
                      TestDisplayItem(first, kForegroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  first.SetDisplayItemsUncached();
  first.SetVisualRect(LayoutRect(150, 150, 100, 100));
  second.SetDisplayItemsUncached();
  DrawRect(context, first, kBackgroundDrawingType,
           FloatRect(150, 150, 100, 100));
  DrawRect(context, first, kForegroundDrawingType,
           FloatRect(150, 150, 100, 100));
  DrawRect(context, second, kBackgroundDrawingType,
           FloatRect(200, 200, 50, 50));
  DrawRect(context, second, kForegroundDrawingType,
           FloatRect(200, 200, 50, 50));
  EXPECT_EQ(0, NumCachedNewItems());
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 4,
                      TestDisplayItem(first, kBackgroundDrawingType),
                      TestDisplayItem(first, kForegroundDrawingType),
                      TestDisplayItem(second, kBackgroundDrawingType),
                      TestDisplayItem(second, kForegroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                UnorderedElementsAre(
                    FloatRect(100, 100, 150, 150),  // Old bounds of |first|.
                    FloatRect(150, 150, 100, 100),  // New bounds of |first|.
                    FloatRect(200, 200, 50,
                              50)));  // |second| newly appeared in the chunk.

    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  first.SetDisplayItemsUncached();
  first.SetVisualRect(LayoutRect(100, 100, 150, 150));
  second.SetDisplayItemsUncached();
  DrawRect(context, first, kBackgroundDrawingType,
           FloatRect(100, 100, 150, 150));
  DrawRect(context, first, kForegroundDrawingType,
           FloatRect(100, 100, 150, 150));
  EXPECT_EQ(0, NumCachedNewItems());
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(first, kBackgroundDrawingType),
                      TestDisplayItem(first, kForegroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                UnorderedElementsAre(
                    FloatRect(150, 150, 100, 100),  // Old bounds of |first|.
                    FloatRect(100, 100, 150, 150),  // New bounds of |first|.
                    FloatRect(200, 200, 50,
                              50)));  // |second| disappeared from the chunk.
  }
}

TEST_P(PaintControllerTest, UpdateClip) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 150, 150));
  FakeDisplayItemClient second("second", LayoutRect(100, 100, 200, 200));
  GraphicsContext context(GetPaintController());

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::Create(
      nullptr, nullptr, FloatRoundedRect(1, 1, 2, 2));

  {
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      PaintChunk::Id id(first, kClipType);
      PaintChunkProperties properties = DefaultPaintChunkProperties();
      properties.property_tree_state.SetClip(clip.Get());
      GetPaintController().UpdateCurrentPaintChunkProperties(&id, properties);
    }
    ClipRecorder clip_recorder(context, first, kClipType, IntRect(1, 1, 2, 2));
    DrawRect(context, first, kBackgroundDrawingType,
             FloatRect(100, 100, 150, 150));
    DrawRect(context, second, kBackgroundDrawingType,
             FloatRect(100, 100, 200, 200));
  }
  GetPaintController().CommitNewDisplayItems();

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                        TestDisplayItem(first, kBackgroundDrawingType),
                        TestDisplayItem(second, kBackgroundDrawingType));

    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  } else {
    EXPECT_DISPLAY_LIST(
        GetPaintController().GetDisplayItemList(), 4,
        TestDisplayItem(first, kClipType),
        TestDisplayItem(first, kBackgroundDrawingType),
        TestDisplayItem(second, kBackgroundDrawingType),
        TestDisplayItem(first, DisplayItem::ClipTypeToEndClipType(kClipType)));
  }

  first.SetDisplayItemsUncached();
  DrawRect(context, first, kBackgroundDrawingType,
           FloatRect(100, 100, 150, 150));
  DrawRect(context, second, kBackgroundDrawingType,
           FloatRect(100, 100, 200, 200));

  EXPECT_EQ(1, NumCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(1, NumSequentialMatches());
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(1, NumIndexedItems());
#endif

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(first, kBackgroundDrawingType),
                      TestDisplayItem(second, kBackgroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                UnorderedElementsAre(FloatRect(
                    LayoutRect::InfiniteIntRect())));  // This is a new chunk.

    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  second.SetDisplayItemsUncached();
  DrawRect(context, first, kBackgroundDrawingType,
           FloatRect(100, 100, 150, 150));

  RefPtr<ClipPaintPropertyNode> clip2 = ClipPaintPropertyNode::Create(
      nullptr, nullptr, FloatRoundedRect(1, 1, 2, 2));

  {
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      PaintChunk::Id id(second, kClipType);
      PaintChunkProperties properties = DefaultPaintChunkProperties();
      properties.property_tree_state.SetClip(clip2.Get());

      GetPaintController().UpdateCurrentPaintChunkProperties(&id, properties);
    }
    ClipRecorder clip_recorder(context, second, kClipType, IntRect(1, 1, 2, 2));
    DrawRect(context, second, kBackgroundDrawingType,
             FloatRect(100, 100, 200, 200));
  }
  GetPaintController().CommitNewDisplayItems();

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                        TestDisplayItem(first, kBackgroundDrawingType),
                        TestDisplayItem(second, kBackgroundDrawingType));

    EXPECT_EQ(2u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                UnorderedElementsAre(FloatRect(
                    100, 100, 200,
                    200)));  // |second| disappeared from the first chunk.
    EXPECT_THAT(GetPaintController().PaintChunks()[1].raster_invalidation_rects,
                UnorderedElementsAre(FloatRect(
                    LayoutRect::InfiniteIntRect())));  // This is a new chunk.
  } else {
    EXPECT_DISPLAY_LIST(
        GetPaintController().GetDisplayItemList(), 4,
        TestDisplayItem(first, kBackgroundDrawingType),
        TestDisplayItem(second, kClipType),
        TestDisplayItem(second, kBackgroundDrawingType),
        TestDisplayItem(second, DisplayItem::ClipTypeToEndClipType(kClipType)));
  }
}

TEST_P(PaintControllerTest, CachedDisplayItems) {
  FakeDisplayItemClient first("first");
  FakeDisplayItemClient second("second");
  GraphicsContext context(GetPaintController());
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  DrawRect(context, first, kBackgroundDrawingType,
           FloatRect(100, 100, 150, 150));
  DrawRect(context, second, kBackgroundDrawingType,
           FloatRect(100, 100, 150, 150));
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(first, kBackgroundDrawingType),
                      TestDisplayItem(second, kBackgroundDrawingType));
  EXPECT_TRUE(GetPaintController().ClientCacheIsValid(first));
  EXPECT_TRUE(GetPaintController().ClientCacheIsValid(second));
  sk_sp<const PaintRecord> first_paint_record =
      static_cast<const DrawingDisplayItem&>(
          GetPaintController().GetDisplayItemList()[0])
          .GetPaintRecord();
  sk_sp<const PaintRecord> second_paint_record =
      static_cast<const DrawingDisplayItem&>(
          GetPaintController().GetDisplayItemList()[1])
          .GetPaintRecord();

  first.SetDisplayItemsUncached();
  EXPECT_FALSE(GetPaintController().ClientCacheIsValid(first));
  EXPECT_TRUE(GetPaintController().ClientCacheIsValid(second));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }
  DrawRect(context, first, kBackgroundDrawingType,
           FloatRect(100, 100, 150, 150));
  DrawRect(context, second, kBackgroundDrawingType,
           FloatRect(100, 100, 150, 150));
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(first, kBackgroundDrawingType),
                      TestDisplayItem(second, kBackgroundDrawingType));
  // The first display item should be updated.
  EXPECT_NE(first_paint_record,
            static_cast<const DrawingDisplayItem&>(
                GetPaintController().GetDisplayItemList()[0])
                .GetPaintRecord());
  // The second display item should be cached.
  if (!RuntimeEnabledFeatures::paintUnderInvalidationCheckingEnabled()) {
    EXPECT_EQ(second_paint_record,
              static_cast<const DrawingDisplayItem&>(
                  GetPaintController().GetDisplayItemList()[1])
                  .GetPaintRecord());
  }
  EXPECT_TRUE(GetPaintController().ClientCacheIsValid(first));
  EXPECT_TRUE(GetPaintController().ClientCacheIsValid(second));

  GetPaintController().InvalidateAll();
  EXPECT_FALSE(GetPaintController().ClientCacheIsValid(first));
  EXPECT_FALSE(GetPaintController().ClientCacheIsValid(second));
}

TEST_P(PaintControllerTest, UpdateSwapOrderWithChildren) {
  FakeDisplayItemClient container1("container1",
                                   LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient content1("content1", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient container2("container2",
                                   LayoutRect(100, 200, 100, 100));
  FakeDisplayItemClient content2("content2", LayoutRect(100, 200, 50, 200));
  GraphicsContext context(GetPaintController());
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  DrawRect(context, container1, kBackgroundDrawingType,
           FloatRect(100, 100, 100, 100));
  DrawRect(context, content1, kBackgroundDrawingType,
           FloatRect(100, 100, 50, 200));
  DrawRect(context, content1, kForegroundDrawingType,
           FloatRect(100, 100, 50, 200));
  DrawRect(context, container1, kForegroundDrawingType,
           FloatRect(100, 100, 100, 100));
  DrawRect(context, container2, kBackgroundDrawingType,
           FloatRect(100, 200, 100, 100));
  DrawRect(context, content2, kBackgroundDrawingType,
           FloatRect(100, 200, 50, 200));
  DrawRect(context, content2, kForegroundDrawingType,
           FloatRect(100, 200, 50, 200));
  DrawRect(context, container2, kForegroundDrawingType,
           FloatRect(100, 200, 100, 100));
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 8,
                      TestDisplayItem(container1, kBackgroundDrawingType),
                      TestDisplayItem(content1, kBackgroundDrawingType),
                      TestDisplayItem(content1, kForegroundDrawingType),
                      TestDisplayItem(container1, kForegroundDrawingType),
                      TestDisplayItem(container2, kBackgroundDrawingType),
                      TestDisplayItem(content2, kBackgroundDrawingType),
                      TestDisplayItem(content2, kForegroundDrawingType),
                      TestDisplayItem(container2, kForegroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  // Simulate the situation when |container1| gets a z-index that is greater
  // than that of |container2|.
  DrawRect(context, container2, kBackgroundDrawingType,
           FloatRect(100, 200, 100, 100));
  DrawRect(context, content2, kBackgroundDrawingType,
           FloatRect(100, 200, 50, 200));
  DrawRect(context, content2, kForegroundDrawingType,
           FloatRect(100, 200, 50, 200));
  DrawRect(context, container2, kForegroundDrawingType,
           FloatRect(100, 200, 100, 100));
  DrawRect(context, container1, kBackgroundDrawingType,
           FloatRect(100, 100, 100, 100));
  DrawRect(context, content1, kBackgroundDrawingType,
           FloatRect(100, 100, 50, 200));
  DrawRect(context, content1, kForegroundDrawingType,
           FloatRect(100, 100, 50, 200));
  DrawRect(context, container1, kForegroundDrawingType,
           FloatRect(100, 100, 100, 100));
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 8,
                      TestDisplayItem(container2, kBackgroundDrawingType),
                      TestDisplayItem(content2, kBackgroundDrawingType),
                      TestDisplayItem(content2, kForegroundDrawingType),
                      TestDisplayItem(container2, kForegroundDrawingType),
                      TestDisplayItem(container1, kBackgroundDrawingType),
                      TestDisplayItem(content1, kBackgroundDrawingType),
                      TestDisplayItem(content1, kForegroundDrawingType),
                      TestDisplayItem(container1, kForegroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(
        GetPaintController().PaintChunks()[0].raster_invalidation_rects,
        UnorderedElementsAre(
            FloatRect(100, 200, 100, 100),   // Bounds of |container2| which was
                                             // moved behind |container1|.
            FloatRect(100, 200, 50, 200)));  // Bounds of |content2| which was
                                             // moved along with |container2|.
  }
}

TEST_P(PaintControllerTest, UpdateSwapOrderWithChildrenAndInvalidation) {
  FakeDisplayItemClient container1("container1",
                                   LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient content1("content1", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient container2("container2",
                                   LayoutRect(100, 200, 100, 100));
  FakeDisplayItemClient content2("content2", LayoutRect(100, 200, 50, 200));
  GraphicsContext context(GetPaintController());
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  DrawRect(context, container1, kBackgroundDrawingType,
           FloatRect(100, 100, 100, 100));
  DrawRect(context, content1, kBackgroundDrawingType,
           FloatRect(100, 100, 50, 200));
  DrawRect(context, content1, kForegroundDrawingType,
           FloatRect(100, 100, 50, 200));
  DrawRect(context, container1, kForegroundDrawingType,
           FloatRect(100, 100, 100, 100));
  DrawRect(context, container2, kBackgroundDrawingType,
           FloatRect(100, 200, 100, 100));
  DrawRect(context, content2, kBackgroundDrawingType,
           FloatRect(100, 200, 50, 200));
  DrawRect(context, content2, kForegroundDrawingType,
           FloatRect(100, 200, 50, 200));
  DrawRect(context, container2, kForegroundDrawingType,
           FloatRect(100, 200, 100, 100));
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 8,
                      TestDisplayItem(container1, kBackgroundDrawingType),
                      TestDisplayItem(content1, kBackgroundDrawingType),
                      TestDisplayItem(content1, kForegroundDrawingType),
                      TestDisplayItem(container1, kForegroundDrawingType),
                      TestDisplayItem(container2, kBackgroundDrawingType),
                      TestDisplayItem(content2, kBackgroundDrawingType),
                      TestDisplayItem(content2, kForegroundDrawingType),
                      TestDisplayItem(container2, kForegroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  // Simulate the situation when |container1| gets a z-index that is greater
  // than that of |container2|, and |container1| is invalidated.
  container1.SetDisplayItemsUncached();
  DrawRect(context, container2, kBackgroundDrawingType,
           FloatRect(100, 200, 100, 100));
  DrawRect(context, content2, kBackgroundDrawingType,
           FloatRect(100, 200, 50, 200));
  DrawRect(context, content2, kForegroundDrawingType,
           FloatRect(100, 200, 50, 200));
  DrawRect(context, container2, kForegroundDrawingType,
           FloatRect(100, 200, 100, 100));
  DrawRect(context, container1, kBackgroundDrawingType,
           FloatRect(100, 100, 100, 100));
  DrawRect(context, content1, kBackgroundDrawingType,
           FloatRect(100, 100, 50, 200));
  DrawRect(context, content1, kForegroundDrawingType,
           FloatRect(100, 100, 50, 200));
  DrawRect(context, container1, kForegroundDrawingType,
           FloatRect(100, 100, 100, 100));
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 8,
                      TestDisplayItem(container2, kBackgroundDrawingType),
                      TestDisplayItem(content2, kBackgroundDrawingType),
                      TestDisplayItem(content2, kForegroundDrawingType),
                      TestDisplayItem(container2, kForegroundDrawingType),
                      TestDisplayItem(container1, kBackgroundDrawingType),
                      TestDisplayItem(content1, kBackgroundDrawingType),
                      TestDisplayItem(content1, kForegroundDrawingType),
                      TestDisplayItem(container1, kForegroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(
        GetPaintController().PaintChunks()[0].raster_invalidation_rects,
        UnorderedElementsAre(
            FloatRect(100, 100, 100, 100),   // Old bounds of |container1|.
            FloatRect(100, 100, 100, 100),   // New bounds of |container1|.
            FloatRect(100, 200, 100, 100),   // Bounds of |container2| which was
                                             // moved behind |container1|.
            FloatRect(100, 200, 50, 200)));  // Bounds of |content2| which was
                                             // moved along with |container2|.
  }
}

TEST_P(PaintControllerTest, CachedSubsequenceSwapOrder) {
  FakeDisplayItemClient container1("container1",
                                   LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient content1("content1", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient container2("container2",
                                   LayoutRect(100, 200, 100, 100));
  FakeDisplayItemClient content2("content2", LayoutRect(100, 200, 50, 200));
  GraphicsContext context(GetPaintController());

  PaintChunkProperties container1_properties = DefaultPaintChunkProperties();
  PaintChunkProperties container2_properties = DefaultPaintChunkProperties();

  {
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      PaintChunk::Id id(container1, kBackgroundDrawingType);
      container1_properties.property_tree_state.SetEffect(
          CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), 0.5).Get());
      GetPaintController().UpdateCurrentPaintChunkProperties(
          &id, container1_properties);
    }
    SubsequenceRecorder r(context, container1);
    DrawRect(context, container1, kBackgroundDrawingType,
             FloatRect(100, 100, 100, 100));
    DrawRect(context, content1, kBackgroundDrawingType,
             FloatRect(100, 100, 50, 200));
    DrawRect(context, content1, kForegroundDrawingType,
             FloatRect(100, 100, 50, 200));
    DrawRect(context, container1, kForegroundDrawingType,
             FloatRect(100, 100, 100, 100));
  }
  {
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      PaintChunk::Id id(container2, kBackgroundDrawingType);
      container2_properties.property_tree_state.SetEffect(
          CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), 0.5).Get());
      GetPaintController().UpdateCurrentPaintChunkProperties(
          &id, container2_properties);
    }
    SubsequenceRecorder r(context, container2);
    DrawRect(context, container2, kBackgroundDrawingType,
             FloatRect(100, 200, 100, 100));
    DrawRect(context, content2, kBackgroundDrawingType,
             FloatRect(100, 200, 50, 200));
    DrawRect(context, content2, kForegroundDrawingType,
             FloatRect(100, 200, 50, 200));
    DrawRect(context, container2, kForegroundDrawingType,
             FloatRect(100, 200, 100, 100));
  }
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 8,
                      TestDisplayItem(container1, kBackgroundDrawingType),
                      TestDisplayItem(content1, kBackgroundDrawingType),
                      TestDisplayItem(content1, kForegroundDrawingType),
                      TestDisplayItem(container1, kForegroundDrawingType),

                      TestDisplayItem(container2, kBackgroundDrawingType),
                      TestDisplayItem(content2, kBackgroundDrawingType),
                      TestDisplayItem(content2, kForegroundDrawingType),
                      TestDisplayItem(container2, kForegroundDrawingType));

  PaintController::SubsequenceMarkers* markers =
      GetPaintController().GetSubsequenceMarkers(container1);
  CHECK(markers);
  EXPECT_EQ(0u, markers->start);
  EXPECT_EQ(3u, markers->end);

  markers = GetPaintController().GetSubsequenceMarkers(container2);
  CHECK(markers);
  EXPECT_EQ(4u, markers->start);
  EXPECT_EQ(7u, markers->end);

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(2u, GetPaintController().PaintChunks().size());
    EXPECT_EQ(PaintChunk::Id(container1, kBackgroundDrawingType),
              GetPaintController().PaintChunks()[0].id);
    EXPECT_EQ(PaintChunk::Id(container2, kBackgroundDrawingType),
              GetPaintController().PaintChunks()[1].id);
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                UnorderedElementsAre(FloatRect(LayoutRect::InfiniteIntRect())));
    EXPECT_THAT(GetPaintController().PaintChunks()[1].raster_invalidation_rects,
                UnorderedElementsAre(FloatRect(LayoutRect::InfiniteIntRect())));
  }

  // Simulate the situation when |container1| gets a z-index that is greater
  // than that of |container2|.
  if (RuntimeEnabledFeatures::paintUnderInvalidationCheckingEnabled()) {
    // When under-invalidation-checking is enabled,
    // useCachedSubsequenceIfPossible is forced off, and the client is expected
    // to create the same painting as in the previous paint.
    EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
        context, container2));
    {
      if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
        PaintChunk::Id id(container2, kBackgroundDrawingType);
        GetPaintController().UpdateCurrentPaintChunkProperties(
            &id, container2_properties);
      }
      SubsequenceRecorder r(context, container2);
      DrawRect(context, container2, kBackgroundDrawingType,
               FloatRect(100, 200, 100, 100));
      DrawRect(context, content2, kBackgroundDrawingType,
               FloatRect(100, 200, 50, 200));
      DrawRect(context, content2, kForegroundDrawingType,
               FloatRect(100, 200, 50, 200));
      DrawRect(context, container2, kForegroundDrawingType,
               FloatRect(100, 200, 100, 100));
    }
    EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
        context, container1));
    {
      if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
        PaintChunk::Id id(container1, kBackgroundDrawingType);
        GetPaintController().UpdateCurrentPaintChunkProperties(
            &id, container1_properties);
      }
      SubsequenceRecorder r(context, container1);
      DrawRect(context, container1, kBackgroundDrawingType,
               FloatRect(100, 100, 100, 100));
      DrawRect(context, content1, kBackgroundDrawingType,
               FloatRect(100, 100, 50, 200));
      DrawRect(context, content1, kForegroundDrawingType,
               FloatRect(100, 100, 50, 200));
      DrawRect(context, container1, kForegroundDrawingType,
               FloatRect(100, 100, 100, 100));
    }
  } else {
    EXPECT_TRUE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
        context, container2));
    EXPECT_TRUE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
        context, container1));
  }

  EXPECT_EQ(8, NumCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(0, NumSequentialMatches());
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(0, NumIndexedItems());
#endif

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 8,
                      TestDisplayItem(container2, kBackgroundDrawingType),
                      TestDisplayItem(content2, kBackgroundDrawingType),
                      TestDisplayItem(content2, kForegroundDrawingType),
                      TestDisplayItem(container2, kForegroundDrawingType),
                      TestDisplayItem(container1, kBackgroundDrawingType),
                      TestDisplayItem(content1, kBackgroundDrawingType),
                      TestDisplayItem(content1, kForegroundDrawingType),
                      TestDisplayItem(container1, kForegroundDrawingType));

  markers = GetPaintController().GetSubsequenceMarkers(container2);
  CHECK(markers);
  EXPECT_EQ(0u, markers->start);
  EXPECT_EQ(3u, markers->end);

  markers = GetPaintController().GetSubsequenceMarkers(container1);
  CHECK(markers);
  EXPECT_EQ(4u, markers->start);
  EXPECT_EQ(7u, markers->end);

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
  DisplayItemClient::EndShouldKeepAliveAllClients();
#endif

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(2u, GetPaintController().PaintChunks().size());
    EXPECT_EQ(PaintChunk::Id(container2, kBackgroundDrawingType),
              GetPaintController().PaintChunks()[0].id);
    EXPECT_EQ(PaintChunk::Id(container1, kBackgroundDrawingType),
              GetPaintController().PaintChunks()[1].id);
    // Swapping order of chunks should not invalidate anything.
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                UnorderedElementsAre());
    EXPECT_THAT(GetPaintController().PaintChunks()[1].raster_invalidation_rects,
                UnorderedElementsAre());
  }
}

TEST_P(PaintControllerTest, UpdateSwapOrderCrossingChunks) {
  FakeDisplayItemClient container1("container1",
                                   LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient content1("content1", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient container2("container2",
                                   LayoutRect(100, 200, 100, 100));
  FakeDisplayItemClient content2("content2", LayoutRect(100, 200, 50, 200));
  GraphicsContext context(GetPaintController());

  PaintChunkProperties container1_properties = DefaultPaintChunkProperties();
  PaintChunkProperties container2_properties = DefaultPaintChunkProperties();

  {
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      PaintChunk::Id id(container1, kBackgroundDrawingType);
      container1_properties.property_tree_state.SetEffect(
          CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), 0.5).Get());
      GetPaintController().UpdateCurrentPaintChunkProperties(
          &id, container1_properties);
    }
    DrawRect(context, container1, kBackgroundDrawingType,
             FloatRect(100, 100, 100, 100));
    DrawRect(context, content1, kBackgroundDrawingType,
             FloatRect(100, 100, 50, 200));
  }
  {
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      PaintChunk::Id id(container2, kBackgroundDrawingType);
      container2_properties.property_tree_state.SetEffect(
          CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), 0.5).Get());
      GetPaintController().UpdateCurrentPaintChunkProperties(
          &id, container2_properties);
    }
    DrawRect(context, container2, kBackgroundDrawingType,
             FloatRect(100, 200, 100, 100));
    DrawRect(context, content2, kBackgroundDrawingType,
             FloatRect(100, 200, 50, 200));
  }
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 4,
                      TestDisplayItem(container1, kBackgroundDrawingType),
                      TestDisplayItem(content1, kBackgroundDrawingType),
                      TestDisplayItem(container2, kBackgroundDrawingType),
                      TestDisplayItem(content2, kBackgroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(2u, GetPaintController().PaintChunks().size());
    EXPECT_EQ(PaintChunk::Id(container1, kBackgroundDrawingType),
              GetPaintController().PaintChunks()[0].id);
    EXPECT_EQ(PaintChunk::Id(container2, kBackgroundDrawingType),
              GetPaintController().PaintChunks()[1].id);
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                UnorderedElementsAre(FloatRect(LayoutRect::InfiniteIntRect())));
    EXPECT_THAT(GetPaintController().PaintChunks()[1].raster_invalidation_rects,
                UnorderedElementsAre(FloatRect(LayoutRect::InfiniteIntRect())));
  }

  // Move content2 into container1, without invalidation.
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    PaintChunk::Id id(container1, kBackgroundDrawingType);
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &id, container1_properties);
  }
  DrawRect(context, container1, kBackgroundDrawingType,
           FloatRect(100, 100, 100, 100));
  DrawRect(context, content1, kBackgroundDrawingType,
           FloatRect(100, 100, 50, 200));
  DrawRect(context, content2, kBackgroundDrawingType,
           FloatRect(100, 200, 50, 200));
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    PaintChunk::Id id(container2, kBackgroundDrawingType);
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &id, container2_properties);
  }
  DrawRect(context, container2, kBackgroundDrawingType,
           FloatRect(100, 200, 100, 100));

  EXPECT_EQ(4, NumCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(3, NumSequentialMatches());
  EXPECT_EQ(1, NumOutOfOrderMatches());
  EXPECT_EQ(1, NumIndexedItems());
#endif

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 4,
                      TestDisplayItem(container1, kBackgroundDrawingType),
                      TestDisplayItem(content1, kBackgroundDrawingType),
                      TestDisplayItem(content2, kBackgroundDrawingType),
                      TestDisplayItem(container2, kBackgroundDrawingType));

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(2u, GetPaintController().PaintChunks().size());
    EXPECT_EQ(PaintChunk::Id(container1, kBackgroundDrawingType),
              GetPaintController().PaintChunks()[0].id);
    EXPECT_EQ(PaintChunk::Id(container2, kBackgroundDrawingType),
              GetPaintController().PaintChunks()[1].id);
    // |content2| is invalidated raster on both the old chunk and the new chunk.
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                UnorderedElementsAre(FloatRect(100, 200, 50, 200)));
    EXPECT_THAT(GetPaintController().PaintChunks()[1].raster_invalidation_rects,
                UnorderedElementsAre(FloatRect(100, 200, 50, 200)));
  }
}

TEST_P(PaintControllerTest, OutOfOrderNoCrash) {
  FakeDisplayItemClient client("client");
  GraphicsContext context(GetPaintController());

  const DisplayItem::Type kType1 = DisplayItem::kDrawingFirst;
  const DisplayItem::Type kType2 =
      static_cast<DisplayItem::Type>(DisplayItem::kDrawingFirst + 1);
  const DisplayItem::Type kType3 =
      static_cast<DisplayItem::Type>(DisplayItem::kDrawingFirst + 2);
  const DisplayItem::Type kType4 =
      static_cast<DisplayItem::Type>(DisplayItem::kDrawingFirst + 3);

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }
  DrawRect(context, client, kType1, FloatRect(100, 100, 100, 100));
  DrawRect(context, client, kType2, FloatRect(100, 100, 50, 200));
  DrawRect(context, client, kType3, FloatRect(100, 100, 50, 200));
  DrawRect(context, client, kType4, FloatRect(100, 100, 100, 100));

  GetPaintController().CommitNewDisplayItems();

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }
  DrawRect(context, client, kType2, FloatRect(100, 100, 50, 200));
  DrawRect(context, client, kType3, FloatRect(100, 100, 50, 200));
  DrawRect(context, client, kType1, FloatRect(100, 100, 100, 100));
  DrawRect(context, client, kType4, FloatRect(100, 100, 100, 100));

  GetPaintController().CommitNewDisplayItems();
}

TEST_P(PaintControllerTest, CachedNestedSubsequenceUpdate) {
  FakeDisplayItemClient container1("container1",
                                   LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient content1("content1", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient container2("container2",
                                   LayoutRect(100, 200, 100, 100));
  FakeDisplayItemClient content2("content2", LayoutRect(100, 200, 50, 200));
  GraphicsContext context(GetPaintController());

  PaintChunkProperties container1_background_properties =
      DefaultPaintChunkProperties();
  PaintChunkProperties content1_properties = DefaultPaintChunkProperties();
  PaintChunkProperties container1_foreground_properties =
      DefaultPaintChunkProperties();
  PaintChunkProperties container2_background_properties =
      DefaultPaintChunkProperties();
  PaintChunkProperties content2_properties = DefaultPaintChunkProperties();

  {
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      PaintChunk::Id id(container1, kBackgroundDrawingType);
      container1_background_properties.property_tree_state.SetEffect(
          CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), 0.5).Get());
      GetPaintController().UpdateCurrentPaintChunkProperties(
          &id, container1_background_properties);
    }
    SubsequenceRecorder r(context, container1);
    DrawRect(context, container1, kBackgroundDrawingType,
             FloatRect(100, 100, 100, 100));
    {
      if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
        PaintChunk::Id id(content1, kBackgroundDrawingType);
        content1_properties.property_tree_state.SetEffect(
            CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), 0.6)
                .Get());
        GetPaintController().UpdateCurrentPaintChunkProperties(
            &id, content1_properties);
      }
      SubsequenceRecorder r(context, content1);
      DrawRect(context, content1, kBackgroundDrawingType,
               FloatRect(100, 100, 50, 200));
      DrawRect(context, content1, kForegroundDrawingType,
               FloatRect(100, 100, 50, 200));
    }
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      PaintChunk::Id id(container1, kForegroundDrawingType);
      container1_foreground_properties.property_tree_state.SetEffect(
          CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), 0.5).Get());
      GetPaintController().UpdateCurrentPaintChunkProperties(
          &id, container1_foreground_properties);
    }
    DrawRect(context, container1, kForegroundDrawingType,
             FloatRect(100, 100, 100, 100));
  }
  {
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      PaintChunk::Id id(container2, kBackgroundDrawingType);
      container2_background_properties.property_tree_state.SetEffect(
          CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), 0.7).Get());
      GetPaintController().UpdateCurrentPaintChunkProperties(
          &id, container2_background_properties);
    }
    SubsequenceRecorder r(context, container2);
    DrawRect(context, container2, kBackgroundDrawingType,
             FloatRect(100, 200, 100, 100));
    {
      if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
        PaintChunk::Id id(content2, kBackgroundDrawingType);
        content2_properties.property_tree_state.SetEffect(
            CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), 0.8)
                .Get());
        GetPaintController().UpdateCurrentPaintChunkProperties(
            &id, content2_properties);
      }
      SubsequenceRecorder r(context, content2);
      DrawRect(context, content2, kBackgroundDrawingType,
               FloatRect(100, 200, 50, 200));
    }
  }
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 6,
                      TestDisplayItem(container1, kBackgroundDrawingType),
                      TestDisplayItem(content1, kBackgroundDrawingType),
                      TestDisplayItem(content1, kForegroundDrawingType),
                      TestDisplayItem(container1, kForegroundDrawingType),
                      TestDisplayItem(container2, kBackgroundDrawingType),
                      TestDisplayItem(content2, kBackgroundDrawingType));

  PaintController::SubsequenceMarkers* markers =
      GetPaintController().GetSubsequenceMarkers(container1);
  CHECK(markers);
  EXPECT_EQ(0u, markers->start);
  EXPECT_EQ(3u, markers->end);

  markers = GetPaintController().GetSubsequenceMarkers(content1);
  CHECK(markers);
  EXPECT_EQ(1u, markers->start);
  EXPECT_EQ(2u, markers->end);

  markers = GetPaintController().GetSubsequenceMarkers(container2);
  CHECK(markers);
  EXPECT_EQ(4u, markers->start);
  EXPECT_EQ(5u, markers->end);

  markers = GetPaintController().GetSubsequenceMarkers(content2);
  CHECK(markers);
  EXPECT_EQ(5u, markers->start);
  EXPECT_EQ(5u, markers->end);

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(5u, GetPaintController().PaintChunks().size());
    EXPECT_EQ(PaintChunk::Id(container1, kBackgroundDrawingType),
              GetPaintController().PaintChunks()[0].id);
    EXPECT_EQ(PaintChunk::Id(content1, kBackgroundDrawingType),
              GetPaintController().PaintChunks()[1].id);
    EXPECT_EQ(PaintChunk::Id(container1, kForegroundDrawingType),
              GetPaintController().PaintChunks()[2].id);
    EXPECT_EQ(PaintChunk::Id(container2, kBackgroundDrawingType),
              GetPaintController().PaintChunks()[3].id);
    EXPECT_EQ(PaintChunk::Id(content2, kBackgroundDrawingType),
              GetPaintController().PaintChunks()[4].id);
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                UnorderedElementsAre(FloatRect(LayoutRect::InfiniteIntRect())));
    EXPECT_THAT(GetPaintController().PaintChunks()[1].raster_invalidation_rects,
                UnorderedElementsAre(FloatRect(LayoutRect::InfiniteIntRect())));
    EXPECT_THAT(GetPaintController().PaintChunks()[2].raster_invalidation_rects,
                UnorderedElementsAre(FloatRect(LayoutRect::InfiniteIntRect())));
    EXPECT_THAT(GetPaintController().PaintChunks()[3].raster_invalidation_rects,
                UnorderedElementsAre(FloatRect(LayoutRect::InfiniteIntRect())));
    EXPECT_THAT(GetPaintController().PaintChunks()[4].raster_invalidation_rects,
                UnorderedElementsAre(FloatRect(LayoutRect::InfiniteIntRect())));
  }

  // Invalidate container1 but not content1.
  container1.SetDisplayItemsUncached();

  // Container2 itself now becomes empty (but still has the 'content2' child),
  // and chooses not to output subsequence info.

  container2.SetDisplayItemsUncached();
  content2.SetDisplayItemsUncached();
  EXPECT_FALSE(
      SubsequenceRecorder::UseCachedSubsequenceIfPossible(context, container2));
  EXPECT_FALSE(
      SubsequenceRecorder::UseCachedSubsequenceIfPossible(context, content2));
  // Content2 now outputs foreground only.
  {
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      PaintChunk::Id id(content2, kForegroundDrawingType);
      GetPaintController().UpdateCurrentPaintChunkProperties(
          &id, content2_properties);
    }
    SubsequenceRecorder r(context, content2);
    DrawRect(context, content2, kForegroundDrawingType,
             FloatRect(100, 200, 50, 200));
  }
  // Repaint container1 with foreground only.
  {
    EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
        context, container1));
    SubsequenceRecorder r(context, container1);
    // Use cached subsequence of content1.
    if (RuntimeEnabledFeatures::paintUnderInvalidationCheckingEnabled()) {
      // When under-invalidation-checking is enabled,
      // useCachedSubsequenceIfPossible is forced off, and the client is
      // expected to create the same painting as in the previous paint.
      EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
          context, content1));
      if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
        PaintChunk::Id id(content1, kBackgroundDrawingType);
        GetPaintController().UpdateCurrentPaintChunkProperties(
            &id, content1_properties);
      }
      SubsequenceRecorder r(context, content1);
      DrawRect(context, content1, kBackgroundDrawingType,
               FloatRect(100, 100, 50, 200));
      DrawRect(context, content1, kForegroundDrawingType,
               FloatRect(100, 100, 50, 200));
    } else {
      EXPECT_TRUE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
          context, content1));
    }
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      PaintChunk::Id id(container1, kForegroundDrawingType);
      GetPaintController().UpdateCurrentPaintChunkProperties(
          &id, container1_foreground_properties);
    }
    DrawRect(context, container1, kForegroundDrawingType,
             FloatRect(100, 100, 100, 100));
  }

  EXPECT_EQ(2, NumCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(0, NumSequentialMatches());
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(0, NumIndexedItems());
#endif

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 4,
                      TestDisplayItem(content2, kForegroundDrawingType),
                      TestDisplayItem(content1, kBackgroundDrawingType),
                      TestDisplayItem(content1, kForegroundDrawingType),
                      TestDisplayItem(container1, kForegroundDrawingType));

  markers = GetPaintController().GetSubsequenceMarkers(content2);
  CHECK(markers);
  EXPECT_EQ(0u, markers->start);
  EXPECT_EQ(0u, markers->end);

  markers = GetPaintController().GetSubsequenceMarkers(container1);
  CHECK(markers);
  EXPECT_EQ(1u, markers->start);
  EXPECT_EQ(3u, markers->end);

  markers = GetPaintController().GetSubsequenceMarkers(content1);
  CHECK(markers);
  EXPECT_EQ(1u, markers->start);
  EXPECT_EQ(2u, markers->end);

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(3u, GetPaintController().PaintChunks().size());
    EXPECT_EQ(PaintChunk::Id(content2, kForegroundDrawingType),
              GetPaintController().PaintChunks()[0].id);
    EXPECT_EQ(PaintChunk::Id(content1, kBackgroundDrawingType),
              GetPaintController().PaintChunks()[1].id);
    EXPECT_EQ(PaintChunk::Id(container1, kForegroundDrawingType),
              GetPaintController().PaintChunks()[2].id);
    // This is a new chunk.
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                UnorderedElementsAre(FloatRect(LayoutRect::InfiniteIntRect())));
    // This chunk didn't change.
    EXPECT_THAT(GetPaintController().PaintChunks()[1].raster_invalidation_rects,
                UnorderedElementsAre());
    // |container1| is invalidated.
    EXPECT_THAT(
        GetPaintController().PaintChunks()[2].raster_invalidation_rects,
        UnorderedElementsAre(
            FloatRect(100, 100, 100, 100),    // Old bounds of |container1|.
            FloatRect(100, 100, 100, 100)));  // New bounds of |container1|.
  }

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
  DisplayItemClient::EndShouldKeepAliveAllClients();
#endif
}

TEST_P(PaintControllerTest, SkipCache) {
  FakeDisplayItemClient multicol("multicol", LayoutRect(100, 100, 200, 200));
  FakeDisplayItemClient content("content", LayoutRect(100, 100, 100, 100));
  GraphicsContext context(GetPaintController());
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  FloatRect rect1(100, 100, 50, 50);
  FloatRect rect2(150, 100, 50, 50);
  FloatRect rect3(200, 100, 50, 50);

  DrawRect(context, multicol, kBackgroundDrawingType,
           FloatRect(100, 200, 100, 100));

  GetPaintController().BeginSkippingCache();
  DrawRect(context, content, kForegroundDrawingType, rect1);
  DrawRect(context, content, kForegroundDrawingType, rect2);
  GetPaintController().EndSkippingCache();

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 3,
                      TestDisplayItem(multicol, kBackgroundDrawingType),
                      TestDisplayItem(content, kForegroundDrawingType),
                      TestDisplayItem(content, kForegroundDrawingType));
  sk_sp<const PaintRecord> record1 =
      static_cast<const DrawingDisplayItem&>(
          GetPaintController().GetDisplayItemList()[1])
          .GetPaintRecord();
  sk_sp<const PaintRecord> record2 =
      static_cast<const DrawingDisplayItem&>(
          GetPaintController().GetDisplayItemList()[2])
          .GetPaintRecord();
  EXPECT_NE(record1, record2);

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                UnorderedElementsAre(FloatRect(LayoutRect::InfiniteIntRect())));

    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  // Draw again with nothing invalidated.
  EXPECT_TRUE(GetPaintController().ClientCacheIsValid(multicol));
  DrawRect(context, multicol, kBackgroundDrawingType,
           FloatRect(100, 200, 100, 100));

  GetPaintController().BeginSkippingCache();
  DrawRect(context, content, kForegroundDrawingType, rect1);
  DrawRect(context, content, kForegroundDrawingType, rect2);
  GetPaintController().EndSkippingCache();

  EXPECT_EQ(1, NumCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(1, NumSequentialMatches());
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(0, NumIndexedItems());
#endif

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 3,
                      TestDisplayItem(multicol, kBackgroundDrawingType),
                      TestDisplayItem(content, kForegroundDrawingType),
                      TestDisplayItem(content, kForegroundDrawingType));
  EXPECT_NE(record1, static_cast<const DrawingDisplayItem&>(
                         GetPaintController().GetDisplayItemList()[1])
                         .GetPaintRecord());
  EXPECT_NE(record2, static_cast<const DrawingDisplayItem&>(
                         GetPaintController().GetDisplayItemList()[2])
                         .GetPaintRecord());

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(
        GetPaintController().PaintChunks()[0].raster_invalidation_rects,
        UnorderedElementsAre(
            FloatRect(100, 100, 100, 100),    // Old bounds of |content|.
            FloatRect(100, 100, 100, 100)));  // New bounds of |content|.

    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  // Now the multicol becomes 3 columns and repaints.
  multicol.SetDisplayItemsUncached();
  DrawRect(context, multicol, kBackgroundDrawingType,
           FloatRect(100, 100, 100, 100));

  GetPaintController().BeginSkippingCache();
  DrawRect(context, content, kForegroundDrawingType, rect1);
  DrawRect(context, content, kForegroundDrawingType, rect2);
  DrawRect(context, content, kForegroundDrawingType, rect3);
  GetPaintController().EndSkippingCache();

  // We should repaint everything on invalidation of the scope container.
  EXPECT_DISPLAY_LIST(GetPaintController().NewDisplayItemList(), 4,
                      TestDisplayItem(multicol, kBackgroundDrawingType),
                      TestDisplayItem(content, kForegroundDrawingType),
                      TestDisplayItem(content, kForegroundDrawingType),
                      TestDisplayItem(content, kForegroundDrawingType));
  EXPECT_NE(record1, static_cast<const DrawingDisplayItem&>(
                         GetPaintController().NewDisplayItemList()[1])
                         .GetPaintRecord());
  EXPECT_NE(record2, static_cast<const DrawingDisplayItem&>(
                         GetPaintController().NewDisplayItemList()[2])
                         .GetPaintRecord());

  GetPaintController().CommitNewDisplayItems();

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(
        GetPaintController().PaintChunks()[0].raster_invalidation_rects,
        UnorderedElementsAre(
            FloatRect(100, 100, 200, 200),    // Old bounds of |multicol|.
            FloatRect(100, 100, 200, 200),    // New bounds of |multicol|.
            FloatRect(100, 100, 100, 100),    // Old bounds of |content|.
            FloatRect(100, 100, 100, 100)));  // New bounds of |content|.
  }
}

TEST_P(PaintControllerTest, PartialSkipCache) {
  FakeDisplayItemClient content("content");
  GraphicsContext context(GetPaintController());

  FloatRect rect1(100, 100, 50, 50);
  FloatRect rect2(150, 100, 50, 50);
  FloatRect rect3(200, 100, 50, 50);

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }
  DrawRect(context, content, kBackgroundDrawingType, rect1);
  GetPaintController().BeginSkippingCache();
  DrawRect(context, content, kForegroundDrawingType, rect2);
  GetPaintController().EndSkippingCache();
  DrawRect(context, content, kForegroundDrawingType, rect3);

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 3,
                      TestDisplayItem(content, kBackgroundDrawingType),
                      TestDisplayItem(content, kForegroundDrawingType),
                      TestDisplayItem(content, kForegroundDrawingType));
  sk_sp<const PaintRecord> record0 =
      static_cast<const DrawingDisplayItem&>(
          GetPaintController().GetDisplayItemList()[0])
          .GetPaintRecord();
  sk_sp<const PaintRecord> record1 =
      static_cast<const DrawingDisplayItem&>(
          GetPaintController().GetDisplayItemList()[1])
          .GetPaintRecord();
  sk_sp<const PaintRecord> record2 =
      static_cast<const DrawingDisplayItem&>(
          GetPaintController().GetDisplayItemList()[2])
          .GetPaintRecord();
  EXPECT_NE(record1, record2);

  // Content's cache is invalid because it has display items skipped cache.
  EXPECT_FALSE(GetPaintController().ClientCacheIsValid(content));
  EXPECT_EQ(kPaintInvalidationFull, content.GetPaintInvalidationReason());

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }
  // Draw again with nothing invalidated.
  DrawRect(context, content, kBackgroundDrawingType, rect1);
  GetPaintController().BeginSkippingCache();
  DrawRect(context, content, kForegroundDrawingType, rect2);
  GetPaintController().EndSkippingCache();
  DrawRect(context, content, kForegroundDrawingType, rect3);

  EXPECT_EQ(0, NumCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(0, NumSequentialMatches());
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(0, NumIndexedItems());
#endif

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 3,
                      TestDisplayItem(content, kBackgroundDrawingType),
                      TestDisplayItem(content, kForegroundDrawingType),
                      TestDisplayItem(content, kForegroundDrawingType));
  EXPECT_NE(record0, static_cast<const DrawingDisplayItem&>(
                         GetPaintController().GetDisplayItemList()[0])
                         .GetPaintRecord());
  EXPECT_NE(record1, static_cast<const DrawingDisplayItem&>(
                         GetPaintController().GetDisplayItemList()[1])
                         .GetPaintRecord());
  EXPECT_NE(record2, static_cast<const DrawingDisplayItem&>(
                         GetPaintController().GetDisplayItemList()[2])
                         .GetPaintRecord());
}

TEST_F(PaintControllerTestBase, OptimizeNoopPairs) {
  FakeDisplayItemClient first("first");
  FakeDisplayItemClient second("second");
  FakeDisplayItemClient third("third");

  GraphicsContext context(GetPaintController());
  DrawRect(context, first, kBackgroundDrawingType, FloatRect(0, 0, 100, 100));
  {
    ClipPathRecorder clip_recorder(context, second, Path());
    DrawRect(context, second, kBackgroundDrawingType,
             FloatRect(0, 0, 100, 100));
  }
  DrawRect(context, third, kBackgroundDrawingType, FloatRect(0, 0, 100, 100));

  GetPaintController().CommitNewDisplayItems();
  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 5,
                      TestDisplayItem(first, kBackgroundDrawingType),
                      TestDisplayItem(second, DisplayItem::kBeginClipPath),
                      TestDisplayItem(second, kBackgroundDrawingType),
                      TestDisplayItem(second, DisplayItem::kEndClipPath),
                      TestDisplayItem(third, kBackgroundDrawingType));

  DrawRect(context, first, kBackgroundDrawingType, FloatRect(0, 0, 100, 100));
  {
    ClipRecorder clip_recorder(context, second, kClipType, IntRect(1, 1, 2, 2));
    // Do not draw anything for second.
  }
  DrawRect(context, third, kBackgroundDrawingType, FloatRect(0, 0, 100, 100));
  GetPaintController().CommitNewDisplayItems();

  // Empty clips should have been optimized out.
  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(first, kBackgroundDrawingType),
                      TestDisplayItem(third, kBackgroundDrawingType));

  second.SetDisplayItemsUncached();
  DrawRect(context, first, kBackgroundDrawingType, FloatRect(0, 0, 100, 100));
  {
    ClipRecorder clip_recorder(context, second, kClipType, IntRect(1, 1, 2, 2));
    {
      ClipPathRecorder clip_path_recorder(context, second, Path());
      // Do not draw anything for second.
    }
  }
  DrawRect(context, third, kBackgroundDrawingType, FloatRect(0, 0, 100, 100));
  GetPaintController().CommitNewDisplayItems();

  // Empty clips should have been optimized out.
  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(first, kBackgroundDrawingType),
                      TestDisplayItem(third, kBackgroundDrawingType));
}

TEST_F(PaintControllerTestBase, SmallPaintControllerHasOnePaintChunk) {
  ScopedSlimmingPaintV2ForTest enable_s_pv2(true);
  FakeDisplayItemClient client("test client");

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        nullptr, DefaultPaintChunkProperties());
  }
  GraphicsContext context(GetPaintController());
  DrawRect(context, client, kBackgroundDrawingType, FloatRect(0, 0, 100, 100));

  GetPaintController().CommitNewDisplayItems();
  const auto& paint_chunks = GetPaintController().PaintChunks();
  ASSERT_EQ(1u, paint_chunks.size());
  EXPECT_EQ(0u, paint_chunks[0].begin_index);
  EXPECT_EQ(1u, paint_chunks[0].end_index);
}

TEST_F(PaintControllerTestBase, PaintArtifactWithVisualRects) {
  FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));

  GraphicsContext context(GetPaintController());
  DrawRect(context, client, kBackgroundDrawingType, FloatRect(0, 0, 100, 100));

  GetPaintController().CommitNewDisplayItems(LayoutSize(20, 30));
  const auto& paint_artifact = GetPaintController().GetPaintArtifact();
  ASSERT_EQ(1u, paint_artifact.GetDisplayItemList().size());
  EXPECT_EQ(IntRect(-20, -30, 200, 100), VisualRect(paint_artifact, 0));
}

void DrawPath(GraphicsContext& context,
              DisplayItemClient& client,
              DisplayItem::Type type,
              unsigned count) {
  if (DrawingRecorder::UseCachedDrawingIfPossible(context, client, type))
    return;

  DrawingRecorder drawing_recorder(context, client, type,
                                   FloatRect(0, 0, 100, 100));
  SkPath path;
  path.moveTo(0, 0);
  path.lineTo(0, 100);
  path.lineTo(50, 50);
  path.lineTo(100, 100);
  path.lineTo(100, 0);
  path.close();
  PaintFlags flags;
  flags.setAntiAlias(true);
  for (unsigned i = 0; i < count; i++)
    context.DrawPath(path, flags);
}

TEST_F(PaintControllerTestBase, IsSuitableForGpuRasterizationSinglePath) {
  FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));
  GraphicsContext context(GetPaintController());
  DrawPath(context, client, kBackgroundDrawingType, 1);
  GetPaintController().CommitNewDisplayItems(LayoutSize());
  EXPECT_TRUE(
      GetPaintController().GetPaintArtifact().IsSuitableForGpuRasterization());
}

TEST_F(PaintControllerTestBase,
       IsNotSuitableForGpuRasterizationSinglePaintRecordManyPaths) {
  FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));
  GraphicsContext context(GetPaintController());

  DrawPath(context, client, kBackgroundDrawingType, 50);
  GetPaintController().CommitNewDisplayItems(LayoutSize());
  EXPECT_FALSE(
      GetPaintController().GetPaintArtifact().IsSuitableForGpuRasterization());
}

TEST_F(PaintControllerTestBase,
       IsNotSuitableForGpuRasterizationMultiplePaintRecordsSinglePathEach) {
  FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));
  GraphicsContext context(GetPaintController());
  GetPaintController().BeginSkippingCache();

  for (int i = 0; i < 50; ++i)
    DrawPath(context, client, kBackgroundDrawingType, 50);

  GetPaintController().EndSkippingCache();
  GetPaintController().CommitNewDisplayItems(LayoutSize());
  EXPECT_FALSE(
      GetPaintController().GetPaintArtifact().IsSuitableForGpuRasterization());
}

TEST_F(PaintControllerTestBase,
       IsNotSuitableForGpuRasterizationSinglePaintRecordManyPathsTwoPaints) {
  FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));

  {
    GraphicsContext context(GetPaintController());
    DrawPath(context, client, kBackgroundDrawingType, 50);
    GetPaintController().CommitNewDisplayItems(LayoutSize());
    EXPECT_FALSE(GetPaintController()
                     .GetPaintArtifact()
                     .IsSuitableForGpuRasterization());
  }

  client.SetDisplayItemsUncached();

  {
    GraphicsContext context(GetPaintController());
    DrawPath(context, client, kBackgroundDrawingType, 50);
    GetPaintController().CommitNewDisplayItems(LayoutSize());
    EXPECT_FALSE(GetPaintController()
                     .GetPaintArtifact()
                     .IsSuitableForGpuRasterization());
  }
}

TEST_F(PaintControllerTestBase,
       IsNotSuitableForGpuRasterizationSinglePaintRecordManyPathsCached) {
  FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));

  {
    GraphicsContext context(GetPaintController());
    DrawPath(context, client, kBackgroundDrawingType, 50);
    GetPaintController().CommitNewDisplayItems(LayoutSize());
    EXPECT_FALSE(GetPaintController()
                     .GetPaintArtifact()
                     .IsSuitableForGpuRasterization());
  }

  {
    GraphicsContext context(GetPaintController());
    DrawPath(context, client, kBackgroundDrawingType, 50);
    GetPaintController().CommitNewDisplayItems(LayoutSize());
    EXPECT_FALSE(GetPaintController()
                     .GetPaintArtifact()
                     .IsSuitableForGpuRasterization());
  }
}

TEST_F(
    PaintControllerTestBase,
    IsNotSuitableForGpuRasterizationSinglePaintRecordManyPathsCachedSubsequence) {
  FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));
  FakeDisplayItemClient container("container", LayoutRect(0, 0, 200, 100));

  GraphicsContext context(GetPaintController());
  {
    SubsequenceRecorder subsequence_recorder(context, container);
    DrawPath(context, client, kBackgroundDrawingType, 50);
  }
  GetPaintController().CommitNewDisplayItems(LayoutSize());
  EXPECT_FALSE(
      GetPaintController().GetPaintArtifact().IsSuitableForGpuRasterization());

  EXPECT_TRUE(
      SubsequenceRecorder::UseCachedSubsequenceIfPossible(context, container));
  GetPaintController().CommitNewDisplayItems(LayoutSize());
  EXPECT_FALSE(
      GetPaintController().GetPaintArtifact().IsSuitableForGpuRasterization());

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
  DisplayItemClient::EndShouldKeepAliveAllClients();
#endif
}

// Temporarily disabled (pref regressions due to GPU veto stickiness:
// http://crbug.com/603969).
TEST_F(PaintControllerTestBase,
       DISABLED_IsNotSuitableForGpuRasterizationConcaveClipPath) {
  Path path;
  path.AddLineTo(FloatPoint(50, 50));
  path.AddLineTo(FloatPoint(100, 0));
  path.AddLineTo(FloatPoint(50, 100));
  path.CloseSubpath();

  FakeDisplayItemClient client("test client", LayoutRect(0, 0, 200, 100));
  GraphicsContext context(GetPaintController());

  // Run twice for empty/non-empty m_currentPaintArtifact coverage.
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 50; ++j)
      GetPaintController().CreateAndAppend<BeginClipPathDisplayItem>(client,
                                                                     path);
    DrawRect(context, client, kBackgroundDrawingType,
             FloatRect(0, 0, 100, 100));
    for (int j = 0; j < 50; ++j)
      GetPaintController().CreateAndAppend<EndClipPathDisplayItem>(client);
    GetPaintController().CommitNewDisplayItems(LayoutSize());
    EXPECT_FALSE(GetPaintController()
                     .GetPaintArtifact()
                     .IsSuitableForGpuRasterization());
  }
}

// Death tests don't work properly on Android.
#if defined(GTEST_HAS_DEATH_TEST) && !OS(ANDROID)

class PaintControllerUnderInvalidationTest
    : public PaintControllerTestBase,
      private ScopedPaintUnderInvalidationCheckingForTest {
 public:
  PaintControllerUnderInvalidationTest()
      : PaintControllerTestBase(),
        ScopedPaintUnderInvalidationCheckingForTest(true) {}

 protected:
  void TestChangeDrawing() {
    FakeDisplayItemClient first("first");
    GraphicsContext context(GetPaintController());

    DrawRect(context, first, kBackgroundDrawingType,
             FloatRect(100, 100, 300, 300));
    DrawRect(context, first, kForegroundDrawingType,
             FloatRect(100, 100, 300, 300));
    GetPaintController().CommitNewDisplayItems();
    DrawRect(context, first, kBackgroundDrawingType,
             FloatRect(200, 200, 300, 300));
    DrawRect(context, first, kForegroundDrawingType,
             FloatRect(100, 100, 300, 300));
    GetPaintController().CommitNewDisplayItems();
  }

  void TestMoreDrawing() {
    FakeDisplayItemClient first("first");
    GraphicsContext context(GetPaintController());

    DrawRect(context, first, kBackgroundDrawingType,
             FloatRect(100, 100, 300, 300));
    GetPaintController().CommitNewDisplayItems();
    DrawRect(context, first, kBackgroundDrawingType,
             FloatRect(100, 100, 300, 300));
    DrawRect(context, first, kForegroundDrawingType,
             FloatRect(100, 100, 300, 300));
    GetPaintController().CommitNewDisplayItems();
  }

  void TestLessDrawing() {
    FakeDisplayItemClient first("first");
    GraphicsContext context(GetPaintController());

    DrawRect(context, first, kBackgroundDrawingType,
             FloatRect(100, 100, 300, 300));
    DrawRect(context, first, kForegroundDrawingType,
             FloatRect(100, 100, 300, 300));
    GetPaintController().CommitNewDisplayItems();
    DrawRect(context, first, kBackgroundDrawingType,
             FloatRect(100, 100, 300, 300));
    GetPaintController().CommitNewDisplayItems();
  }

  void TestNoopPairsInSubsequence() {
    EXPECT_FALSE(GetPaintController().LastDisplayItemIsSubsequenceEnd());

    FakeDisplayItemClient container("container");
    GraphicsContext context(GetPaintController());

    {
      SubsequenceRecorder r(context, container);
      DrawRect(context, container, kBackgroundDrawingType,
               FloatRect(100, 100, 100, 100));
    }
    GetPaintController().CommitNewDisplayItems();

    EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
        context, container));
    {
      // Generate some no-op pairs which should not affect under-invalidation
      // checking.
      ClipRecorder r1(context, container, kClipType, IntRect(1, 1, 9, 9));
      ClipRecorder r2(context, container, kClipType, IntRect(1, 1, 2, 2));
      ClipRecorder r3(context, container, kClipType, IntRect(1, 1, 3, 3));
      ClipPathRecorder r4(context, container, Path());
    }
    {
      EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
          context, container));
      SubsequenceRecorder r(context, container);
      DrawRect(context, container, kBackgroundDrawingType,
               FloatRect(100, 100, 100, 100));
    }
    EXPECT_TRUE(GetPaintController().LastDisplayItemIsSubsequenceEnd());

    GetPaintController().CommitNewDisplayItems();

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
    DisplayItemClient::EndShouldKeepAliveAllClients();
#endif
  }

  void TestChangeDrawingInSubsequence() {
    FakeDisplayItemClient first("first");
    GraphicsContext context(GetPaintController());
    {
      SubsequenceRecorder r(context, first);
      DrawRect(context, first, kBackgroundDrawingType,
               FloatRect(100, 100, 300, 300));
      DrawRect(context, first, kForegroundDrawingType,
               FloatRect(100, 100, 300, 300));
    }
    GetPaintController().CommitNewDisplayItems();
    {
      EXPECT_FALSE(
          SubsequenceRecorder::UseCachedSubsequenceIfPossible(context, first));
      SubsequenceRecorder r(context, first);
      DrawRect(context, first, kBackgroundDrawingType,
               FloatRect(200, 200, 300, 300));
      DrawRect(context, first, kForegroundDrawingType,
               FloatRect(100, 100, 300, 300));
    }
    GetPaintController().CommitNewDisplayItems();
  }

  void TestMoreDrawingInSubsequence() {
    FakeDisplayItemClient first("first");
    GraphicsContext context(GetPaintController());

    {
      SubsequenceRecorder r(context, first);
      DrawRect(context, first, kBackgroundDrawingType,
               FloatRect(100, 100, 300, 300));
    }
    GetPaintController().CommitNewDisplayItems();

    {
      EXPECT_FALSE(
          SubsequenceRecorder::UseCachedSubsequenceIfPossible(context, first));
      SubsequenceRecorder r(context, first);
      DrawRect(context, first, kBackgroundDrawingType,
               FloatRect(100, 100, 300, 300));
      DrawRect(context, first, kForegroundDrawingType,
               FloatRect(100, 100, 300, 300));
    }
    GetPaintController().CommitNewDisplayItems();
  }

  void TestLessDrawingInSubsequence() {
    FakeDisplayItemClient first("first");
    GraphicsContext context(GetPaintController());

    {
      SubsequenceRecorder r(context, first);
      DrawRect(context, first, kBackgroundDrawingType,
               FloatRect(100, 100, 300, 300));
      DrawRect(context, first, kForegroundDrawingType,
               FloatRect(100, 100, 300, 300));
    }
    GetPaintController().CommitNewDisplayItems();

    {
      EXPECT_FALSE(
          SubsequenceRecorder::UseCachedSubsequenceIfPossible(context, first));
      SubsequenceRecorder r(context, first);
      DrawRect(context, first, kBackgroundDrawingType,
               FloatRect(100, 100, 300, 300));
    }
    GetPaintController().CommitNewDisplayItems();
  }

  void TestChangeNonCacheableInSubsequence() {
    FakeDisplayItemClient container("container");
    FakeDisplayItemClient content("content");
    GraphicsContext context(GetPaintController());

    {
      SubsequenceRecorder r(context, container);
      { ClipPathRecorder clip_path_recorder(context, container, Path()); }
      ClipRecorder clip(context, container, kClipType, IntRect(1, 1, 9, 9));
      DrawRect(context, content, kBackgroundDrawingType,
               FloatRect(100, 100, 300, 300));
    }
    GetPaintController().CommitNewDisplayItems();

    {
      EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
          context, container));
      SubsequenceRecorder r(context, container);
      { ClipPathRecorder clip_path_recorder(context, container, Path()); }
      ClipRecorder clip(context, container, kClipType, IntRect(1, 1, 30, 30));
      DrawRect(context, content, kBackgroundDrawingType,
               FloatRect(100, 100, 300, 300));
    }
    GetPaintController().CommitNewDisplayItems();
  }

  void TestInvalidationInSubsequence() {
    FakeDisplayItemClient container("container");
    FakeDisplayItemClient content("content");
    GraphicsContext context(GetPaintController());

    {
      SubsequenceRecorder r(context, container);
      DrawRect(context, content, kBackgroundDrawingType,
               FloatRect(100, 100, 300, 300));
    }
    GetPaintController().CommitNewDisplayItems();

    content.SetDisplayItemsUncached();
    // Leave container not invalidated.
    {
      EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
          context, container));
      SubsequenceRecorder r(context, container);
      DrawRect(context, content, kBackgroundDrawingType,
               FloatRect(100, 100, 300, 300));
    }
    GetPaintController().CommitNewDisplayItems();

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
    DisplayItemClient::EndShouldKeepAliveAllClients();
#endif
  }

  void TestFoldCompositingDrawingInSubsequence() {
    FakeDisplayItemClient container("container");
    FakeDisplayItemClient content("content");
    GraphicsContext context(GetPaintController());

    {
      SubsequenceRecorder subsequence(context, container);
      CompositingRecorder compositing(context, content, SkBlendMode::kSrc, 0.5);
      DrawRect(context, content, kBackgroundDrawingType,
               FloatRect(100, 100, 300, 300));
    }
    GetPaintController().CommitNewDisplayItems();
    EXPECT_EQ(
        1u,
        GetPaintController().GetPaintArtifact().GetDisplayItemList().size());

    {
      EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
          context, container));
      SubsequenceRecorder subsequence(context, container);
      CompositingRecorder compositing(context, content, SkBlendMode::kSrc, 0.5);
      DrawRect(context, content, kBackgroundDrawingType,
               FloatRect(100, 100, 300, 300));
    }
    GetPaintController().CommitNewDisplayItems();
    EXPECT_EQ(
        1u,
        GetPaintController().GetPaintArtifact().GetDisplayItemList().size());

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
    DisplayItemClient::EndShouldKeepAliveAllClients();
#endif
  }
};

TEST_F(PaintControllerUnderInvalidationTest, ChangeDrawing) {
  EXPECT_DEATH(TestChangeDrawing(), "under-invalidation: display item changed");
}

TEST_F(PaintControllerUnderInvalidationTest, MoreDrawing) {
  EXPECT_DEATH(TestMoreDrawing(), "");
}

TEST_F(PaintControllerUnderInvalidationTest, LessDrawing) {
  // We don't detect under-invalidation in this case, and PaintController can
  // also handle the case gracefully. However, less drawing at one time often
  // means more-drawing at another time, so eventually we'll detect such
  // under-invalidations.
  TestLessDrawing();
}

TEST_F(PaintControllerUnderInvalidationTest, NoopPairsInSubsequence) {
  // This should not die.
  TestNoopPairsInSubsequence();
}

TEST_F(PaintControllerUnderInvalidationTest, ChangeDrawingInSubsequence) {
  EXPECT_DEATH(TestChangeDrawingInSubsequence(),
               "\"\\(In cached subsequence of first\\)\" under-invalidation: "
               "display item changed");
}

TEST_F(PaintControllerUnderInvalidationTest, MoreDrawingInSubsequence) {
  EXPECT_DEATH(TestMoreDrawingInSubsequence(),
               "Check failed: false. Can't find cached display item");
}

TEST_F(PaintControllerUnderInvalidationTest, LessDrawingInSubsequence) {
  // We allow invalidated display item clients as long as they would produce the
  // same display items. The cases of changed display items are tested by other
  // test cases.
  EXPECT_DEATH(TestLessDrawingInSubsequence(),
               "\"\\(In cached subsequence of first\\)\" under-invalidation: "
               "new subsequence wrong length");
}

TEST_F(PaintControllerUnderInvalidationTest, ChangeNonCacheableInSubsequence) {
  EXPECT_DEATH(TestChangeNonCacheableInSubsequence(),
               "\"\\(In cached subsequence of container\\)\" "
               "under-invalidation: display item changed");
}

TEST_F(PaintControllerUnderInvalidationTest, InvalidationInSubsequence) {
  // We allow invalidated display item clients as long as they would produce the
  // same display items. The cases of changed display items are tested by other
  // test cases.
  TestInvalidationInSubsequence();
}

TEST_F(PaintControllerUnderInvalidationTest,
       FoldCompositingDrawingInSubsequence) {
  TestFoldCompositingDrawingInSubsequence();
}

#endif  // defined(GTEST_HAS_DEATH_TEST) && !OS(ANDROID)

}  // namespace blink
