// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintControllerTest.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/ClipPathDisplayItem.h"
#include "platform/graphics/paint/ClipPathRecorder.h"
#include "platform/graphics/paint/ClipRecorder.h"
#include "platform/graphics/paint/CompositingRecorder.h"
#include "platform/graphics/paint/DisplayItemCacheSkipper.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/SubsequenceRecorder.h"
#include "platform/runtime_enabled_features.h"
#include "platform/testing/FakeDisplayItemClient.h"
#include "platform/testing/PaintPropertyTestHelpers.h"
#include "platform/testing/PaintTestConfigurations.h"
#include "testing/gmock/include/gmock/gmock.h"

using blink::testing::CreateOpacityOnlyEffect;
using blink::testing::DefaultPaintChunkProperties;
using ::testing::UnorderedElementsAre;

namespace blink {

// Tests using this class will be tested with under-invalidation-checking
// enabled and disabled.
class PaintControllerTest : public PaintTestConfigurations,
                            public PaintControllerTestBase {
 public:
  PaintControllerTest()
      : root_paint_property_client_("root"),
        root_paint_chunk_id_(root_paint_property_client_,
                             DisplayItem::kUninitializedType) {}

  FakeDisplayItemClient root_paint_property_client_;
  PaintChunk::Id root_paint_chunk_id_;
};

INSTANTIATE_TEST_CASE_P(All,
                        PaintControllerTest,
                        ::testing::Values(0,
                                          kSlimmingPaintV2,
                                          kUnderInvalidationChecking,
                                          kSlimmingPaintV2 |
                                              kUnderInvalidationChecking));

TEST_P(PaintControllerTest, NestedRecorders) {
  GraphicsContext context(GetPaintController());
  FakeDisplayItemClient client("client", LayoutRect(100, 100, 200, 200));
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  {
    ClipRecorder clip_recorder(context, client, kClipType,
                               IntRect(100, 100, 50, 50));
    DrawRect(context, client, kBackgroundType, FloatRect(100, 100, 200, 200));
  }
  GetPaintController().CommitNewDisplayItems();

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 1,
                        TestDisplayItem(client, kBackgroundType));

    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    // Raster invalidation for the whole chunk will be issued during
    // PaintArtifactCompositor::Update().
    EXPECT_TRUE(GetPaintController()
                    .PaintChunks()[0]
                    .raster_invalidation_rects.IsEmpty());
  } else {
    EXPECT_DISPLAY_LIST(
        GetPaintController().GetDisplayItemList(), 3,
        TestDisplayItem(client, kClipType),
        TestDisplayItem(client, kBackgroundType),
        TestDisplayItem(client, DisplayItem::ClipTypeToEndClipType(kClipType)));
  }
}

TEST_P(PaintControllerTest, UpdateBasic) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 300, 300));
  FakeDisplayItemClient second("second", LayoutRect(100, 100, 200, 200));
  GraphicsContext context(GetPaintController());
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 300, 300));
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 200, 200));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 300, 300));

  EXPECT_EQ(0, NumCachedNewItems());

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 3,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(second, kBackgroundType),
                      TestDisplayItem(first, kForegroundType));

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    // Raster invalidation for the whole chunk will be issued during
    // PaintArtifactCompositor::Update().
    EXPECT_TRUE(GetPaintController()
                    .PaintChunks()[0]
                    .raster_invalidation_rects.IsEmpty());

    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 300, 300));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 300, 300));

  EXPECT_EQ(2, NumCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(2, NumSequentialMatches());
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(1, NumIndexedItems());
#endif

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(first, kForegroundType));

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                // |second| disappeared from the chunk.
                UnorderedElementsAre(FloatRect(100, 100, 200, 200)));
  }
}

TEST_P(PaintControllerTest, UpdateSwapOrder) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient second("second", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient unaffected("unaffected", LayoutRect(300, 300, 10, 10));
  GraphicsContext context(GetPaintController());
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, second, kForegroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, unaffected, kBackgroundType, FloatRect(300, 300, 10, 10));
  DrawRect(context, unaffected, kForegroundType, FloatRect(300, 300, 10, 10));
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 6,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(first, kForegroundType),
                      TestDisplayItem(second, kBackgroundType),
                      TestDisplayItem(second, kForegroundType),
                      TestDisplayItem(unaffected, kBackgroundType),
                      TestDisplayItem(unaffected, kForegroundType));

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, second, kForegroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, unaffected, kBackgroundType, FloatRect(300, 300, 10, 10));
  DrawRect(context, unaffected, kForegroundType, FloatRect(300, 300, 10, 10));

  EXPECT_EQ(6, NumCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(5, NumSequentialMatches());  // second, first foreground, unaffected
  EXPECT_EQ(1, NumOutOfOrderMatches());  // first
  EXPECT_EQ(2, NumIndexedItems());       // first
#endif

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 6,
                      TestDisplayItem(second, kBackgroundType),
                      TestDisplayItem(second, kForegroundType),
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(first, kForegroundType),
                      TestDisplayItem(unaffected, kBackgroundType),
                      TestDisplayItem(unaffected, kForegroundType));

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                // Bounds of |second| (old and new are the same).
                UnorderedElementsAre(FloatRect(100, 100, 50, 200)));
  }
}

TEST_P(PaintControllerTest, UpdateSwapOrderWithInvalidation) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient second("second", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient unaffected("unaffected", LayoutRect(300, 300, 10, 10));
  GraphicsContext context(GetPaintController());
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, second, kForegroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, unaffected, kBackgroundType, FloatRect(300, 300, 10, 10));
  DrawRect(context, unaffected, kForegroundType, FloatRect(300, 300, 10, 10));
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 6,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(first, kForegroundType),
                      TestDisplayItem(second, kBackgroundType),
                      TestDisplayItem(second, kForegroundType),
                      TestDisplayItem(unaffected, kBackgroundType),
                      TestDisplayItem(unaffected, kForegroundType));

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  first.SetDisplayItemsUncached();
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, second, kForegroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, unaffected, kBackgroundType, FloatRect(300, 300, 10, 10));
  DrawRect(context, unaffected, kForegroundType, FloatRect(300, 300, 10, 10));

  EXPECT_EQ(4, NumCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(4, NumSequentialMatches());  // second, unaffected
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(2, NumIndexedItems());
#endif

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 6,
                      TestDisplayItem(second, kBackgroundType),
                      TestDisplayItem(second, kForegroundType),
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(first, kForegroundType),
                      TestDisplayItem(unaffected, kBackgroundType),
                      TestDisplayItem(unaffected, kForegroundType));

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                // Bounds of |first| (old and new are the same).
                UnorderedElementsAre(FloatRect(100, 100, 100, 100)));
    // No need to invalidate raster of |second|, because the client (|first|)
    // which swapped order with it has been invalidated.
  }
}

TEST_P(PaintControllerTest, UpdateNewItemInMiddle) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient second("second", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient third("third", LayoutRect(125, 100, 200, 50));
  GraphicsContext context(GetPaintController());
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 50, 200));
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(second, kBackgroundType));

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, third, kBackgroundType, FloatRect(125, 100, 200, 50));
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 50, 200));

  EXPECT_EQ(2, NumCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(2, NumSequentialMatches());  // first, second
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(0, NumIndexedItems());
#endif

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 3,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(third, kBackgroundType),
                      TestDisplayItem(second, kBackgroundType));

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                // |third| newly appeared in the chunk.
                UnorderedElementsAre(FloatRect(125, 100, 200, 50)));
  }
}

TEST_P(PaintControllerTest, UpdateInvalidationWithPhases) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient second("second", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient third("third", LayoutRect(300, 100, 50, 50));
  GraphicsContext context(GetPaintController());
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, third, kBackgroundType, FloatRect(300, 100, 50, 50));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, second, kForegroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, third, kForegroundType, FloatRect(300, 100, 50, 50));
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 6,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(second, kBackgroundType),
                      TestDisplayItem(third, kBackgroundType),
                      TestDisplayItem(first, kForegroundType),
                      TestDisplayItem(second, kForegroundType),
                      TestDisplayItem(third, kForegroundType));

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  second.SetDisplayItemsUncached();
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, third, kBackgroundType, FloatRect(300, 100, 50, 50));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, second, kForegroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, third, kForegroundType, FloatRect(300, 100, 50, 50));

  EXPECT_EQ(4, NumCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(4, NumSequentialMatches());
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(2, NumIndexedItems());
#endif

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 6,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(second, kBackgroundType),
                      TestDisplayItem(third, kBackgroundType),
                      TestDisplayItem(first, kForegroundType),
                      TestDisplayItem(second, kForegroundType),
                      TestDisplayItem(third, kForegroundType));

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                // Bounds of |second| (old and new are the same).
                UnorderedElementsAre(FloatRect(100, 100, 50, 200)));
  }
}

