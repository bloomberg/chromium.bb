// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintControllerPaintTest_h
#define PaintControllerPaintTest_h

#include "core/frame/FrameView.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/CullRect.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include <gtest/gtest.h>

namespace blink {

class PaintControllerPaintTestBase : private ScopedSlimmingPaintV2ForTest,
                                     public RenderingTest {
 public:
  PaintControllerPaintTestBase(bool enable_slimming_paint_v2)
      : ScopedSlimmingPaintV2ForTest(enable_slimming_paint_v2) {}

 protected:
  LayoutView& GetLayoutView() { return *GetDocument().GetLayoutView(); }
  PaintController& RootPaintController() {
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
      return *GetDocument().View()->GetPaintController();
    return GetLayoutView()
        .Layer()
        ->GraphicsLayerBacking()
        ->GetPaintController();
  }

  void SetUp() override {
    RenderingTest::SetUp();
    EnableCompositing();
  }

  bool PaintWithoutCommit(const IntRect* interest_rect = nullptr) {
    GetDocument().View()->Lifecycle().AdvanceTo(DocumentLifecycle::kInPaint);
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      if (GetLayoutView().Layer()->NeedsRepaint()) {
        GraphicsContext graphics_context(RootPaintController());
        GetDocument().View()->Paint(graphics_context,
                                    CullRect(LayoutRect::InfiniteIntRect()));
        return true;
      }
      GetDocument().View()->Lifecycle().AdvanceTo(
          DocumentLifecycle::kPaintClean);
      return false;
    }
    // Only root graphics layer is supported.
    if (!GetLayoutView().Layer()->GraphicsLayerBacking()->PaintWithoutCommit(
            interest_rect)) {
      GetDocument().View()->Lifecycle().AdvanceTo(
          DocumentLifecycle::kPaintClean);
      return false;
    }
    return true;
  }

  void Commit() {
    // Only root graphics layer is supported.
    RootPaintController().CommitNewDisplayItems();
    GetDocument().View()->Lifecycle().AdvanceTo(DocumentLifecycle::kPaintClean);
  }

  void Paint(const IntRect* interest_rect = nullptr) {
    // Only root graphics layer is supported.
    if (PaintWithoutCommit(interest_rect))
      Commit();
  }

  bool DisplayItemListContains(const DisplayItemList& display_item_list,
                               DisplayItemClient& client,
                               DisplayItem::Type type) {
    for (auto& item : display_item_list) {
      if (item.Client() == client && item.GetType() == type)
        return true;
    }
    return false;
  }

  int NumCachedNewItems() {
    return RootPaintController().num_cached_new_items_;
  }
};

class PaintControllerPaintTest : public PaintControllerPaintTestBase {
 public:
  PaintControllerPaintTest() : PaintControllerPaintTestBase(false) {}
};

class PaintControllerPaintTestForSlimmingPaintV2
    : public PaintControllerPaintTestBase,
      public testing::WithParamInterface<bool>,
      private ScopedRootLayerScrollingForTest {
 public:
  PaintControllerPaintTestForSlimmingPaintV2()
      : PaintControllerPaintTestBase(true),
        ScopedRootLayerScrollingForTest(GetParam()) {}
};

class PaintControllerPaintTestForSlimmingPaintV1AndV2
    : public PaintControllerPaintTestBase,
      public testing::WithParamInterface<bool> {
 public:
  PaintControllerPaintTestForSlimmingPaintV1AndV2()
      : PaintControllerPaintTestBase(GetParam()) {}
};

class TestDisplayItem final : public DisplayItem {
 public:
  TestDisplayItem(const DisplayItemClient& client, Type type)
      : DisplayItem(client, type, sizeof(*this)) {}

  void Replay(GraphicsContext&) const final { ASSERT_NOT_REACHED(); }
  void AppendToWebDisplayItemList(const IntRect&,
                                  WebDisplayItemList*) const final {
    ASSERT_NOT_REACHED();
  }
};

#ifndef NDEBUG
#define TRACE_DISPLAY_ITEMS(i, expected, actual)             \
  String trace = String::Format("%d: ", (int)i) +            \
                 "Expected: " + (expected).AsDebugString() + \
                 " Actual: " + (actual).AsDebugString();     \
  SCOPED_TRACE(trace.Utf8().data());
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

// Shorter names for frequently used display item types in tests.
const DisplayItem::Type kBackgroundType = DisplayItem::kBoxDecorationBackground;
const DisplayItem::Type kForegroundType =
    DisplayItem::PaintPhaseToDrawingType(kPaintPhaseForeground);
const DisplayItem::Type kDocumentBackgroundType =
    DisplayItem::kDocumentBackground;

}  // namespace blink

#endif  // PaintControllerPaintTest_h
