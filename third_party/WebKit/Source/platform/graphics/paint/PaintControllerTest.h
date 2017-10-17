// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintControllerTest_h
#define PaintControllerTest_h

#include "platform/graphics/paint/ClipRecorder.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/PaintController.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class GraphicsContext;

class PaintControllerTestBase : public ::testing::Test {
 public:
  PaintControllerTestBase() : paint_controller_(PaintController::Create()) {}

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

  void InvalidateAll() { paint_controller_->InvalidateAllForTesting(); }

  using SubsequenceMarkers = PaintController::SubsequenceMarkers;
  SubsequenceMarkers* GetSubsequenceMarkers(const DisplayItemClient& client) {
    return paint_controller_->GetSubsequenceMarkers(client);
  }

  template <typename Rect>
  static void DrawNothing(GraphicsContext& context,
                          const DisplayItemClient& client,
                          DisplayItem::Type type,
                          const Rect& bounds) {
    if (DrawingRecorder::UseCachedDrawingIfPossible(context, client, type))
      return;
    DrawingRecorder recorder(context, client, type, bounds);
  }

  template <typename Rect>
  static void DrawRect(GraphicsContext& context,
                       const DisplayItemClient& client,
                       DisplayItem::Type type,
                       const Rect& bounds) {
    if (DrawingRecorder::UseCachedDrawingIfPossible(context, client, type))
      return;
    DrawingRecorder recorder(context, client, type, bounds);
    context.DrawRect(RoundedIntRect(FloatRect(bounds)));
  }

 private:
  std::unique_ptr<PaintController> paint_controller_;
};

class TestDisplayItem final : public DisplayItem {
 public:
  TestDisplayItem(const DisplayItemClient& client, Type type)
      : DisplayItem(client, type, sizeof(*this)) {}

  void Replay(GraphicsContext&) const final { NOTREACHED(); }
  void AppendToWebDisplayItemList(const LayoutSize&,
                                  WebDisplayItemList*) const final {
    NOTREACHED();
  }
};

#define EXPECT_DISPLAY_LIST(actual, expected_size, ...)                   \
  do {                                                                    \
    EXPECT_EQ((size_t)expected_size, actual.size());                      \
    if (expected_size != actual.size())                                   \
      break;                                                              \
    const TestDisplayItem expected[] = {__VA_ARGS__};                     \
    for (size_t i = 0; i < expected_size; ++i) {                          \
      SCOPED_TRACE(                                                       \
          String::Format("%d: Expected:(client=%p:\"%s\" type=%d) "       \
                         "Actual:(client=%p:%s type=%d)",                 \
                         (int)i, &expected[i].Client(),                   \
                         expected[i].Client().DebugName().Ascii().data(), \
                         (int)expected[i].GetType(), &actual[i].Client(), \
                         actual[i].Client().DebugName().Ascii().data(),   \
                         (int)actual[i].GetType()));                      \
      EXPECT_EQ(&expected[i].Client(), &actual[i].Client());              \
      EXPECT_EQ(expected[i].GetType(), actual[i].GetType());              \
    }                                                                     \
  } while (false);

// Shorter names for frequently used display item types in tests.
const DisplayItem::Type kBackgroundType = DisplayItem::kBoxDecorationBackground;
const DisplayItem::Type kForegroundType =
    static_cast<DisplayItem::Type>(DisplayItem::kDrawingPaintPhaseFirst + 4);
const DisplayItem::Type kDocumentBackgroundType =
    DisplayItem::kDocumentBackground;
const DisplayItem::Type kScrollHitTestType = DisplayItem::kScrollHitTest;
const DisplayItem::Type kClipType = DisplayItem::kClipFirst;

}  // namespace blink

#endif  // PaintControllerTest_h