TEST_P(PaintControllerTest, IncrementalRasterInvalidation) {
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  LayoutRect initial_rect(100, 100, 100, 100);
  std::unique_ptr<FakeDisplayItemClient> clients[6];
  for (auto& client : clients)
    client = std::make_unique<FakeDisplayItemClient>("", initial_rect);
  GraphicsContext context(GetPaintController());

  GetPaintController().UpdateCurrentPaintChunkProperties(
      &root_paint_chunk_id_, DefaultPaintChunkProperties());
  for (auto& client : clients)
    DrawRect(context, *client, kBackgroundType, FloatRect(initial_rect));
  GetPaintController().CommitNewDisplayItems();

  GetPaintController().UpdateCurrentPaintChunkProperties(
      &root_paint_chunk_id_, DefaultPaintChunkProperties());
  clients[0]->SetVisualRect(LayoutRect(100, 100, 150, 100));
  clients[1]->SetVisualRect(LayoutRect(100, 100, 100, 150));
  clients[2]->SetVisualRect(LayoutRect(100, 100, 150, 80));
  clients[3]->SetVisualRect(LayoutRect(100, 100, 80, 150));
  clients[4]->SetVisualRect(LayoutRect(100, 100, 150, 150));
  clients[5]->SetVisualRect(LayoutRect(100, 100, 80, 80));
  for (auto& client : clients) {
    client->SetDisplayItemsUncached(PaintInvalidationReason::kIncremental);
    DrawRect(context, *client, kBackgroundType,
             FloatRect(client->VisualRect()));
  }
  GetPaintController().CommitNewDisplayItems();

  EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
  EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
              UnorderedElementsAre(FloatRect(200, 100, 50, 100),    // 0: right
                                   FloatRect(100, 200, 100, 50),    // 1: bottom
                                   FloatRect(200, 100, 50, 80),     // 2: right
                                   FloatRect(100, 180, 100, 20),    // 2: bottom
                                   FloatRect(180, 100, 20, 100),    // 3: right
                                   FloatRect(100, 200, 80, 50),     // 3: bottom
                                   FloatRect(200, 100, 50, 150),    // 4: right
                                   FloatRect(100, 200, 150, 50),    // 4: bottom
                                   FloatRect(180, 100, 20, 100),    // 5: right
                                   FloatRect(100, 180, 100, 20)));  // 5: bottom

  GetPaintController().UpdateCurrentPaintChunkProperties(
      &root_paint_chunk_id_, DefaultPaintChunkProperties());
}

TEST_P(PaintControllerTest, UpdateAddFirstOverlap) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 150, 150));
  FakeDisplayItemClient second("second", LayoutRect(200, 200, 50, 50));
  GraphicsContext context(GetPaintController());
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  DrawRect(context, second, kBackgroundType, FloatRect(200, 200, 50, 50));
  DrawRect(context, second, kForegroundType, FloatRect(200, 200, 50, 50));
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(second, kBackgroundType),
                      TestDisplayItem(second, kForegroundType));

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  first.SetDisplayItemsUncached();
  second.SetDisplayItemsUncached();
  second.SetVisualRect(LayoutRect(150, 250, 100, 100));
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 150, 150));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 150, 150));
  DrawRect(context, second, kBackgroundType, FloatRect(150, 250, 100, 100));
  DrawRect(context, second, kForegroundType, FloatRect(150, 250, 100, 100));
  EXPECT_EQ(0, NumCachedNewItems());
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 4,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(first, kForegroundType),
                      TestDisplayItem(second, kBackgroundType),
                      TestDisplayItem(second, kForegroundType));

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(
        GetPaintController().PaintChunks()[0].raster_invalidation_rects,
        UnorderedElementsAre(
            // |first| newly appeared in the chunk.
            FloatRect(100, 100, 150, 150),
            // Old and new bounds of |second|.
            FloatRect(200, 200, 50, 50), FloatRect(150, 250, 100, 100)));

    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  DrawRect(context, second, kBackgroundType, FloatRect(150, 250, 100, 100));
  DrawRect(context, second, kForegroundType, FloatRect(150, 250, 100, 100));

  EXPECT_EQ(2, NumCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(2, NumSequentialMatches());
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(2, NumIndexedItems());
#endif

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(second, kBackgroundType),
                      TestDisplayItem(second, kForegroundType));

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                // |first| disappeared from the chunk.
                UnorderedElementsAre(FloatRect(100, 100, 150, 150)));
  }
}

TEST_P(PaintControllerTest, UpdateAddLastOverlap) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 150, 150));
  FakeDisplayItemClient second("second", LayoutRect(200, 200, 50, 50));
  GraphicsContext context(GetPaintController());
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 150, 150));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 150, 150));
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(first, kForegroundType));

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  first.SetDisplayItemsUncached();
  first.SetVisualRect(LayoutRect(150, 150, 100, 100));
  second.SetDisplayItemsUncached();
  DrawRect(context, first, kBackgroundType, FloatRect(150, 150, 100, 100));
  DrawRect(context, first, kForegroundType, FloatRect(150, 150, 100, 100));
  DrawRect(context, second, kBackgroundType, FloatRect(200, 200, 50, 50));
  DrawRect(context, second, kForegroundType, FloatRect(200, 200, 50, 50));
  EXPECT_EQ(0, NumCachedNewItems());
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 4,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(first, kForegroundType),
                      TestDisplayItem(second, kBackgroundType),
                      TestDisplayItem(second, kForegroundType));

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                UnorderedElementsAre(
                    // The bigger of old and new bounds of |first|.
                    FloatRect(100, 100, 150, 150),
                    // |second| newly appeared in the chunk.
                    FloatRect(200, 200, 50, 50)));

    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  first.SetDisplayItemsUncached();
  first.SetVisualRect(LayoutRect(100, 100, 150, 150));
  second.SetDisplayItemsUncached();
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 150, 150));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 150, 150));
  EXPECT_EQ(0, NumCachedNewItems());
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(first, kForegroundType));

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                UnorderedElementsAre(
                    // The bigger of old and new bounds of |first|.
                    FloatRect(100, 100, 150, 150),
                    // |second| disappeared from the chunk.
                    FloatRect(200, 200, 50, 50)));
  }
}

TEST_P(PaintControllerTest, UpdateClip) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 150, 150));
  FakeDisplayItemClient second("second", LayoutRect(100, 100, 200, 200));
  GraphicsContext context(GetPaintController());

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::Create(
      nullptr, nullptr, FloatRoundedRect(1, 1, 2, 2));

  {
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      PaintChunk::Id id(first, kClipType);
      PaintChunkProperties properties = DefaultPaintChunkProperties();
      properties.property_tree_state.SetClip(clip.get());
      GetPaintController().UpdateCurrentPaintChunkProperties(&id, properties);
    }
    ClipRecorder clip_recorder(context, first, kClipType, IntRect(1, 1, 2, 2));
    DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 150, 150));
    DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 200, 200));
  }
  GetPaintController().CommitNewDisplayItems();

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                        TestDisplayItem(first, kBackgroundType),
                        TestDisplayItem(second, kBackgroundType));

    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  } else {
    EXPECT_DISPLAY_LIST(
        GetPaintController().GetDisplayItemList(), 4,
        TestDisplayItem(first, kClipType),
        TestDisplayItem(first, kBackgroundType),
        TestDisplayItem(second, kBackgroundType),
        TestDisplayItem(first, DisplayItem::ClipTypeToEndClipType(kClipType)));
  }

  first.SetDisplayItemsUncached();
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 150, 150));
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 200, 200));

  EXPECT_EQ(1, NumCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(1, NumSequentialMatches());
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(1, NumIndexedItems());
#endif

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(second, kBackgroundType));

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    // This is a new chunk. Raster invalidation for the whole chunk will be
    // issued during PaintArtifactCompositor::Update().
    EXPECT_TRUE(GetPaintController()
                    .PaintChunks()[0]
                    .raster_invalidation_rects.IsEmpty());

    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  second.SetDisplayItemsUncached();
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 150, 150));

  RefPtr<ClipPaintPropertyNode> clip2 = ClipPaintPropertyNode::Create(
      nullptr, nullptr, FloatRoundedRect(1, 1, 2, 2));

  {
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      PaintChunk::Id id(second, kClipType);
      PaintChunkProperties properties = DefaultPaintChunkProperties();
      properties.property_tree_state.SetClip(clip2.get());

      GetPaintController().UpdateCurrentPaintChunkProperties(&id, properties);
    }
    ClipRecorder clip_recorder(context, second, kClipType, IntRect(1, 1, 2, 2));
    DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 200, 200));
  }
  GetPaintController().CommitNewDisplayItems();

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                        TestDisplayItem(first, kBackgroundType),
                        TestDisplayItem(second, kBackgroundType));

    EXPECT_EQ(2u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                // |second| disappeared from the first chunk.
                UnorderedElementsAre(FloatRect(100, 100, 200, 200)));
    // This is a new chunk. Raster invalidation for the whole chunk will be
    // issued during PaintArtifactCompositor::Update().
    EXPECT_TRUE(GetPaintController()
                    .PaintChunks()[1]
                    .raster_invalidation_rects.IsEmpty());
  } else {
    EXPECT_DISPLAY_LIST(
        GetPaintController().GetDisplayItemList(), 4,
        TestDisplayItem(first, kBackgroundType),
        TestDisplayItem(second, kClipType),
        TestDisplayItem(second, kBackgroundType),
        TestDisplayItem(second, DisplayItem::ClipTypeToEndClipType(kClipType)));
  }
}

