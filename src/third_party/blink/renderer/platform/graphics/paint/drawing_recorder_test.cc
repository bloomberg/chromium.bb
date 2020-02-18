// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller_test.h"
#include "third_party/blink/renderer/platform/testing/fake_display_item_client.h"

using testing::ElementsAre;

namespace blink {

using DrawingRecorderTest = PaintControllerTestBase;

namespace {

const IntRect kBounds(1, 2, 3, 4);

TEST_F(DrawingRecorderTest, Nothing) {
  FakeDisplayItemClient client;
  GraphicsContext context(GetPaintController());
  InitRootChunk();
  DrawNothing(context, client, kForegroundType);
  CommitAndFinishCycle();
  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&client, kForegroundType)));
  EXPECT_FALSE(static_cast<const DrawingDisplayItem&>(
                   GetPaintController().GetDisplayItemList()[0])
                   .GetPaintRecord());
}

TEST_F(DrawingRecorderTest, Rect) {
  FakeDisplayItemClient client;
  GraphicsContext context(GetPaintController());
  InitRootChunk();
  DrawRect(context, client, kForegroundType, kBounds);
  CommitAndFinishCycle();
  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&client, kForegroundType)));
}

TEST_F(DrawingRecorderTest, Cached) {
  FakeDisplayItemClient client;
  GraphicsContext context(GetPaintController());
  InitRootChunk();
  DrawNothing(context, client, kBackgroundType);
  DrawRect(context, client, kForegroundType, kBounds);
  CommitAndFinishCycle();

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&client, kBackgroundType),
                          IsSameId(&client, kForegroundType)));

  InitRootChunk();
  DrawNothing(context, client, kBackgroundType);
  DrawRect(context, client, kForegroundType, kBounds);

  EXPECT_EQ(2u, NumCachedNewItems());

  CommitAndFinishCycle();

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&client, kBackgroundType),
                          IsSameId(&client, kForegroundType)));
}

}  // namespace
}  // namespace blink
