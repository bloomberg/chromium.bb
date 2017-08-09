// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/compositing/PaintLayerCompositor.h"

#include "core/animation/Animation.h"
#include "core/animation/ElementAnimation.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/paint/PaintLayer.h"

namespace blink {

namespace {
class PaintLayerCompositorTest : public RenderingTest {
 public:
  PaintLayerCompositorTest()
      : RenderingTest(SingleChildLocalFrameClient::Create()) {}

 private:
  void SetUp() {
    RenderingTest::SetUp();
    EnableCompositing();
  }
};
}  // namespace

TEST_F(PaintLayerCompositorTest, AdvancingToCompositingInputsClean) {
  SetBodyInnerHTML("<div id='box' style='position: relative'></div>");

  PaintLayer* box_layer =
      ToLayoutBox(GetLayoutObjectByElementId("box"))->Layer();
  ASSERT_TRUE(box_layer);
  EXPECT_FALSE(box_layer->NeedsCompositingInputsUpdate());

  box_layer->SetNeedsCompositingInputsUpdate();

  GetDocument().View()->UpdateLifecycleToCompositingInputsClean();
  EXPECT_EQ(DocumentLifecycle::kCompositingInputsClean,
            GetDocument().Lifecycle().GetState());
  EXPECT_FALSE(box_layer->NeedsCompositingInputsUpdate());

  GetDocument().View()->SetNeedsLayout();
  EXPECT_TRUE(GetDocument().View()->NeedsLayout());
}

TEST_F(PaintLayerCompositorTest,
       CompositingInputsCleanDoesNotTriggerAnimations) {
  SetBodyInnerHTML(
      "<style>@keyframes fadeOut { from { opacity: 1; } to { opacity: 0; } }"
      ".animate { animation: fadeOut 2s; }</style>"
      "<div id='box'></div>"
      "<div id='otherBox'></div>");

  Element* box = GetDocument().getElementById("box");
  Element* otherBox = GetDocument().getElementById("otherBox");
  ASSERT_TRUE(box);
  ASSERT_TRUE(otherBox);

  box->setAttribute("class", "animate", ASSERT_NO_EXCEPTION);

  // Update the lifecycle to CompositingInputsClean. This should not start the
  // animation lifecycle.
  GetDocument().View()->UpdateLifecycleToCompositingInputsClean();
  EXPECT_EQ(DocumentLifecycle::kCompositingInputsClean,
            GetDocument().Lifecycle().GetState());

  otherBox->setAttribute("class", "animate", ASSERT_NO_EXCEPTION);

  // Now run the rest of the lifecycle. Because both 'box' and 'otherBox' were
  // given animations separated only by a lifecycle update to
  // CompositingInputsClean, they should both be started in the same lifecycle
  // and as such grouped together.
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(DocumentLifecycle::kPaintClean,
            GetDocument().Lifecycle().GetState());

  HeapVector<Member<Animation>> boxAnimations =
      ElementAnimation::getAnimations(*box);
  HeapVector<Member<Animation>> otherBoxAnimations =
      ElementAnimation::getAnimations(*box);

  EXPECT_EQ(1ul, boxAnimations.size());
  EXPECT_EQ(1ul, otherBoxAnimations.size());
  EXPECT_EQ(boxAnimations.front()->CompositorGroup(),
            otherBoxAnimations.front()->CompositorGroup());
}
}  // namespace blink