TEST_P(PaintControllerTest, CachedDisplayItems) {
  FakeDisplayItemClient first("first");
  FakeDisplayItemClient second("second");
  GraphicsContext context(GetPaintController());
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 150, 150));
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 150, 150));
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(second, kBackgroundType));
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

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 150, 150));
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 150, 150));
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(second, kBackgroundType));
  // The first display item should be updated.
  EXPECT_NE(first_paint_record,
            static_cast<const DrawingDisplayItem&>(
                GetPaintController().GetDisplayItemList()[0])
                .GetPaintRecord());
  // The second display item should be cached.
  if (!RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
    EXPECT_EQ(second_paint_record,
              static_cast<const DrawingDisplayItem&>(
                  GetPaintController().GetDisplayItemList()[1])
                  .GetPaintRecord());
  }
  EXPECT_TRUE(GetPaintController().ClientCacheIsValid(first));
  EXPECT_TRUE(GetPaintController().ClientCacheIsValid(second));

  InvalidateAll();
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
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  DrawRect(context, container1, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, content1, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, content1, kForegroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, container1, kForegroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, container2, kBackgroundType, FloatRect(100, 200, 100, 100));
  DrawRect(context, content2, kBackgroundType, FloatRect(100, 200, 50, 200));
  DrawRect(context, content2, kForegroundType, FloatRect(100, 200, 50, 200));
  DrawRect(context, container2, kForegroundType, FloatRect(100, 200, 100, 100));
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 8,
                      TestDisplayItem(container1, kBackgroundType),
                      TestDisplayItem(content1, kBackgroundType),
                      TestDisplayItem(content1, kForegroundType),
                      TestDisplayItem(container1, kForegroundType),
                      TestDisplayItem(container2, kBackgroundType),
                      TestDisplayItem(content2, kBackgroundType),
                      TestDisplayItem(content2, kForegroundType),
                      TestDisplayItem(container2, kForegroundType));

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  // Simulate the situation when |container1| gets a z-index that is greater
  // than that of |container2|.
  DrawRect(context, container2, kBackgroundType, FloatRect(100, 200, 100, 100));
  DrawRect(context, content2, kBackgroundType, FloatRect(100, 200, 50, 200));
  DrawRect(context, content2, kForegroundType, FloatRect(100, 200, 50, 200));
  DrawRect(context, container2, kForegroundType, FloatRect(100, 200, 100, 100));
  DrawRect(context, container1, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, content1, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, content1, kForegroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, container1, kForegroundType, FloatRect(100, 100, 100, 100));
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 8,
                      TestDisplayItem(container2, kBackgroundType),
                      TestDisplayItem(content2, kBackgroundType),
                      TestDisplayItem(content2, kForegroundType),
                      TestDisplayItem(container2, kForegroundType),
                      TestDisplayItem(container1, kBackgroundType),
                      TestDisplayItem(content1, kBackgroundType),
                      TestDisplayItem(content1, kForegroundType),
                      TestDisplayItem(container1, kForegroundType));

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(
        GetPaintController().PaintChunks()[0].raster_invalidation_rects,
        UnorderedElementsAre(
            // Bounds of |container2| which was moved behind |container1|.
            FloatRect(100, 200, 100, 100),
            // Bounds of |content2| which was moved along with |container2|.
            FloatRect(100, 200, 50, 200)));
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
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  DrawRect(context, container1, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, content1, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, content1, kForegroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, container1, kForegroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, container2, kBackgroundType, FloatRect(100, 200, 100, 100));
  DrawRect(context, content2, kBackgroundType, FloatRect(100, 200, 50, 200));
  DrawRect(context, content2, kForegroundType, FloatRect(100, 200, 50, 200));
  DrawRect(context, container2, kForegroundType, FloatRect(100, 200, 100, 100));
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 8,
                      TestDisplayItem(container1, kBackgroundType),
                      TestDisplayItem(content1, kBackgroundType),
                      TestDisplayItem(content1, kForegroundType),
                      TestDisplayItem(container1, kForegroundType),
                      TestDisplayItem(container2, kBackgroundType),
                      TestDisplayItem(content2, kBackgroundType),
                      TestDisplayItem(content2, kForegroundType),
                      TestDisplayItem(container2, kForegroundType));

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  // Simulate the situation when |container1| gets a z-index that is greater
  // than that of |container2|, and |container1| is invalidated.
  container1.SetDisplayItemsUncached();
  DrawRect(context, container2, kBackgroundType, FloatRect(100, 200, 100, 100));
  DrawRect(context, content2, kBackgroundType, FloatRect(100, 200, 50, 200));
  DrawRect(context, content2, kForegroundType, FloatRect(100, 200, 50, 200));
  DrawRect(context, container2, kForegroundType, FloatRect(100, 200, 100, 100));
  DrawRect(context, container1, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, content1, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, content1, kForegroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, container1, kForegroundType, FloatRect(100, 100, 100, 100));
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 8,
                      TestDisplayItem(container2, kBackgroundType),
                      TestDisplayItem(content2, kBackgroundType),
                      TestDisplayItem(content2, kForegroundType),
                      TestDisplayItem(container2, kForegroundType),
                      TestDisplayItem(container1, kBackgroundType),
                      TestDisplayItem(content1, kBackgroundType),
                      TestDisplayItem(content1, kForegroundType),
                      TestDisplayItem(container1, kForegroundType));

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(
        GetPaintController().PaintChunks()[0].raster_invalidation_rects,
        UnorderedElementsAre(
            // Bounds of |container1| (old and new are the same).
            FloatRect(100, 100, 100, 100),
            // Bounds of |container2| which was moved behind |container1|.
            FloatRect(100, 200, 100, 100),
            // Bounds of |content2| which was moved along with |container2|.
            FloatRect(100, 200, 50, 200)));
  }
}

