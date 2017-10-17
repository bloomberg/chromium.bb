// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/DrawingRecorder.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintControllerTest.h"
#include "platform/testing/FakeDisplayItemClient.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using DrawingRecorderTest = PaintControllerTestBase;

namespace {

const IntRect kBounds(1, 2, 3, 4);

TEST_F(DrawingRecorderTest, Nothing) {
  FakeDisplayItemClient client;
  GraphicsContext context(GetPaintController());
  DrawNothing(context, client, kForegroundType, kBounds);
  GetPaintController().CommitNewDisplayItems();
  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 1,
                      TestDisplayItem(client, kForegroundType));
  EXPECT_FALSE(static_cast<const DrawingDisplayItem&>(
                   GetPaintController().GetDisplayItemList()[0])
                   .GetPaintRecord());
}

TEST_F(DrawingRecorderTest, Rect) {
  FakeDisplayItemClient client;
  GraphicsContext context(GetPaintController());
  DrawRect(context, client, kForegroundType, kBounds);
  GetPaintController().CommitNewDisplayItems();
  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 1,
                      TestDisplayItem(client, kForegroundType));
}

TEST_F(DrawingRecorderTest, Cached) {
  FakeDisplayItemClient client;
  GraphicsContext context(GetPaintController());
  DrawNothing(context, client, kBackgroundType, kBounds);
  DrawRect(context, client, kForegroundType, kBounds);
  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(client, kBackgroundType),
                      TestDisplayItem(client, kForegroundType));

  DrawNothing(context, client, kBackgroundType, kBounds);
  DrawRect(context, client, kForegroundType, kBounds);

  EXPECT_EQ(2, NumCachedNewItems());

  GetPaintController().CommitNewDisplayItems();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(client, kBackgroundType),
                      TestDisplayItem(client, kForegroundType));
}

template <typename T>
FloatRect DrawAndGetCullRect(PaintController& controller,
                             const DisplayItemClient& client,
                             const T& bounds) {
  {
    // Draw some things which will produce a non-null picture.
    GraphicsContext context(controller);
    DrawingRecorder recorder(context, client, kBackgroundType, bounds);
    context.DrawRect(EnclosedIntRect(FloatRect(bounds)));
  }
  controller.CommitNewDisplayItems();
  const auto& drawing = static_cast<const DrawingDisplayItem&>(
      controller.GetDisplayItemList()[0]);
  return drawing.GetPaintRecordBounds();
}

TEST_F(DrawingRecorderTest, CullRectMatchesProvidedClip) {
  // It's safe for the picture's cull rect to be expanded (though doing so
  // excessively may harm performance), but it cannot be contracted.
  // For now, this test expects the two rects to match completely.
  //
  // This rect is chosen so that in the x direction, pixel snapping rounds in
  // the opposite direction to enclosing, and in the y direction, the edges
  // are exactly on a half-pixel boundary. The numbers chosen map nicely to
  // both float and LayoutUnit, to make equality checking reliable.
  //
  // The final cull rect should be the enclosing int rect of this rect.
  FakeDisplayItemClient client;
  FloatRect rect(20.75, -5.5, 5.375, 10);
  EXPECT_EQ(EnclosingIntRect(rect),
            DrawAndGetCullRect(GetPaintController(), client, rect));
  InvalidateAll();
  EXPECT_EQ(EnclosingIntRect(rect),
            DrawAndGetCullRect(GetPaintController(), client, LayoutRect(rect)));
}

}  // namespace
}  // namespace blink
