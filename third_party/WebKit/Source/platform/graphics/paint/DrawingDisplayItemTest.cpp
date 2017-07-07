// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/DrawingDisplayItem.h"

#include "SkTypes.h"
#include "platform/graphics/paint/PaintRecorder.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/testing/FakeDisplayItemClient.h"
#include "public/platform/WebDisplayItemList.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

using ::testing::_;

class DrawingDisplayItemTest : public ::testing::Test {
 protected:
  FakeDisplayItemClient client_;
};

class MockWebDisplayItemList : public WebDisplayItemList {
 public:
  MOCK_METHOD3(AppendDrawingItem,
               void(const WebRect& visual_rect,
                    sk_sp<const cc::PaintRecord>,
                    const WebRect& record_bounds));
};

static sk_sp<PaintRecord> CreateRectRecord(const FloatRect& record_bounds) {
  PaintRecorder recorder;
  PaintCanvas* canvas =
      recorder.beginRecording(record_bounds.Width(), record_bounds.Height());
  canvas->drawRect(record_bounds, PaintFlags());
  return recorder.finishRecordingAsPicture();
}

TEST_F(DrawingDisplayItemTest, VisualRectAndDrawingBounds) {
  FloatRect record_bounds(5.5, 6.6, 7.7, 8.8);
  LayoutRect drawing_bounds(record_bounds);
  client_.SetVisualRect(drawing_bounds);

  DrawingDisplayItem item(client_, DisplayItem::Type::kDocumentBackground,
                          CreateRectRecord(record_bounds), record_bounds);
  EXPECT_EQ(drawing_bounds, item.VisualRect());

  MockWebDisplayItemList list1;
  WebRect expected_rect = EnclosingIntRect(drawing_bounds);
  WebRect expected_record_rect = EnclosingIntRect(record_bounds);
  EXPECT_CALL(list1, AppendDrawingItem(expected_rect, _, expected_record_rect))
      .Times(1);
  item.AppendToWebDisplayItemList(LayoutSize(), &list1);

  LayoutSize offset(LayoutUnit(2.1), LayoutUnit(3.6));
  LayoutRect visual_rect_with_offset = drawing_bounds;
  visual_rect_with_offset.Move(-offset);
  WebRect expected_visual_rect = EnclosingIntRect(visual_rect_with_offset);
  MockWebDisplayItemList list2;
  EXPECT_CALL(list2,
              AppendDrawingItem(expected_visual_rect, _, expected_record_rect))
      .Times(1);
  item.AppendToWebDisplayItemList(offset, &list2);
}

}  // namespace
}  // namespace blink
