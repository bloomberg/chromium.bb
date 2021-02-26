// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"

namespace blink {

class CompositedLayerAssignerTest : public RenderingTest {
 public:
  CompositedLayerAssignerTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

 private:
  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }
};

TEST_F(CompositedLayerAssignerTest, SquashingSimple) {
  SetBodyInnerHTML(R"HTML(
    <div>
      <div style="width: 20px; height: 20px; will-change: transform"></div>
    </div>
    <div id="squashed" style="position: absolute; top: 0; width: 100px;
        height: 100px; background: green"></div>
    )HTML");

  PaintLayer* squashed = GetPaintLayerByElementId("squashed");
  EXPECT_EQ(kPaintsIntoGroupedBacking, squashed->GetCompositingState());
  CompositedLayerMapping* mapping = squashed->GroupedMapping();
  EXPECT_EQ(mapping->NonScrollingSquashingLayer(),
            mapping->SquashingLayer(*squashed));
  EXPECT_EQ(mapping->NonScrollingSquashingLayer(),
            squashed->GraphicsLayerBacking());
}

TEST_F(CompositedLayerAssignerTest, SquashingAcrossClipPathDisallowed) {
  SetBodyInnerHTML(R"HTML(
    <div style="clip-path: circle(100%)">
      <div style="width: 20px; height: 20px; will-change: transform"></div>
    </div>
    <div id="squashed" style="position: absolute; top: 0; width: 100px;
        height: 100px; background: green"></div>
    )HTML");
  // #squashed should not be squashed after all, because of the clip path above
  // #squashing.
  PaintLayer* squashed = GetPaintLayerByElementId("squashed");
  EXPECT_EQ(kPaintsIntoOwnBacking, squashed->GetCompositingState());
}

TEST_F(CompositedLayerAssignerTest, SquashingAcrossMaskDisallowed) {
  SetBodyInnerHTML(R"HTML(
    <div style="-webkit-mask-image: linear-gradient(black, white);">
      <div style="width: 20px; height: 20px; will-change: transform"></div>
    </div>
    <div id="squashed" style="position: absolute; top: 0; width: 100px;
        height: 100px; background: green"></div>
    )HTML");
  // #squashed should not be squashed after all, because of the mask above
  // #squashing.
  PaintLayer* squashed = GetPaintLayerByElementId("squashed");
  EXPECT_EQ(kPaintsIntoOwnBacking, squashed->GetCompositingState());
}

TEST_F(CompositedLayerAssignerTest,
       SquashingAcrossLayoutContainmentDisallowed) {
  SetBodyInnerHTML(R"HTML(
    <div style="contain: layout">
      <div style="width: 20px; height: 20px; will-change: transform"></div>
    </div>
    <div id="squashed" style="position: absolute; top: 0; width: 100px;
        height: 100px; background: green"></div>
    )HTML");
  // #squashed should not be squashed after all, because of 'contain: layout' on
  // #squashing.
  PaintLayer* squashed = GetPaintLayerByElementId("squashed");
  EXPECT_EQ(kPaintsIntoOwnBacking, squashed->GetCompositingState());
}

TEST_F(CompositedLayerAssignerTest,
       SquashingAcrossSelfLayoutContainmentDisallowed) {
  SetBodyInnerHTML(R"HTML(
    <div style="width: 20px; height: 20px; will-change: transform"></div>
    <div id="squashed" style="contain: layout; position: absolute; top: 0; width: 100px;
        height: 100px; background: green"></div>
    )HTML");
  // #squashed should not be squashed after all, because of 'contain: layout' on
  // #squahed.
  PaintLayer* squashed = GetPaintLayerByElementId("squashed");
  EXPECT_EQ(kPaintsIntoOwnBacking, squashed->GetCompositingState());
}

TEST_F(CompositedLayerAssignerTest,
       SquashingAcrossCompositedInnerDocumentDisallowed) {
  SetBodyInnerHTML(R"HTML(
    <div id="bottom" style="position: absolute; will-change: transform">Bottom</div>
    <div id="middle" style="position: absolute">
      <iframe style="border: 10px solid magenta"></iframe>
    </div>
    <div id="top" style="position: absolute; width: 200px; height: 200px; background: green;">Top</div>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>body {will-change: transform; background: blue}</style>
  )HTML");
  LocalFrameView* frame_view = GetDocument().View();
  frame_view->UpdateAllLifecyclePhasesForTest();
  PaintLayer* top = GetPaintLayerByElementId("top");
  EXPECT_EQ(kPaintsIntoOwnBacking, top->GetCompositingState());
}

}  // namespace blink
