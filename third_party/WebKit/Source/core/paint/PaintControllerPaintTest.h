// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintControllerPaintTest_h
#define PaintControllerPaintTest_h

#include "core/editing/FrameSelection.h"
#include "core/frame/LocalFrameView.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/CullRect.h"
#include "platform/graphics/paint/PaintControllerTest.h"
#include "platform/testing/PaintTestConfigurations.h"

namespace blink {

class PaintControllerPaintTestBase : public RenderingTest {
 public:
  PaintControllerPaintTestBase(LocalFrameClient* local_frame_client = nullptr)
      : RenderingTest(local_frame_client) {}

 protected:
  LayoutView& GetLayoutView() const { return *GetDocument().GetLayoutView(); }
  PaintController& RootPaintController() const {
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
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
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      if (GetLayoutView().Layer()->NeedsRepaint()) {
        GraphicsContext graphics_context(RootPaintController());
        GetDocument().View()->Paint(graphics_context, kGlobalPaintNormalPhase,
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
                               DisplayItem::Type type) const {
    for (auto& item : display_item_list) {
      if (item.Client() == client && item.GetType() == type)
        return true;
    }
    return false;
  }

  int NumCachedNewItems() const {
    return RootPaintController().num_cached_new_items_;
  }

  const DisplayItemClient& CaretDisplayItemClientForTesting() const {
    return GetDocument()
        .GetFrame()
        ->Selection()
        .CaretDisplayItemClientForTesting();
  }

  void InvalidateAll(PaintController& paint_controller) {
    paint_controller.InvalidateAllForTesting();
  }
};

class PaintControllerPaintTest : public PaintTestConfigurations,
                                 public PaintControllerPaintTestBase {
 public:
  PaintControllerPaintTest(LocalFrameClient* local_frame_client = nullptr)
      : PaintControllerPaintTestBase(local_frame_client) {}
};

}  // namespace blink

#endif  // PaintControllerPaintTest_h