TEST_P(PaintControllerTest, CachedSubsequenceForcePaintChunk) {
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() ||
      RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled())
    return;

  GraphicsContext context(GetPaintController());

  FakeDisplayItemClient root("root");
  auto root_properties = DefaultPaintChunkProperties();
  PaintChunk::Id root_id(root, DisplayItem::kCaret);
  // Record a first chunk with backface_hidden == false
  GetPaintController().UpdateCurrentPaintChunkProperties(&root_id,
                                                         root_properties);
  DrawRect(context, root, kBackgroundType, FloatRect(100, 100, 100, 100));

  FakeDisplayItemClient container("container");
  {
    // Record a second chunk with backface_hidden == true
    auto container_properties = DefaultPaintChunkProperties();
    container_properties.backface_hidden = true;
    PaintChunk::Id container_id(container, DisplayItem::kCaret);

    SubsequenceRecorder r(context, container);
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &container_id, container_properties);
    DrawRect(context, container, kBackgroundType,
             FloatRect(100, 100, 100, 100));
    DrawRect(context, container, kForegroundType,
             FloatRect(100, 100, 100, 100));
  }

  DrawRect(context, root, kForegroundType, FloatRect(100, 100, 100, 100));

  GetPaintController().CommitNewDisplayItems();

  root_properties.backface_hidden = true;
  // This time, record the fist chunk with backface_hidden == true
  GetPaintController().UpdateCurrentPaintChunkProperties(&root_id,
                                                         root_properties);
  DrawRect(context, root, kBackgroundType, FloatRect(100, 100, 100, 100));
  EXPECT_TRUE(GetPaintController().UseCachedSubsequenceIfPossible(container));
  DrawRect(context, root, kForegroundType, FloatRect(100, 100, 100, 100));
  GetPaintController().CommitNewDisplayItems();

  // Even though the paint properties match, |container| should receive its
  // own PaintChunk because it is a cached subsequence.
  EXPECT_EQ(3u, GetPaintController().GetPaintArtifact().PaintChunks().size());
  EXPECT_EQ(root,
            GetPaintController().GetPaintArtifact().PaintChunks()[0].id.client);
  EXPECT_EQ(container,
            GetPaintController().GetPaintArtifact().PaintChunks()[1].id.client);
  EXPECT_EQ(root,
            GetPaintController().GetPaintArtifact().PaintChunks()[2].id.client);
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
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      PaintChunk::Id id(container1, kBackgroundType);
      container1_properties.property_tree_state.SetEffect(
          CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), 0.5).get());
      GetPaintController().UpdateCurrentPaintChunkProperties(
          &id, container1_properties);
    }
    SubsequenceRecorder r(context, container1);
    DrawRect(context, container1, kBackgroundType,
             FloatRect(100, 100, 100, 100));
    DrawRect(context, content1, kBackgroundType, FloatRect(100, 100, 50, 200));
    DrawRect(context, content1, kForegroundType, FloatRect(100, 100, 50, 200));
    DrawRect(context, container1, kForegroundType,
             FloatRect(100, 100, 100, 100));
  }
  {
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      PaintChunk::Id id(container2, kBackgroundType);
      container2_properties.property_tree_state.SetEffect(
          CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), 0.5).get());
      GetPaintController().UpdateCurrentPaintChunkProperties(
          &id, container2_properties);
    }
    SubsequenceRecorder r(context, container2);
    DrawRect(context, container2, kBackgroundType,
             FloatRect(100, 200, 100, 100));
    DrawRect(context, content2, kBackgroundType, FloatRect(100, 200, 50, 200));
    DrawRect(context, content2, kForegroundType, FloatRect(100, 200, 50, 200));
    DrawRect(context, container2, kForegroundType,
             FloatRect(100, 200, 100, 100));
  }
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 8,
                      TestDisplayItem(container1, kBackgroundType),
                      TestDisplayItem(content1, kBackgroundType),
                      TestDisplayItem(content1, kForegroundType),
                      TestDisplayItem(container1, kForegroundType),

                      TestDisplayItem(container2, kBackgroundType),
                      TestDisplayItem(content2, kBackgroundType),
                      TestDisplayItem(content2, kForegroundType),
                      TestDisplayItem(container2, kForegroundType));

  auto* markers = GetSubsequenceMarkers(container1);
  CHECK(markers);
  EXPECT_EQ(0u, markers->start);
  EXPECT_EQ(4u, markers->end);

  markers = GetSubsequenceMarkers(container2);
  CHECK(markers);
  EXPECT_EQ(4u, markers->start);
  EXPECT_EQ(8u, markers->end);

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(2u, GetPaintController().PaintChunks().size());
    EXPECT_EQ(PaintChunk::Id(container1, kBackgroundType),
              GetPaintController().PaintChunks()[0].id);
    EXPECT_EQ(PaintChunk::Id(container2, kBackgroundType),
              GetPaintController().PaintChunks()[1].id);
    // Raster invalidation for the whole chunks will be issued during
    // PaintArtifactCompositor::Update().
    EXPECT_TRUE(GetPaintController()
                    .PaintChunks()[0]
                    .raster_invalidation_rects.IsEmpty());
    EXPECT_TRUE(GetPaintController()
                    .PaintChunks()[1]
                    .raster_invalidation_rects.IsEmpty());
  }

  // Simulate the situation when |container1| gets a z-index that is greater
  // than that of |container2|.
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
    // When under-invalidation-checking is enabled,
    // useCachedSubsequenceIfPossible is forced off, and the client is expected
    // to create the same painting as in the previous paint.
    EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
        context, container2));
    {
      if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
        PaintChunk::Id id(container2, kBackgroundType);
        GetPaintController().UpdateCurrentPaintChunkProperties(
            &id, container2_properties);
      }
      SubsequenceRecorder r(context, container2);
      DrawRect(context, container2, kBackgroundType,
               FloatRect(100, 200, 100, 100));
      DrawRect(context, content2, kBackgroundType,
               FloatRect(100, 200, 50, 200));
      DrawRect(context, content2, kForegroundType,
               FloatRect(100, 200, 50, 200));
      DrawRect(context, container2, kForegroundType,
               FloatRect(100, 200, 100, 100));
    }
    EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
        context, container1));
    {
      if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
        PaintChunk::Id id(container1, kBackgroundType);
        GetPaintController().UpdateCurrentPaintChunkProperties(
            &id, container1_properties);
      }
      SubsequenceRecorder r(context, container1);
      DrawRect(context, container1, kBackgroundType,
               FloatRect(100, 100, 100, 100));
      DrawRect(context, content1, kBackgroundType,
               FloatRect(100, 100, 50, 200));
      DrawRect(context, content1, kForegroundType,
               FloatRect(100, 100, 50, 200));
      DrawRect(context, container1, kForegroundType,
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
                      TestDisplayItem(container2, kBackgroundType),
                      TestDisplayItem(content2, kBackgroundType),
                      TestDisplayItem(content2, kForegroundType),
                      TestDisplayItem(container2, kForegroundType),
                      TestDisplayItem(container1, kBackgroundType),
                      TestDisplayItem(content1, kBackgroundType),
                      TestDisplayItem(content1, kForegroundType),
                      TestDisplayItem(container1, kForegroundType));

  markers = GetSubsequenceMarkers(container2);
  CHECK(markers);
  EXPECT_EQ(0u, markers->start);
  EXPECT_EQ(4u, markers->end);

  markers = GetSubsequenceMarkers(container1);
  CHECK(markers);
  EXPECT_EQ(4u, markers->start);
  EXPECT_EQ(8u, markers->end);

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(2u, GetPaintController().PaintChunks().size());
    EXPECT_EQ(PaintChunk::Id(container2, kBackgroundType),
              GetPaintController().PaintChunks()[0].id);
    EXPECT_EQ(PaintChunk::Id(container1, kBackgroundType),
              GetPaintController().PaintChunks()[1].id);
    // Swapping order of chunks should not invalidate anything.
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                UnorderedElementsAre());
    EXPECT_THAT(GetPaintController().PaintChunks()[1].raster_invalidation_rects,
                UnorderedElementsAre());
  }
}

