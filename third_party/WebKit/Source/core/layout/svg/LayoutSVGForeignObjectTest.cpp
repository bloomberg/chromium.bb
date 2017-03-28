// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutGeometryMap.h"
#include "core/layout/LayoutTestHelper.h"

namespace blink {

class LayoutSVGForeignObjectTest : public RenderingTest {
 public:
  LayoutSVGForeignObjectTest()
      : RenderingTest(SingleChildLocalFrameClient::create()) {}

  const Node* hitTest(int x, int y) {
    HitTestResult result(
        HitTestRequest(HitTestRequest::ReadOnly | HitTestRequest::Active |
                       HitTestRequest::AllowChildFrameContent),
        IntPoint(x, y));
    layoutView().hitTest(result);
    return result.innerNode();
  }
};

TEST_F(LayoutSVGForeignObjectTest, DivInForeignObject) {
  setBodyInnerHTML(
      "<style>body { margin: 0 }</style>"
      "<svg id='svg' style='width: 500px; height: 400px'>"
      "  <foreignObject id='foreign' x='100' y='100' width='300' height='200'>"
      "    <div id='div' style='margin: 50px; width: 200px; height: 100px'>"
      "    </div>"
      "  </foreignObject>"
      "</svg>");

  const auto& svg = *document().getElementById("svg");
  const auto& foreignObject = *getLayoutObjectByElementId("foreign");
  const auto& div = *getLayoutObjectByElementId("div");

  EXPECT_EQ(FloatRect(100, 100, 300, 200), foreignObject.objectBoundingBox());
  EXPECT_EQ(AffineTransform(), foreignObject.localSVGTransform());
  EXPECT_EQ(AffineTransform(), foreignObject.localToSVGParentTransform());

  // mapToVisualRectInAncestorSpace
  LayoutRect divRect(0, 0, 100, 50);
  EXPECT_TRUE(div.mapToVisualRectInAncestorSpace(&layoutView(), divRect));
  EXPECT_EQ(LayoutRect(150, 150, 100, 50), divRect);

  // mapLocalToAncestor
  TransformState transformState(TransformState::ApplyTransformDirection,
                                FloatPoint());
  div.mapLocalToAncestor(&layoutView(), transformState,
                         TraverseDocumentBoundaries);
  transformState.flatten();
  EXPECT_EQ(FloatPoint(150, 150), transformState.lastPlanarPoint());

  // mapAncestorToLocal
  TransformState transformState1(
      TransformState::UnapplyInverseTransformDirection, FloatPoint());
  div.mapAncestorToLocal(&layoutView(), transformState1,
                         TraverseDocumentBoundaries);
  transformState1.flatten();
  EXPECT_EQ(FloatPoint(-150, -150), transformState1.lastPlanarPoint());

  // pushMappingToContainer
  LayoutGeometryMap rgm(TraverseDocumentBoundaries);
  rgm.pushMappingsToAncestor(&div, nullptr);
  EXPECT_EQ(FloatQuad(FloatRect(150, 150, 1, 2)),
            rgm.mapToAncestor(FloatRect(0, 0, 1, 2), nullptr));

  // Hit testing
  EXPECT_EQ(svg, hitTest(149, 149));
  EXPECT_EQ(div.node(), hitTest(150, 150));
  EXPECT_EQ(div.node(), hitTest(349, 249));
  EXPECT_EQ(svg, hitTest(350, 250));
}

TEST_F(LayoutSVGForeignObjectTest, IframeInForeignObject) {
  setBodyInnerHTML(
      "<style>body { margin: 0 }</style>"
      "<svg id='svg' style='width: 500px; height: 450px'>"
      "  <foreignObject id='foreign' x='100' y='100' width='300' height='250'>"
      "    <iframe style='border: none; margin: 30px;"
      "         width: 240px; height: 190px'></iframe>"
      "  </foreignObject>"
      "</svg>");
  setChildFrameHTML(
      "<style>"
      "  body { margin: 0 }"
      "  * { background: white; }"
      "</style>"
      "<div id='div' style='margin: 70px; width: 100px; height: 50px'></div>");
  document().view()->updateAllLifecyclePhases();

  const auto& svg = *document().getElementById("svg");
  const auto& foreignObject = *getLayoutObjectByElementId("foreign");
  const auto& div = *childDocument().getElementById("div")->layoutObject();

  EXPECT_EQ(FloatRect(100, 100, 300, 250), foreignObject.objectBoundingBox());
  EXPECT_EQ(AffineTransform(), foreignObject.localSVGTransform());
  EXPECT_EQ(AffineTransform(), foreignObject.localToSVGParentTransform());

  // mapToVisualRectInAncestorSpace
  LayoutRect divRect(0, 0, 100, 50);
  EXPECT_TRUE(div.mapToVisualRectInAncestorSpace(&layoutView(), divRect));
  EXPECT_EQ(LayoutRect(200, 200, 100, 50), divRect);

  // mapLocalToAncestor
  TransformState transformState(TransformState::ApplyTransformDirection,
                                FloatPoint());
  div.mapLocalToAncestor(&layoutView(), transformState,
                         TraverseDocumentBoundaries);
  transformState.flatten();
  EXPECT_EQ(FloatPoint(200, 200), transformState.lastPlanarPoint());

  // mapAncestorToLocal
  TransformState transformState1(
      TransformState::UnapplyInverseTransformDirection, FloatPoint());
  div.mapAncestorToLocal(&layoutView(), transformState1,
                         TraverseDocumentBoundaries);
  transformState1.flatten();
  EXPECT_EQ(FloatPoint(-200, -200), transformState1.lastPlanarPoint());

  // pushMappingToContainer
  LayoutGeometryMap rgm(TraverseDocumentBoundaries);
  rgm.pushMappingsToAncestor(&div, nullptr);
  EXPECT_EQ(FloatQuad(FloatRect(200, 200, 1, 2)),
            rgm.mapToAncestor(FloatRect(0, 0, 1, 2), nullptr));

  // Hit testing
  EXPECT_EQ(svg, hitTest(129, 129));
  EXPECT_EQ(childDocument().documentElement(), hitTest(130, 130));
  EXPECT_EQ(childDocument().documentElement(), hitTest(199, 199));
  EXPECT_EQ(div.node(), hitTest(200, 200));
  EXPECT_EQ(div.node(), hitTest(299, 249));
  EXPECT_EQ(childDocument().documentElement(), hitTest(300, 250));
  EXPECT_EQ(childDocument().documentElement(), hitTest(369, 319));
  EXPECT_EQ(svg, hitTest(370, 320));
}

}  // namespace blink
