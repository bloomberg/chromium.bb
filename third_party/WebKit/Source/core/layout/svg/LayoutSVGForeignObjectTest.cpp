// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutGeometryMap.h"
#include "core/layout/LayoutTestHelper.h"

namespace blink {

class LayoutSVGForeignObjectTest : public RenderingTest {
 public:
  LayoutSVGForeignObjectTest()
      : RenderingTest(SingleChildLocalFrameClient::Create()) {}

  const Node* HitTest(int x, int y) {
    HitTestResult result(
        HitTestRequest(HitTestRequest::kReadOnly | HitTestRequest::kActive |
                       HitTestRequest::kAllowChildFrameContent),
        IntPoint(x, y));
    GetLayoutView().HitTest(result);
    return result.InnerNode();
  }
};

TEST_F(LayoutSVGForeignObjectTest, DivInForeignObject) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <svg id='svg' style='width: 500px; height: 400px'>
      <foreignObject id='foreign' x='100' y='100' width='300' height='200'>
        <div id='div' style='margin: 50px; width: 200px; height: 100px'>
        </div>
      </foreignObject>
    </svg>
  )HTML");

  const auto& svg = *GetDocument().getElementById("svg");
  const auto& foreign_object = *GetLayoutObjectByElementId("foreign");
  const auto& div = *GetLayoutObjectByElementId("div");

  EXPECT_EQ(FloatRect(100, 100, 300, 200), foreign_object.ObjectBoundingBox());
  EXPECT_EQ(AffineTransform(), foreign_object.LocalSVGTransform());
  EXPECT_EQ(AffineTransform(), foreign_object.LocalToSVGParentTransform());

  // mapToVisualRectInAncestorSpace
  LayoutRect div_rect(0, 0, 100, 50);
  EXPECT_TRUE(div.MapToVisualRectInAncestorSpace(&GetLayoutView(), div_rect));
  EXPECT_EQ(LayoutRect(150, 150, 100, 50), div_rect);

  // mapLocalToAncestor
  TransformState transform_state(TransformState::kApplyTransformDirection,
                                 FloatPoint());
  div.MapLocalToAncestor(&GetLayoutView(), transform_state,
                         kTraverseDocumentBoundaries);
  transform_state.Flatten();
  EXPECT_EQ(FloatPoint(150, 150), transform_state.LastPlanarPoint());

  // mapAncestorToLocal
  TransformState transform_state1(
      TransformState::kUnapplyInverseTransformDirection, FloatPoint());
  div.MapAncestorToLocal(&GetLayoutView(), transform_state1,
                         kTraverseDocumentBoundaries);
  transform_state1.Flatten();
  EXPECT_EQ(FloatPoint(-150, -150), transform_state1.LastPlanarPoint());

  // pushMappingToContainer
  LayoutGeometryMap rgm(kTraverseDocumentBoundaries);
  rgm.PushMappingsToAncestor(&div, nullptr);
  EXPECT_EQ(FloatQuad(FloatRect(150, 150, 1, 2)),
            rgm.MapToAncestor(FloatRect(0, 0, 1, 2), nullptr));

  // Hit testing
  EXPECT_EQ(svg, HitTest(149, 149));
  EXPECT_EQ(div.GetNode(), HitTest(150, 150));
  EXPECT_EQ(div.GetNode(), HitTest(349, 249));
  EXPECT_EQ(svg, HitTest(350, 250));
}

TEST_F(LayoutSVGForeignObjectTest, IframeInForeignObject) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <svg id='svg' style='width: 500px; height: 450px'>
      <foreignObject id='foreign' x='100' y='100' width='300' height='250'>
        <iframe style='border: none; margin: 30px;
             width: 240px; height: 190px'></iframe>
      </foreignObject>
    </svg>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>
      body { margin: 0 }
      * { background: white; }
    </style>
    <div id='div' style='margin: 70px; width: 100px; height: 50px'></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  const auto& svg = *GetDocument().getElementById("svg");
  const auto& foreign_object = *GetLayoutObjectByElementId("foreign");
  const auto& div = *ChildDocument().getElementById("div")->GetLayoutObject();

  EXPECT_EQ(FloatRect(100, 100, 300, 250), foreign_object.ObjectBoundingBox());
  EXPECT_EQ(AffineTransform(), foreign_object.LocalSVGTransform());
  EXPECT_EQ(AffineTransform(), foreign_object.LocalToSVGParentTransform());

  // mapToVisualRectInAncestorSpace
  LayoutRect div_rect(0, 0, 100, 50);
  EXPECT_TRUE(div.MapToVisualRectInAncestorSpace(&GetLayoutView(), div_rect));
  EXPECT_EQ(LayoutRect(200, 200, 100, 50), div_rect);

  // mapLocalToAncestor
  TransformState transform_state(TransformState::kApplyTransformDirection,
                                 FloatPoint());
  div.MapLocalToAncestor(&GetLayoutView(), transform_state,
                         kTraverseDocumentBoundaries);
  transform_state.Flatten();
  EXPECT_EQ(FloatPoint(200, 200), transform_state.LastPlanarPoint());

  // mapAncestorToLocal
  TransformState transform_state1(
      TransformState::kUnapplyInverseTransformDirection, FloatPoint());
  div.MapAncestorToLocal(&GetLayoutView(), transform_state1,
                         kTraverseDocumentBoundaries);
  transform_state1.Flatten();
  EXPECT_EQ(FloatPoint(-200, -200), transform_state1.LastPlanarPoint());

  // pushMappingToContainer
  LayoutGeometryMap rgm(kTraverseDocumentBoundaries);
  rgm.PushMappingsToAncestor(&div, nullptr);
  EXPECT_EQ(FloatQuad(FloatRect(200, 200, 1, 2)),
            rgm.MapToAncestor(FloatRect(0, 0, 1, 2), nullptr));

  // Hit testing
  EXPECT_EQ(svg, HitTest(129, 129));
  EXPECT_EQ(ChildDocument().documentElement(), HitTest(130, 130));
  EXPECT_EQ(ChildDocument().documentElement(), HitTest(199, 199));
  EXPECT_EQ(div.GetNode(), HitTest(200, 200));
  EXPECT_EQ(div.GetNode(), HitTest(299, 249));
  EXPECT_EQ(ChildDocument().documentElement(), HitTest(300, 250));
  EXPECT_EQ(ChildDocument().documentElement(), HitTest(369, 319));
  EXPECT_EQ(svg, HitTest(370, 320));
}

}  // namespace blink