TEST_P(PaintControllerTest, CachedSubsequenceAndDisplayItemsSwapOrder) {
  FakeDisplayItemClient root("root", LayoutRect(0, 0, 300, 300));
  FakeDisplayItemClient content1("content1", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient container2("container2",
                                   LayoutRect(100, 200, 100, 100));
  FakeDisplayItemClient content2("content2", LayoutRect(100, 200, 50, 200));
  GraphicsContext context(GetPaintController());

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  DrawRect(context, content1, kBackgroundType, FloatRect(100, 100, 50, 200));
  {
    SubsequenceRecorder r(context, container2);
    DrawRect(context, container2, kBackgroundType,
             FloatRect(100, 200, 100, 100));
    DrawRect(context, content2, kBackgroundType, FloatRect(100, 200, 50, 200));
    DrawRect(context, content2, kForegroundType, FloatRect(100, 200, 50, 200));
    DrawRect(context, container2, kForegroundType,
             FloatRect(100, 200, 100, 100));
  }
  DrawRect(context, content1, kForegroundType, FloatRect(100, 100, 50, 200));
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 6,
                      TestDisplayItem(content1, kBackgroundType),
                      TestDisplayItem(container2, kBackgroundType),
                      TestDisplayItem(content2, kBackgroundType),
                      TestDisplayItem(content2, kForegroundType),
                      TestDisplayItem(container2, kForegroundType),
                      TestDisplayItem(content1, kForegroundType));

  auto* markers = GetSubsequenceMarkers(container2);
  CHECK(markers);
  EXPECT_EQ(1u, markers->start);
  EXPECT_EQ(5u, markers->end);

  // Simulate the situation when |container2| gets a z-index that is smaller
  // than that of |content1|.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
    // When under-invalidation-checking is enabled,
    // useCachedSubsequenceIfPossible is forced off, and the client is expected
    // to create the same painting as in the previous paint.
    EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
        context, container2));
    {
      SubsequenceRecorder r(context, container2);
      DrawRect(context, container2, kBackgroundType,
               FloatRect(100, 200, 100, 100));
      DrawRect(context, content2, kBackgroundType,
               FloatRect(100, 200, 50, 200));
      DrawRect(context, content2, kForegroundType,
               FloatRect(100, 200, 50, 200));
      DrawRect(context, container2, kForegroundType,
               FloatRect(100, 200, 100, 100));
    }
    DrawRect(context, content1, kBackgroundType, FloatRect(100, 100, 50, 200));
    DrawRect(context, content1, kForegroundType, FloatRect(100, 100, 50, 200));
  } else {
    EXPECT_TRUE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
        context, container2));
    EXPECT_TRUE(DrawingRecorder::UseCachedDrawingIfPossible(context, content1,
                                                            kBackgroundType));
    EXPECT_TRUE(DrawingRecorder::UseCachedDrawingIfPossible(context, content1,
                                                            kForegroundType));
  }

  EXPECT_EQ(6, NumCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(2, NumSequentialMatches());
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(0, NumIndexedItems());
#endif

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 6,
                      TestDisplayItem(container2, kBackgroundType),
                      TestDisplayItem(content2, kBackgroundType),
                      TestDisplayItem(content2, kForegroundType),
                      TestDisplayItem(container2, kForegroundType),
                      TestDisplayItem(content1, kBackgroundType),
                      TestDisplayItem(content1, kForegroundType));

  markers = GetSubsequenceMarkers(container2);
  CHECK(markers);
  EXPECT_EQ(0u, markers->start);
  EXPECT_EQ(4u, markers->end);
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
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      PaintChunk::Id id(container1, kBackgroundType);
      container1_properties.property_tree_state.SetEffect(
          CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), 0.5).get());
      GetPaintController().UpdateCurrentPaintChunkProperties(
          &id, container1_properties);
    }
    DrawRect(context, container1, kBackgroundType,
             FloatRect(100, 100, 100, 100));
    DrawRect(context, content1, kBackgroundType, FloatRect(100, 100, 50, 200));
  }
  {
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      PaintChunk::Id id(container2, kBackgroundType);
      container2_properties.property_tree_state.SetEffect(
          CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), 0.5).get());
      GetPaintController().UpdateCurrentPaintChunkProperties(
          &id, container2_properties);
    }
    DrawRect(context, container2, kBackgroundType,
             FloatRect(100, 200, 100, 100));
    DrawRect(context, content2, kBackgroundType, FloatRect(100, 200, 50, 200));
  }
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 4,
                      TestDisplayItem(container1, kBackgroundType),
                      TestDisplayItem(content1, kBackgroundType),
                      TestDisplayItem(container2, kBackgroundType),
                      TestDisplayItem(content2, kBackgroundType));

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(2u, GetPaintController().PaintChunks().size());
    EXPECT_EQ(PaintChunk::Id(container1, kBackgroundType),
              GetPaintController().PaintChunks()[0].id);
    EXPECT_EQ(PaintChunk::Id(container2, kBackgroundType),
              GetPaintController().PaintChunks()[1].id);
    // Raster invalidation for the whole chunks will be issued during
    // PaintArtifactCompositor::Update().
    EXPECT_TRUE(GetPaintController()
                    .PaintChunks()[0]
                    .raster_invalidation_rects.IsEmpty());
    EXPECT_TRUE(GetPaintController()
                    .PaintChunks()[1]
                    .raster_invalidation_rects.IsEmpty());
  }

  // Move content2 into container1, without invalidation.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    PaintChunk::Id id(container1, kBackgroundType);
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &id, container1_properties);
  }
  DrawRect(context, container1, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, content1, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, content2, kBackgroundType, FloatRect(100, 200, 50, 200));
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    PaintChunk::Id id(container2, kBackgroundType);
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &id, container2_properties);
  }
  DrawRect(context, container2, kBackgroundType, FloatRect(100, 200, 100, 100));

  EXPECT_EQ(4, NumCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(3, NumSequentialMatches());
  EXPECT_EQ(1, NumOutOfOrderMatches());
  EXPECT_EQ(1, NumIndexedItems());
#endif

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 4,
                      TestDisplayItem(container1, kBackgroundType),
                      TestDisplayItem(content1, kBackgroundType),
                      TestDisplayItem(content2, kBackgroundType),
                      TestDisplayItem(container2, kBackgroundType));

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(2u, GetPaintController().PaintChunks().size());
    EXPECT_EQ(PaintChunk::Id(container1, kBackgroundType),
              GetPaintController().PaintChunks()[0].id);
    EXPECT_EQ(PaintChunk::Id(container2, kBackgroundType),
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

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }
  DrawRect(context, client, kType1, FloatRect(100, 100, 100, 100));
  DrawRect(context, client, kType2, FloatRect(100, 100, 50, 200));
  DrawRect(context, client, kType3, FloatRect(100, 100, 50, 200));
  DrawRect(context, client, kType4, FloatRect(100, 100, 100, 100));

  GetPaintController().CommitNewDisplayItems();

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
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
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      PaintChunk::Id id(container1, kBackgroundType);
      container1_background_properties.property_tree_state.SetEffect(
          CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), 0.5).get());
      GetPaintController().UpdateCurrentPaintChunkProperties(
          &id, container1_background_properties);
    }
    SubsequenceRecorder r(context, container1);
    DrawRect(context, container1, kBackgroundType,
             FloatRect(100, 100, 100, 100));
    {
      if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
        PaintChunk::Id id(content1, kBackgroundType);
        content1_properties.property_tree_state.SetEffect(
            CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), 0.6)
                .get());
        GetPaintController().UpdateCurrentPaintChunkProperties(
            &id, content1_properties);
      }
      SubsequenceRecorder r(context, content1);
      DrawRect(context, content1, kBackgroundType,
               FloatRect(100, 100, 50, 200));
      DrawRect(context, content1, kForegroundType,
               FloatRect(100, 100, 50, 200));
    }
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      PaintChunk::Id id(container1, kForegroundType);
      container1_foreground_properties.property_tree_state.SetEffect(
          CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), 0.5).get());
      GetPaintController().UpdateCurrentPaintChunkProperties(
          &id, container1_foreground_properties);
    }
    DrawRect(context, container1, kForegroundType,
             FloatRect(100, 100, 100, 100));
  }
  {
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      PaintChunk::Id id(container2, kBackgroundType);
      container2_background_properties.property_tree_state.SetEffect(
          CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), 0.7).get());
      GetPaintController().UpdateCurrentPaintChunkProperties(
          &id, container2_background_properties);
    }
    SubsequenceRecorder r(context, container2);
    DrawRect(context, container2, kBackgroundType,
             FloatRect(100, 200, 100, 100));
    {
      if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
        PaintChunk::Id id(content2, kBackgroundType);
        content2_properties.property_tree_state.SetEffect(
            CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), 0.8)
                .get());
        GetPaintController().UpdateCurrentPaintChunkProperties(
            &id, content2_properties);
      }
      SubsequenceRecorder r(context, content2);
      DrawRect(context, content2, kBackgroundType,
               FloatRect(100, 200, 50, 200));
    }
  }
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 6,
                      TestDisplayItem(container1, kBackgroundType),
                      TestDisplayItem(content1, kBackgroundType),
                      TestDisplayItem(content1, kForegroundType),
                      TestDisplayItem(container1, kForegroundType),
                      TestDisplayItem(container2, kBackgroundType),
                      TestDisplayItem(content2, kBackgroundType));

  auto* markers = GetSubsequenceMarkers(container1);
  CHECK(markers);
  EXPECT_EQ(0u, markers->start);
  EXPECT_EQ(4u, markers->end);

  markers = GetSubsequenceMarkers(content1);
  CHECK(markers);
  EXPECT_EQ(1u, markers->start);
  EXPECT_EQ(3u, markers->end);

  markers = GetSubsequenceMarkers(container2);
  CHECK(markers);
  EXPECT_EQ(4u, markers->start);
  EXPECT_EQ(6u, markers->end);

  markers = GetSubsequenceMarkers(content2);
  CHECK(markers);
  EXPECT_EQ(5u, markers->start);
  EXPECT_EQ(6u, markers->end);

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(5u, GetPaintController().PaintChunks().size());
    EXPECT_EQ(PaintChunk::Id(container1, kBackgroundType),
              GetPaintController().PaintChunks()[0].id);
    EXPECT_EQ(PaintChunk::Id(content1, kBackgroundType),
              GetPaintController().PaintChunks()[1].id);
    EXPECT_EQ(PaintChunk::Id(container1, kForegroundType),
              GetPaintController().PaintChunks()[2].id);
    EXPECT_EQ(PaintChunk::Id(container2, kBackgroundType),
              GetPaintController().PaintChunks()[3].id);
    EXPECT_EQ(PaintChunk::Id(content2, kBackgroundType),
              GetPaintController().PaintChunks()[4].id);
    // Raster invalidation for the whole chunks will be issued during
    // PaintArtifactCompositor::Update().
    EXPECT_TRUE(GetPaintController()
                    .PaintChunks()[0]
                    .raster_invalidation_rects.IsEmpty());
    EXPECT_TRUE(GetPaintController()
                    .PaintChunks()[1]
                    .raster_invalidation_rects.IsEmpty());
    EXPECT_TRUE(GetPaintController()
                    .PaintChunks()[2]
                    .raster_invalidation_rects.IsEmpty());
    EXPECT_TRUE(GetPaintController()
                    .PaintChunks()[3]
                    .raster_invalidation_rects.IsEmpty());
    EXPECT_TRUE(GetPaintController()
                    .PaintChunks()[4]
                    .raster_invalidation_rects.IsEmpty());
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
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      PaintChunk::Id id(content2, kForegroundType);
      GetPaintController().UpdateCurrentPaintChunkProperties(
          &id, content2_properties);
    }
    SubsequenceRecorder r(context, content2);
    DrawRect(context, content2, kForegroundType, FloatRect(100, 200, 50, 200));
  }
  // Repaint container1 with foreground only.
  {
    EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
        context, container1));
    SubsequenceRecorder r(context, container1);
    // Use cached subsequence of content1.
    if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
      // When under-invalidation-checking is enabled,
      // useCachedSubsequenceIfPossible is forced off, and the client is
      // expected to create the same painting as in the previous paint.
      EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
          context, content1));
      if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
        PaintChunk::Id id(content1, kBackgroundType);
        GetPaintController().UpdateCurrentPaintChunkProperties(
            &id, content1_properties);
      }
      SubsequenceRecorder r(context, content1);
      DrawRect(context, content1, kBackgroundType,
               FloatRect(100, 100, 50, 200));
      DrawRect(context, content1, kForegroundType,
               FloatRect(100, 100, 50, 200));
    } else {
      EXPECT_TRUE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
          context, content1));
    }
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      PaintChunk::Id id(container1, kForegroundType);
      GetPaintController().UpdateCurrentPaintChunkProperties(
          &id, container1_foreground_properties);
    }
    DrawRect(context, container1, kForegroundType,
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
                      TestDisplayItem(content2, kForegroundType),
                      TestDisplayItem(content1, kBackgroundType),
                      TestDisplayItem(content1, kForegroundType),
                      TestDisplayItem(container1, kForegroundType));

  markers = GetSubsequenceMarkers(content2);
  CHECK(markers);
  EXPECT_EQ(0u, markers->start);
  EXPECT_EQ(1u, markers->end);

  markers = GetSubsequenceMarkers(container1);
  CHECK(markers);
  EXPECT_EQ(1u, markers->start);
  EXPECT_EQ(4u, markers->end);

  markers = GetSubsequenceMarkers(content1);
  CHECK(markers);
  EXPECT_EQ(1u, markers->start);
  EXPECT_EQ(3u, markers->end);

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(3u, GetPaintController().PaintChunks().size());
    EXPECT_EQ(PaintChunk::Id(content2, kForegroundType),
              GetPaintController().PaintChunks()[0].id);
    EXPECT_EQ(PaintChunk::Id(content1, kBackgroundType),
              GetPaintController().PaintChunks()[1].id);
    EXPECT_EQ(PaintChunk::Id(container1, kForegroundType),
              GetPaintController().PaintChunks()[2].id);
    // This is a new chunk. Raster invalidation of the whole chunk will be
    // issued during PaintArtifactCompositor::Update().
    EXPECT_TRUE(GetPaintController()
                    .PaintChunks()[0]
                    .raster_invalidation_rects.IsEmpty());
    // This chunk didn't change.
    EXPECT_TRUE(GetPaintController()
                    .PaintChunks()[1]
                    .raster_invalidation_rects.IsEmpty());
    // |container1| is invalidated.
    EXPECT_THAT(GetPaintController().PaintChunks()[2].raster_invalidation_rects,
                UnorderedElementsAre(FloatRect(100, 100, 100, 100)));
  }
}

