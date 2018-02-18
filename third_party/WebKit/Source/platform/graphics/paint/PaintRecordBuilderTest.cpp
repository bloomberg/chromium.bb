// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintRecordBuilder.h"

#include "platform/graphics/paint/PaintControllerTest.h"
#include "platform/graphics/test/MockPaintCanvas.h"
#include "platform/testing/FakeDisplayItemClient.h"

using testing::_;

namespace blink {

using PaintRecordBuilderTest = PaintControllerTestBase;

TEST_F(PaintRecordBuilderTest, TransientPaintController) {
  PaintRecordBuilder builder;
  auto& context = builder.Context();
  FakeDisplayItemClient client("client", LayoutRect(10, 10, 20, 20));
  DrawRect(context, client, kBackgroundType, FloatRect(10, 10, 20, 20));
  DrawRect(context, client, kForegroundType, FloatRect(15, 15, 10, 10));
  EXPECT_FALSE(context.GetPaintController().ClientCacheIsValid(client));

  MockPaintCanvas canvas;
  PaintFlags flags;
  EXPECT_CALL(canvas, drawPicture(_)).Times(1);
  builder.EndRecording(canvas);

  EXPECT_DISPLAY_LIST(context.GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(client, kBackgroundType),
                      TestDisplayItem(client, kForegroundType));
  EXPECT_FALSE(context.GetPaintController().ClientCacheIsValid(client));
}

TEST_F(PaintRecordBuilderTest, LastingPaintController) {
  if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        WTF::nullopt, PaintChunkProperties(PropertyTreeState::Root()));
  }

  PaintRecordBuilder builder(nullptr, nullptr, &GetPaintController());
  auto& context = builder.Context();
  EXPECT_EQ(&context.GetPaintController(), &GetPaintController());

  FakeDisplayItemClient client("client", LayoutRect(10, 10, 20, 20));
  DrawRect(context, client, kBackgroundType, FloatRect(10, 10, 20, 20));
  DrawRect(context, client, kForegroundType, FloatRect(15, 15, 10, 10));
  EXPECT_FALSE(GetPaintController().ClientCacheIsValid(client));

  MockPaintCanvas canvas;
  PaintFlags flags;
  EXPECT_CALL(canvas, drawPicture(_)).Times(1);
  builder.EndRecording(canvas);
  EXPECT_TRUE(GetPaintController().ClientCacheIsValid(client));

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(client, kBackgroundType),
                      TestDisplayItem(client, kForegroundType));

  if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled()) {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        WTF::nullopt, PaintChunkProperties(PropertyTreeState::Root()));
  }

  EXPECT_TRUE(DrawingRecorder::UseCachedDrawingIfPossible(context, client,
                                                          kBackgroundType));
  EXPECT_TRUE(DrawingRecorder::UseCachedDrawingIfPossible(context, client,
                                                          kForegroundType));
  EXPECT_CALL(canvas, drawPicture(_)).Times(1);
  builder.EndRecording(canvas);

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(client, kBackgroundType),
                      TestDisplayItem(client, kForegroundType));
  EXPECT_TRUE(GetPaintController().ClientCacheIsValid(client));
}

TEST_F(PaintRecordBuilderTest, TransientAndAnotherPaintController) {
  GraphicsContext context(GetPaintController());

  InitRootChunk();
  FakeDisplayItemClient client("client", LayoutRect(10, 10, 20, 20));
  DrawRect(context, client, kBackgroundType, FloatRect(10, 10, 20, 20));
  DrawRect(context, client, kForegroundType, FloatRect(15, 15, 10, 10));
  GetPaintController().CommitNewDisplayItems();
  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(client, kBackgroundType),
                      TestDisplayItem(client, kForegroundType));
  EXPECT_TRUE(GetPaintController().ClientCacheIsValid(client));

  PaintRecordBuilder builder;
  EXPECT_NE(&builder.Context().GetPaintController(), &GetPaintController());
  DrawRect(builder.Context(), client, kBackgroundType,
           FloatRect(10, 10, 20, 20));
  builder.EndRecording();

  // The transient PaintController in PaintRecordBuilder doesn't affect the
  // client's cache status in another PaintController.
  EXPECT_TRUE(GetPaintController().ClientCacheIsValid(client));
  EXPECT_FALSE(
      builder.Context().GetPaintController().ClientCacheIsValid(client));
}

}  // namespace blink
