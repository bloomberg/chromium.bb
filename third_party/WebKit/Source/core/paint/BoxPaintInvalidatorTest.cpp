// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/HTMLNames.h"
#include "core/frame/FrameView.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/RasterInvalidationTracking.h"

namespace blink {

class BoxPaintInvalidatorTest : public RenderingTest {
 protected:
  const RasterInvalidationTracking* getRasterInvalidationTracking() const {
    // TODO(wangxianzhu): Test SPv2.
    return layoutView()
        .layer()
        ->graphicsLayerBacking()
        ->getRasterInvalidationTracking();
  }

 private:
  void SetUp() override {
    RenderingTest::SetUp();
    enableCompositing();
    setBodyInnerHTML(
        "<style>"
        "  body { margin: 0 }"
        "  #target {"
        "    width: 50px;"
        "    height: 100px;"
        "    border-width: 20px 10px;"
        "    border-style: solid;"
        "    border-color: red;"
        "  }"
        "</style>"
        "<div id='target'></div>");
  }
};

TEST_F(BoxPaintInvalidatorTest, IncrementalInvalidationExpand) {
  document().view()->setTracksPaintInvalidations(true);
  Element* target = document().getElementById("target");
  target->setAttribute(HTMLNames::styleAttr, "width: 100px; height: 200px");
  document().view()->updateAllLifecyclePhases();
  const auto* rasterInvalidations =
      &getRasterInvalidationTracking()->trackedRasterInvalidations;
  EXPECT_EQ(2u, rasterInvalidations->size());
  EXPECT_EQ(IntRect(60, 0, 60, 240), (*rasterInvalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationIncremental, (*rasterInvalidations)[0].reason);
  EXPECT_EQ(IntRect(0, 120, 120, 120), (*rasterInvalidations)[1].rect);
  EXPECT_EQ(PaintInvalidationIncremental, (*rasterInvalidations)[1].reason);
  document().view()->setTracksPaintInvalidations(false);
}

TEST_F(BoxPaintInvalidatorTest, IncrementalInvalidationShrink) {
  document().view()->setTracksPaintInvalidations(true);
  Element* target = document().getElementById("target");
  target->setAttribute(HTMLNames::styleAttr, "width: 20px; height: 80px");
  document().view()->updateAllLifecyclePhases();
  const auto* rasterInvalidations =
      &getRasterInvalidationTracking()->trackedRasterInvalidations;
  EXPECT_EQ(2u, rasterInvalidations->size());
  EXPECT_EQ(IntRect(30, 0, 40, 140), (*rasterInvalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationIncremental, (*rasterInvalidations)[0].reason);
  EXPECT_EQ(IntRect(0, 100, 70, 40), (*rasterInvalidations)[1].rect);
  EXPECT_EQ(PaintInvalidationIncremental, (*rasterInvalidations)[1].reason);
  document().view()->setTracksPaintInvalidations(false);
}

TEST_F(BoxPaintInvalidatorTest, IncrementalInvalidationMixed) {
  document().view()->setTracksPaintInvalidations(true);
  Element* target = document().getElementById("target");
  target->setAttribute(HTMLNames::styleAttr, "width: 100px; height: 80px");
  document().view()->updateAllLifecyclePhases();
  const auto* rasterInvalidations =
      &getRasterInvalidationTracking()->trackedRasterInvalidations;
  EXPECT_EQ(2u, rasterInvalidations->size());
  EXPECT_EQ(IntRect(60, 0, 60, 120), (*rasterInvalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationIncremental, (*rasterInvalidations)[0].reason);
  EXPECT_EQ(IntRect(0, 100, 70, 40), (*rasterInvalidations)[1].rect);
  EXPECT_EQ(PaintInvalidationIncremental, (*rasterInvalidations)[1].reason);
  document().view()->setTracksPaintInvalidations(false);
}

}  // namespace blink