TEST_P(PaintControllerTest, SkipCache) {
  FakeDisplayItemClient multicol("multicol", LayoutRect(100, 100, 200, 200));
  FakeDisplayItemClient content("content", LayoutRect(100, 100, 100, 100));
  GraphicsContext context(GetPaintController());
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  FloatRect rect1(100, 100, 50, 50);
  FloatRect rect2(150, 100, 50, 50);
  FloatRect rect3(200, 100, 50, 50);

  DrawRect(context, multicol, kBackgroundType, FloatRect(100, 200, 100, 100));

  GetPaintController().BeginSkippingCache();
  DrawRect(context, content, kForegroundType, rect1);
  DrawRect(context, content, kForegroundType, rect2);
  GetPaintController().EndSkippingCache();

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 3,
                      TestDisplayItem(multicol, kBackgroundType),
                      TestDisplayItem(content, kForegroundType),
                      TestDisplayItem(content, kForegroundType));
  sk_sp<const PaintRecord> record1 =
      static_cast<const DrawingDisplayItem&>(
          GetPaintController().GetDisplayItemList()[1])
          .GetPaintRecord();
  sk_sp<const PaintRecord> record2 =
      static_cast<const DrawingDisplayItem&>(
          GetPaintController().GetDisplayItemList()[2])
          .GetPaintRecord();
  EXPECT_NE(record1, record2);

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    // Raster invalidation for the whole chunk will be issued during
    // PaintArtifactCompositor::Update().
    EXPECT_TRUE(GetPaintController()
                    .PaintChunks()[0]
                    .raster_invalidation_rects.IsEmpty());

    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  // Draw again with nothing invalidated.
  EXPECT_TRUE(GetPaintController().ClientCacheIsValid(multicol));
  DrawRect(context, multicol, kBackgroundType, FloatRect(100, 200, 100, 100));

  GetPaintController().BeginSkippingCache();
  DrawRect(context, content, kForegroundType, rect1);
  DrawRect(context, content, kForegroundType, rect2);
  GetPaintController().EndSkippingCache();

  EXPECT_EQ(1, NumCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(1, NumSequentialMatches());
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(0, NumIndexedItems());
#endif

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 3,
                      TestDisplayItem(multicol, kBackgroundType),
                      TestDisplayItem(content, kForegroundType),
                      TestDisplayItem(content, kForegroundType));
  EXPECT_NE(record1, static_cast<const DrawingDisplayItem&>(
                         GetPaintController().GetDisplayItemList()[1])
                         .GetPaintRecord());
  EXPECT_NE(record2, static_cast<const DrawingDisplayItem&>(
                         GetPaintController().GetDisplayItemList()[2])
                         .GetPaintRecord());

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                // Bounds of |content| (old and new are the same);
                UnorderedElementsAre(FloatRect(100, 100, 100, 100)));

    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }

  // Now the multicol becomes 3 columns and repaints.
  multicol.SetDisplayItemsUncached();
  DrawRect(context, multicol, kBackgroundType, FloatRect(100, 100, 100, 100));

  GetPaintController().BeginSkippingCache();
  DrawRect(context, content, kForegroundType, rect1);
  DrawRect(context, content, kForegroundType, rect2);
  DrawRect(context, content, kForegroundType, rect3);
  GetPaintController().EndSkippingCache();

  // We should repaint everything on invalidation of the scope container.
  EXPECT_DISPLAY_LIST(GetPaintController().NewDisplayItemList(), 4,
                      TestDisplayItem(multicol, kBackgroundType),
                      TestDisplayItem(content, kForegroundType),
                      TestDisplayItem(content, kForegroundType),
                      TestDisplayItem(content, kForegroundType));
  EXPECT_NE(record1, static_cast<const DrawingDisplayItem&>(
                         GetPaintController().NewDisplayItemList()[1])
                         .GetPaintRecord());
  EXPECT_NE(record2, static_cast<const DrawingDisplayItem&>(
                         GetPaintController().NewDisplayItemList()[2])
                         .GetPaintRecord());

  GetPaintController().CommitNewDisplayItems();

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
    EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
                UnorderedElementsAre(
                    // Bounds of |multicol| (old and new are the same);
                    FloatRect(100, 100, 200, 200),
                    // Bounds of |content| (old and new are the same);
                    FloatRect(100, 100, 100, 100)));
  }
}

TEST_P(PaintControllerTest, PartialSkipCache) {
  FakeDisplayItemClient content("content");
  GraphicsContext context(GetPaintController());

  FloatRect rect1(100, 100, 50, 50);
  FloatRect rect2(150, 100, 50, 50);
  FloatRect rect3(200, 100, 50, 50);

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }
  DrawRect(context, content, kBackgroundType, rect1);
  GetPaintController().BeginSkippingCache();
  DrawRect(context, content, kForegroundType, rect2);
  GetPaintController().EndSkippingCache();
  DrawRect(context, content, kForegroundType, rect3);

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 3,
                      TestDisplayItem(content, kBackgroundType),
                      TestDisplayItem(content, kForegroundType),
                      TestDisplayItem(content, kForegroundType));
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
  EXPECT_EQ(PaintInvalidationReason::kFull,
            content.GetPaintInvalidationReason());

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        &root_paint_chunk_id_, DefaultPaintChunkProperties());
  }
  // Draw again with nothing invalidated.
  DrawRect(context, content, kBackgroundType, rect1);
  GetPaintController().BeginSkippingCache();
  DrawRect(context, content, kForegroundType, rect2);
  GetPaintController().EndSkippingCache();
  DrawRect(context, content, kForegroundType, rect3);

  EXPECT_EQ(0, NumCachedNewItems());
#ifndef NDEBUG
  EXPECT_EQ(0, NumSequentialMatches());
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(0, NumIndexedItems());
#endif

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 3,
                      TestDisplayItem(content, kBackgroundType),
                      TestDisplayItem(content, kForegroundType),
                      TestDisplayItem(content, kForegroundType));
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

