// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintLayer.h"
#include "core/paint/compositing/CompositedLayerMapping.h"
#include "core/paint/compositing/CompositingLayerPropertyUpdater.h"
#include "core/testing/CoreUnitTestHelper.h"
#include "platform/graphics/GraphicsLayer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class CompositingLayerPropertyUpdaterTest : public RenderingTest {
 private:
  void SetUp() override {
    RenderingTest::SetUp();
    EnableCompositing();
  }
};

TEST_F(CompositingLayerPropertyUpdaterTest, MaskLayerState) {
  SetBodyInnerHTML(R"HTML(
    <div id=target style="position: absolute;
        clip-path: polygon(-1px -1px, 86px 400px);
        clip: rect(9px, -1px, -1px, 96px); will-change: transform">
    </div>
    )HTML");

  PaintLayer* target =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("target"))->Layer();
  EXPECT_EQ(kPaintsIntoOwnBacking, target->GetCompositingState());
  auto* mapping = target->GetCompositedLayerMapping();
  auto* mask_layer = mapping->MaskLayer();

  auto* paint_properties =
      target->GetLayoutObject().FirstFragment().PaintProperties();
  EXPECT_TRUE(paint_properties->CssClip());
  EXPECT_TRUE(paint_properties->MaskClip());
  EXPECT_EQ(paint_properties->MaskClip(),
            mask_layer->layer_state_->state.Clip());
}

}  // namespace blink