TEST_P(PaintControllerTest, OptimizeNoopPairs) {
  if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled())
    return;

  FakeDisplayItemClient first("first");
  FakeDisplayItemClient second("second");
  FakeDisplayItemClient third("third");

  GraphicsContext context(GetPaintController());
  DrawRect(context, first, kBackgroundType, FloatRect(0, 0, 100, 100));
  {
    ClipPathRecorder clip_recorder(context, second, Path());
    DrawRect(context, second, kBackgroundType, FloatRect(0, 0, 100, 100));
  }
  DrawRect(context, third, kBackgroundType, FloatRect(0, 0, 100, 100));

  GetPaintController().CommitNewDisplayItems();
  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 5,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(second, DisplayItem::kBeginClipPath),
                      TestDisplayItem(second, kBackgroundType),
                      TestDisplayItem(second, DisplayItem::kEndClipPath),
                      TestDisplayItem(third, kBackgroundType));

  DrawRect(context, first, kBackgroundType, FloatRect(0, 0, 100, 100));
  {
    ClipRecorder clip_recorder(context, second, kClipType, IntRect(1, 1, 2, 2));
    // Do not draw anything for second.
  }
  DrawRect(context, third, kBackgroundType, FloatRect(0, 0, 100, 100));
  GetPaintController().CommitNewDisplayItems();

  // Empty clips should have been optimized out.
  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(third, kBackgroundType));

  second.SetDisplayItemsUncached();
  DrawRect(context, first, kBackgroundType, FloatRect(0, 0, 100, 100));
  {
    ClipRecorder clip_recorder(context, second, kClipType, IntRect(1, 1, 2, 2));
    {
      ClipPathRecorder clip_path_recorder(context, second, Path());
      // Do not draw anything for second.
    }
  }
  DrawRect(context, third, kBackgroundType, FloatRect(0, 0, 100, 100));
  GetPaintController().CommitNewDisplayItems();

  // Empty clips should have been optimized out.
  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(third, kBackgroundType));
}

TEST_P(PaintControllerTest, SmallPaintControllerHasOnePaintChunk) {
  ScopedSlimmingPaintV2ForTest enable_s_pv2(true);
  FakeDisplayItemClient client("test client");

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        nullptr, DefaultPaintChunkProperties());
  }
  GraphicsContext context(GetPaintController());
  DrawRect(context, client, kBackgroundType, FloatRect(0, 0, 100, 100));

  GetPaintController().CommitNewDisplayItems();
  const auto& paint_chunks = GetPaintController().PaintChunks();
  ASSERT_EQ(1u, paint_chunks.size());
  EXPECT_EQ(0u, paint_chunks[0].begin_index);
  EXPECT_EQ(1u, paint_chunks[0].end_index);
}

void DrawPath(GraphicsContext& context,
              DisplayItemClient& client,
              DisplayItem::Type type,
              unsigned count) {
  if (DrawingRecorder::UseCachedDrawingIfPossible(context, client, type))
    return;

  DrawingRecorder recorder(context, client, type, FloatRect(0, 0, 100, 100));
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

TEST_P(PaintControllerTest, BeginAndEndFrame) {
  class FakeFrame {};

  // PaintController should have one null frame in the stack since beginning.
  GetPaintController().SetFirstPainted();
  FrameFirstPaint result = GetPaintController().EndFrame(nullptr);
  EXPECT_TRUE(result.first_painted);
  EXPECT_FALSE(result.text_painted);
  EXPECT_FALSE(result.image_painted);
  // Readd the null frame.
  GetPaintController().BeginFrame(nullptr);

  std::unique_ptr<FakeFrame> frame1(new FakeFrame);
  GetPaintController().BeginFrame(frame1.get());
  GetPaintController().SetFirstPainted();
  GetPaintController().SetTextPainted();
  GetPaintController().SetImagePainted();

  result = GetPaintController().EndFrame(frame1.get());
  EXPECT_TRUE(result.first_painted);
  EXPECT_TRUE(result.text_painted);
  EXPECT_TRUE(result.image_painted);

  std::unique_ptr<FakeFrame> frame2(new FakeFrame);
  GetPaintController().BeginFrame(frame2.get());
  GetPaintController().SetFirstPainted();

  std::unique_ptr<FakeFrame> frame3(new FakeFrame);
  GetPaintController().BeginFrame(frame3.get());
  GetPaintController().SetTextPainted();
  GetPaintController().SetImagePainted();

  result = GetPaintController().EndFrame(frame3.get());
  EXPECT_FALSE(result.first_painted);
  EXPECT_TRUE(result.text_painted);
  EXPECT_TRUE(result.image_painted);

  result = GetPaintController().EndFrame(frame2.get());
  EXPECT_TRUE(result.first_painted);
  EXPECT_FALSE(result.text_painted);
  EXPECT_FALSE(result.image_painted);
}

TEST_P(PaintControllerTest, PartialInvalidation) {
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  FakeDisplayItemClient client("client", LayoutRect(100, 100, 300, 300));
  GraphicsContext context(GetPaintController());

  // Test partial rect invalidation in a new chunk.
  GetPaintController().UpdateCurrentPaintChunkProperties(
      &root_paint_chunk_id_, DefaultPaintChunkProperties());
  client.SetPartialInvalidationRect(LayoutRect(200, 200, 100, 100));
  DrawRect(context, client, kBackgroundType, FloatRect(100, 100, 300, 300));
  GetPaintController().CommitNewDisplayItems();
  ASSERT_EQ(1u, GetPaintController().PaintChunks().size());
  // Raster invalidation for the whole new chunk will be issued during
  // PaintArtifactCompositor::Update().
  EXPECT_TRUE(GetPaintController()
                  .PaintChunks()[0]
                  .raster_invalidation_rects.IsEmpty());
  EXPECT_EQ(LayoutRect(), client.PartialInvalidationRect());

  // Test partial rect invalidation without other invalidations.
  GetPaintController().UpdateCurrentPaintChunkProperties(
      &root_paint_chunk_id_, DefaultPaintChunkProperties());
  client.SetPartialInvalidationRect(LayoutRect(150, 160, 170, 180));
  DrawRect(context, client, kBackgroundType, FloatRect(100, 100, 300, 300));
  GetPaintController().CommitNewDisplayItems();
  ASSERT_EQ(1u, GetPaintController().PaintChunks().size());
  EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
              // Partial invalidation.
              UnorderedElementsAre(FloatRect(150, 160, 170, 180)));
  EXPECT_EQ(LayoutRect(), client.PartialInvalidationRect());

  // Test partial rect invalidation with full invalidation.
  GetPaintController().UpdateCurrentPaintChunkProperties(
      &root_paint_chunk_id_, DefaultPaintChunkProperties());
  client.SetPartialInvalidationRect(LayoutRect(150, 160, 170, 180));
  client.SetDisplayItemsUncached();
  DrawRect(context, client, kBackgroundType, FloatRect(100, 100, 300, 300));
  GetPaintController().CommitNewDisplayItems();
  ASSERT_EQ(1u, GetPaintController().PaintChunks().size());
  EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
              // Partial invalidation is shadowed by full invalidation.
              UnorderedElementsAre(FloatRect(100, 100, 300, 300)));
  EXPECT_EQ(LayoutRect(), client.PartialInvalidationRect());

  // Test partial rect invalidation with incremental invalidation.
  GetPaintController().UpdateCurrentPaintChunkProperties(
      &root_paint_chunk_id_, DefaultPaintChunkProperties());
  client.SetPartialInvalidationRect(LayoutRect(150, 160, 170, 180));
  client.SetVisualRect(LayoutRect(100, 100, 300, 400));
  DrawRect(context, client, kBackgroundType, FloatRect(100, 100, 300, 400));
  GetPaintController().CommitNewDisplayItems();
  ASSERT_EQ(1u, GetPaintController().PaintChunks().size());
  EXPECT_THAT(GetPaintController().PaintChunks()[0].raster_invalidation_rects,
              // Both partial invalidation and incremental invalidation.
              UnorderedElementsAre(FloatRect(100, 400, 300, 100),
                                   FloatRect(150, 160, 170, 180)));
  EXPECT_EQ(LayoutRect(), client.PartialInvalidationRect());
}

TEST_P(PaintControllerTest, InvalidateAll) {
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  EXPECT_TRUE(GetPaintController().CacheIsAllInvalid());
  GetPaintController().CommitNewDisplayItems();
  EXPECT_TRUE(GetPaintController().GetPaintArtifact().IsEmpty());
  EXPECT_FALSE(GetPaintController().CacheIsAllInvalid());

  InvalidateAll();
  EXPECT_TRUE(GetPaintController().CacheIsAllInvalid());
  GetPaintController().CommitNewDisplayItems();
  EXPECT_TRUE(GetPaintController().GetPaintArtifact().IsEmpty());
  EXPECT_FALSE(GetPaintController().CacheIsAllInvalid());

  FakeDisplayItemClient client("client", LayoutRect(1, 2, 3, 4));
  GraphicsContext context(GetPaintController());
  DrawRect(context, client, kBackgroundType, FloatRect(1, 2, 3, 4));
  GetPaintController().CommitNewDisplayItems();
  EXPECT_FALSE(GetPaintController().GetPaintArtifact().IsEmpty());
  EXPECT_FALSE(GetPaintController().CacheIsAllInvalid());

  InvalidateAll();
  EXPECT_TRUE(GetPaintController().GetPaintArtifact().IsEmpty());
  EXPECT_TRUE(GetPaintController().CacheIsAllInvalid());
}

// Death tests don't work properly on Android.
#if defined(GTEST_HAS_DEATH_TEST) && !defined(OS_ANDROID)

class PaintControllerUnderInvalidationTest
    : public PaintControllerTestBase,
      private ScopedPaintUnderInvalidationCheckingForTest {
 public:
  PaintControllerUnderInvalidationTest()
      : PaintControllerTestBase(),
        ScopedPaintUnderInvalidationCheckingForTest(true) {}

 protected:
  void SetUp() override {
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  }

  void TestChangeDrawing() {
    FakeDisplayItemClient first("first");
    GraphicsContext context(GetPaintController());

    DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 300, 300));
    DrawRect(context, first, kForegroundType, FloatRect(100, 100, 300, 300));
    GetPaintController().CommitNewDisplayItems();
    DrawRect(context, first, kBackgroundType, FloatRect(200, 200, 300, 300));
    DrawRect(context, first, kForegroundType, FloatRect(100, 100, 300, 300));
    GetPaintController().CommitNewDisplayItems();
  }

  void TestMoreDrawing() {
    FakeDisplayItemClient first("first");
    GraphicsContext context(GetPaintController());

    DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 300, 300));
    GetPaintController().CommitNewDisplayItems();
    DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 300, 300));
    DrawRect(context, first, kForegroundType, FloatRect(100, 100, 300, 300));
    GetPaintController().CommitNewDisplayItems();
  }

  void TestLessDrawing() {
    FakeDisplayItemClient first("first");
    GraphicsContext context(GetPaintController());

    DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 300, 300));
    DrawRect(context, first, kForegroundType, FloatRect(100, 100, 300, 300));
    GetPaintController().CommitNewDisplayItems();
    DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 300, 300));
    GetPaintController().CommitNewDisplayItems();
  }

  void TestNoopPairsInSubsequence() {
    EXPECT_FALSE(GetPaintController().LastDisplayItemIsSubsequenceEnd());

    FakeDisplayItemClient container("container");
    GraphicsContext context(GetPaintController());

    {
      SubsequenceRecorder r(context, container);
      DrawRect(context, container, kBackgroundType,
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
      DrawRect(context, container, kBackgroundType,
               FloatRect(100, 100, 100, 100));
    }
    EXPECT_TRUE(GetPaintController().LastDisplayItemIsSubsequenceEnd());

    GetPaintController().CommitNewDisplayItems();
  }

  void TestChangeDrawingInSubsequence() {
    FakeDisplayItemClient first("first");
    GraphicsContext context(GetPaintController());
    {
      SubsequenceRecorder r(context, first);
      DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 300, 300));
      DrawRect(context, first, kForegroundType, FloatRect(100, 100, 300, 300));
    }
    GetPaintController().CommitNewDisplayItems();
    {
      EXPECT_FALSE(
          SubsequenceRecorder::UseCachedSubsequenceIfPossible(context, first));
      SubsequenceRecorder r(context, first);
      DrawRect(context, first, kBackgroundType, FloatRect(200, 200, 300, 300));
      DrawRect(context, first, kForegroundType, FloatRect(100, 100, 300, 300));
    }
    GetPaintController().CommitNewDisplayItems();
  }

  void TestMoreDrawingInSubsequence() {
    FakeDisplayItemClient first("first");
    GraphicsContext context(GetPaintController());

    {
      SubsequenceRecorder r(context, first);
      DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 300, 300));
    }
    GetPaintController().CommitNewDisplayItems();

    {
      EXPECT_FALSE(
          SubsequenceRecorder::UseCachedSubsequenceIfPossible(context, first));
      SubsequenceRecorder r(context, first);
      DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 300, 300));
      DrawRect(context, first, kForegroundType, FloatRect(100, 100, 300, 300));
    }
    GetPaintController().CommitNewDisplayItems();
  }

  void TestLessDrawingInSubsequence() {
    FakeDisplayItemClient first("first");
    GraphicsContext context(GetPaintController());

    {
      SubsequenceRecorder r(context, first);
      DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 300, 300));
      DrawRect(context, first, kForegroundType, FloatRect(100, 100, 300, 300));
    }
    GetPaintController().CommitNewDisplayItems();

    {
      EXPECT_FALSE(
          SubsequenceRecorder::UseCachedSubsequenceIfPossible(context, first));
      SubsequenceRecorder r(context, first);
      DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 300, 300));
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
      DrawRect(context, content, kBackgroundType,
               FloatRect(100, 100, 300, 300));
    }
    GetPaintController().CommitNewDisplayItems();

    {
      EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
          context, container));
      SubsequenceRecorder r(context, container);
      { ClipPathRecorder clip_path_recorder(context, container, Path()); }
      ClipRecorder clip(context, container, kClipType, IntRect(1, 1, 30, 30));
      DrawRect(context, content, kBackgroundType,
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
      DrawRect(context, content, kBackgroundType,
               FloatRect(100, 100, 300, 300));
    }
    GetPaintController().CommitNewDisplayItems();

    content.SetDisplayItemsUncached();
    // Leave container not invalidated.
    {
      EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
          context, container));
      SubsequenceRecorder r(context, container);
      DrawRect(context, content, kBackgroundType,
               FloatRect(100, 100, 300, 300));
    }
    GetPaintController().CommitNewDisplayItems();
  }

  void TestSubsequenceBecomesEmpty() {
    FakeDisplayItemClient target("target");
    GraphicsContext context(GetPaintController());

    {
      SubsequenceRecorder r(context, target);
      DrawRect(context, target, kBackgroundType, FloatRect(100, 100, 300, 300));
    }
    GetPaintController().CommitNewDisplayItems();

    {
      EXPECT_FALSE(
          SubsequenceRecorder::UseCachedSubsequenceIfPossible(context, target));
      SubsequenceRecorder r(context, target);
    }
    GetPaintController().CommitNewDisplayItems();
  }
};

TEST_F(PaintControllerUnderInvalidationTest, ChangeDrawing) {
  EXPECT_DEATH(TestChangeDrawing(), "");
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
  EXPECT_DEATH(TestChangeDrawingInSubsequence(), "");
}

TEST_F(PaintControllerUnderInvalidationTest, MoreDrawingInSubsequence) {
  EXPECT_DEATH(TestMoreDrawingInSubsequence(), "");
}

TEST_F(PaintControllerUnderInvalidationTest, LessDrawingInSubsequence) {
  // We allow invalidated display item clients as long as they would produce the
  // same display items. The cases of changed display items are tested by other
  // test cases.
  EXPECT_DEATH(TestLessDrawingInSubsequence(), "");
}

TEST_F(PaintControllerUnderInvalidationTest, ChangeNonCacheableInSubsequence) {
  EXPECT_DEATH(TestChangeNonCacheableInSubsequence(), "");
}

TEST_F(PaintControllerUnderInvalidationTest, InvalidationInSubsequence) {
  // We allow invalidated display item clients as long as they would produce the
  // same display items. The cases of changed display items are tested by other
  // test cases.
  TestInvalidationInSubsequence();
}

TEST_F(PaintControllerUnderInvalidationTest, SubsequenceBecomesEmpty) {
  EXPECT_DEATH(TestSubsequenceBecomesEmpty(), "");
}

TEST_F(PaintControllerUnderInvalidationTest, SkipCacheInSubsequence) {
  FakeDisplayItemClient container("container");
  FakeDisplayItemClient content("content");
  GraphicsContext context(GetPaintController());

  {
    SubsequenceRecorder r(context, container);
    {
      DisplayItemCacheSkipper cache_skipper(context);
      DrawRect(context, content, kBackgroundType,
               FloatRect(100, 100, 300, 300));
    }
    DrawRect(context, content, kForegroundType, FloatRect(200, 200, 400, 400));
  }
  GetPaintController().CommitNewDisplayItems();

  {
    EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
        context, container));
    SubsequenceRecorder r(context, container);
    {
      DisplayItemCacheSkipper cache_skipper(context);
      DrawRect(context, content, kBackgroundType,
               FloatRect(200, 200, 400, 400));
    }
    DrawRect(context, content, kForegroundType, FloatRect(200, 200, 400, 400));
  }
  GetPaintController().CommitNewDisplayItems();
}

TEST_F(PaintControllerUnderInvalidationTest,
       EmptySubsequenceInCachedSubsequence) {
  FakeDisplayItemClient container("container");
  FakeDisplayItemClient content("content");
  GraphicsContext context(GetPaintController());

  {
    SubsequenceRecorder r(context, container);
    DrawRect(context, container, kBackgroundType,
             FloatRect(100, 100, 300, 300));
    { SubsequenceRecorder r1(context, content); }
    DrawRect(context, container, kForegroundType,
             FloatRect(100, 100, 300, 300));
  }
  GetPaintController().CommitNewDisplayItems();

  {
    EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
        context, container));
    SubsequenceRecorder r(context, container);
    DrawRect(context, container, kBackgroundType,
             FloatRect(100, 100, 300, 300));
    EXPECT_FALSE(
        SubsequenceRecorder::UseCachedSubsequenceIfPossible(context, content));
    { SubsequenceRecorder r1(context, content); }
    DrawRect(context, container, kForegroundType,
             FloatRect(100, 100, 300, 300));
  }
  GetPaintController().CommitNewDisplayItems();
}

#endif  // defined(GTEST_HAS_DEATH_TEST) && !defined(OS_ANDROID)

}  // namespace blink
