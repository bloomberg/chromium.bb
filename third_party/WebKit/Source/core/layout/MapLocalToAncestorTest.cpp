// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "platform/geometry/TransformState.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class MapLocalToAncestorTest : public RenderingTest {
public:
    FloatPoint mapLocalToAncestor(const LayoutObject*, const LayoutBoxModelObject* ancestor, FloatPoint, MapCoordinatesFlags = 0) const;
    FloatQuad mapLocalToAncestor(const LayoutObject*, const LayoutBoxModelObject* ancestor, FloatQuad, MapCoordinatesFlags = 0) const;
};

// One note about tests here that operate on LayoutInline and LayoutText objects:
// mapLocalToAncestor() expects such objects to pass their static location and size (relatively to
// the border edge of their container) to mapLocalToAncestor() via the TransformState
// argument. mapLocalToAncestor() is then only expected to make adjustments for
// relative-positioning, container-specific characteristics (such as writing mode roots, multicol),
// and so on. This in contrast to LayoutBox objects, where the TransformState passed is relative to
// the box itself, not the container.

FloatPoint MapLocalToAncestorTest::mapLocalToAncestor(const LayoutObject* object, const LayoutBoxModelObject* ancestor, FloatPoint point, MapCoordinatesFlags mode) const
{
    TransformState transformState(TransformState::ApplyTransformDirection, point);
    object->mapLocalToAncestor(ancestor, transformState, mode);
    transformState.flatten();
    return transformState.lastPlanarPoint();
}

FloatQuad MapLocalToAncestorTest::mapLocalToAncestor(const LayoutObject* object, const LayoutBoxModelObject* ancestor, FloatQuad quad, MapCoordinatesFlags mode) const
{
    TransformState transformState(TransformState::ApplyTransformDirection, quad.boundingBox().center(), quad);
    object->mapLocalToAncestor(ancestor, transformState, mode);
    transformState.flatten();
    return transformState.lastPlanarQuad();
}

TEST_F(MapLocalToAncestorTest, SimpleText)
{
    setBodyInnerHTML("<div id='container'><br>text</div>");

    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));
    LayoutObject* text = toLayoutBlockFlow(container)->lastChild();
    ASSERT_TRUE(text->isText());
    FloatPoint mappedPoint = mapLocalToAncestor(text, container, FloatPoint(10, 30));
    EXPECT_EQ(FloatPoint(10, 30), mappedPoint);
}

TEST_F(MapLocalToAncestorTest, SimpleInline)
{
    setBodyInnerHTML("<div><span id='target'>text</span></div>");

    LayoutObject* target = getLayoutObjectByElementId("target");
    FloatPoint mappedPoint = mapLocalToAncestor(target, toLayoutBoxModelObject(target->parent()), FloatPoint(10, 10));
    EXPECT_EQ(FloatPoint(10, 10), mappedPoint);
}

TEST_F(MapLocalToAncestorTest, SimpleBlock)
{
    setBodyInnerHTML(
        "<div style='margin:666px; border:8px solid; padding:7px;'>"
        "    <div id='target' style='margin:10px; border:666px; padding:666px;'></div>"
        "</div>");

    LayoutObject* target = getLayoutObjectByElementId("target");
    FloatPoint mappedPoint = mapLocalToAncestor(target, toLayoutBoxModelObject(target->parent()), FloatPoint(100, 100));
    EXPECT_EQ(FloatPoint(125, 125), mappedPoint);
}

TEST_F(MapLocalToAncestorTest, TextInRelPosInline)
{
    setBodyInnerHTML("<div><span style='position:relative; left:7px; top:4px;'><br id='sibling'>text</span></div>");

    LayoutObject* br = getLayoutObjectByElementId("sibling");
    LayoutObject* text = br->nextSibling();
    ASSERT_TRUE(text->isText());
    FloatPoint mappedPoint = mapLocalToAncestor(text, text->containingBlock(), FloatPoint(10, 30));
    EXPECT_EQ(FloatPoint(17, 34), mappedPoint);
}

TEST_F(MapLocalToAncestorTest, RelposInline)
{
    setBodyInnerHTML("<span id='target' style='position:relative; left:50px; top:100px;'>text</span>");

    LayoutObject* target = getLayoutObjectByElementId("target");
    FloatPoint mappedPoint = mapLocalToAncestor(target, toLayoutBoxModelObject(target->parent()), FloatPoint(10, 10));
    EXPECT_EQ(FloatPoint(60, 110), mappedPoint);
}

TEST_F(MapLocalToAncestorTest, RelposInlineInRelposInline)
{
    setBodyInnerHTML(
        "<div style='padding-left:10px;'>"
        "    <span style='position:relative; left:5px; top:6px;'>"
        "        <span id='target' style='position:relative; left:50px; top:100px;'>text</span>"
        "    </span>"
        "</div>");

    LayoutObject* target = getLayoutObjectByElementId("target");
    LayoutInline* parent = toLayoutInline(target->parent());
    LayoutBlockFlow* containingBlock = toLayoutBlockFlow(parent->parent());

    FloatPoint mappedPoint = mapLocalToAncestor(target, containingBlock, FloatPoint(20, 10));
    EXPECT_EQ(FloatPoint(75, 116), mappedPoint);

    // Walk each ancestor in the chain separately, to verify each step on the way.
    mappedPoint = mapLocalToAncestor(target, parent, FloatPoint(20, 10));
    EXPECT_EQ(FloatPoint(70, 110), mappedPoint);

    mappedPoint = mapLocalToAncestor(parent, containingBlock, mappedPoint);
    EXPECT_EQ(FloatPoint(75, 116), mappedPoint);
}

TEST_F(MapLocalToAncestorTest, RelPosBlock)
{
    setBodyInnerHTML(
        "<div id='container' style='margin:666px; border:8px solid; padding:7px;'>"
        "    <div id='middle' style='margin:30px; border:1px solid;'>"
        "        <div id='target' style='position:relative; left:50px; top:50px; margin:10px; border:666px; padding:666px;'></div>"
        "    </div>"
        "</div>");

    LayoutBox* target = toLayoutBox(getLayoutObjectByElementId("target"));
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));

    FloatPoint mappedPoint = mapLocalToAncestor(target, container, FloatPoint());
    EXPECT_EQ(FloatPoint(106, 106), mappedPoint);

    // Walk each ancestor in the chain separately, to verify each step on the way.
    LayoutBox* middle = toLayoutBox(getLayoutObjectByElementId("middle"));

    mappedPoint = mapLocalToAncestor(target, middle, FloatPoint());
    EXPECT_EQ(FloatPoint(61, 61), mappedPoint);

    mappedPoint = mapLocalToAncestor(middle, container, mappedPoint);
    EXPECT_EQ(FloatPoint(106, 106), mappedPoint);
}

TEST_F(MapLocalToAncestorTest, AbsPos)
{
    setBodyInnerHTML(
        "<div id='container' style='position:relative; margin:666px; border:8px solid; padding:7px;'>"
        "    <div id='staticChild' style='margin:30px; padding-top:666px;'>"
        "        <div style='padding-top:666px;'></div>"
        "        <div id='target' style='position:absolute; left:-1px; top:-1px; margin:10px; border:666px; padding:666px;'></div>"
        "    </div>"
        "</div>");

    LayoutBox* target = toLayoutBox(getLayoutObjectByElementId("target"));
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));

    FloatPoint mappedPoint = mapLocalToAncestor(target, container, FloatPoint());
    EXPECT_EQ(FloatPoint(17, 17), mappedPoint);

    // Walk each ancestor in the chain separately, to verify each step on the way.
    LayoutBox* staticChild = toLayoutBox(getLayoutObjectByElementId("staticChild"));

    mappedPoint = mapLocalToAncestor(target, staticChild, FloatPoint());
    EXPECT_EQ(FloatPoint(-28, -28), mappedPoint);

    mappedPoint = mapLocalToAncestor(staticChild, container, mappedPoint);
    EXPECT_EQ(FloatPoint(17, 17), mappedPoint);
}

TEST_F(MapLocalToAncestorTest, AbsPosAuto)
{
    setBodyInnerHTML(
        "<div id='container' style='position:absolute; margin:666px; border:8px solid; padding:7px;'>"
        "    <div id='staticChild' style='margin:30px; padding-top:5px;'>"
        "        <div style='padding-top:20px;'></div>"
        "        <div id='target' style='position:absolute; margin:10px; border:666px; padding:666px;'></div>"
        "    </div>"
        "</div>");

    LayoutBox* target = toLayoutBox(getLayoutObjectByElementId("target"));
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));

    FloatPoint mappedPoint = mapLocalToAncestor(target, container, FloatPoint());
    EXPECT_EQ(FloatPoint(55, 80), mappedPoint);

    // Walk each ancestor in the chain separately, to verify each step on the way.
    LayoutBox* staticChild = toLayoutBox(getLayoutObjectByElementId("staticChild"));

    mappedPoint = mapLocalToAncestor(target, staticChild, FloatPoint());
    EXPECT_EQ(FloatPoint(10, 35), mappedPoint);

    mappedPoint = mapLocalToAncestor(staticChild, container, mappedPoint);
    EXPECT_EQ(FloatPoint(55, 80), mappedPoint);
}

TEST_F(MapLocalToAncestorTest, FixedPos)
{
    // Assuming BODY margin of 8px.
    setBodyInnerHTML(
        "<div id='container' style='position:absolute; margin:4px; border:5px solid; padding:7px;'>"
        "    <div id='staticChild' style='padding-top:666px;'>"
        "        <div style='padding-top:666px;'></div>"
        "        <div id='target' style='position:fixed; left:-1px; top:-1px; margin:10px; border:666px; padding:666px;'></div>"
        "    </div>"
        "</div>");

    LayoutBox* target = toLayoutBox(getLayoutObjectByElementId("target"));
    LayoutBox* staticChild = toLayoutBox(getLayoutObjectByElementId("staticChild"));
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));
    LayoutBox* body = container->parentBox();
    LayoutBox* html = body->parentBox();
    LayoutBox* view = html->parentBox();
    ASSERT_TRUE(view->isLayoutView());

    FloatPoint mappedPoint = mapLocalToAncestor(target, view, FloatPoint());
    EXPECT_EQ(FloatPoint(9, 9), mappedPoint);

    // Walk each ancestor in the chain separately, to verify each step on the way.
    mappedPoint = mapLocalToAncestor(target, staticChild, FloatPoint());
    EXPECT_EQ(FloatPoint(-15, -15), mappedPoint);

    mappedPoint = mapLocalToAncestor(staticChild, container, mappedPoint);
    EXPECT_EQ(FloatPoint(-3, -3), mappedPoint);

    mappedPoint = mapLocalToAncestor(container, body, mappedPoint);
    EXPECT_EQ(FloatPoint(1, 1), mappedPoint);

    mappedPoint = mapLocalToAncestor(body, html, mappedPoint);
    EXPECT_EQ(FloatPoint(9, 9), mappedPoint);

    mappedPoint = mapLocalToAncestor(html, view, mappedPoint);
    EXPECT_EQ(FloatPoint(9, 9), mappedPoint);
}

TEST_F(MapLocalToAncestorTest, FixedPosAuto)
{
    // Assuming BODY margin of 8px.
    setBodyInnerHTML(
        "<div id='container' style='position:absolute; margin:3px; border:8px solid; padding:7px;'>"
        "    <div id='staticChild' style='padding-top:5px;'>"
        "        <div style='padding-top:20px;'></div>"
        "        <div id='target' style='position:fixed; margin:10px; border:666px; padding:666px;'></div>"
        "    </div>"
        "</div>");

    LayoutBox* target = toLayoutBox(getLayoutObjectByElementId("target"));
    LayoutBox* staticChild = toLayoutBox(getLayoutObjectByElementId("staticChild"));
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));
    LayoutBox* body = container->parentBox();
    LayoutBox* html = body->parentBox();
    LayoutBox* view = html->parentBox();
    ASSERT_TRUE(view->isLayoutView());

    FloatPoint mappedPoint = mapLocalToAncestor(target, target->containingBlock(), FloatPoint());
    EXPECT_EQ(FloatPoint(36, 61), mappedPoint);

    // Walk each ancestor in the chain separately, to verify each step on the way.
    mappedPoint = mapLocalToAncestor(target, staticChild, FloatPoint());
    EXPECT_EQ(FloatPoint(10, 35), mappedPoint);

    mappedPoint = mapLocalToAncestor(staticChild, container, mappedPoint);
    EXPECT_EQ(FloatPoint(25, 50), mappedPoint);

    mappedPoint = mapLocalToAncestor(container, body, mappedPoint);
    EXPECT_EQ(FloatPoint(28, 53), mappedPoint);

    mappedPoint = mapLocalToAncestor(body, html, mappedPoint);
    EXPECT_EQ(FloatPoint(36, 61), mappedPoint);

    mappedPoint = mapLocalToAncestor(html, view, mappedPoint);
    EXPECT_EQ(FloatPoint(36, 61), mappedPoint);
}

TEST_F(MapLocalToAncestorTest, FixedPosInFixedPos)
{
    // Assuming BODY margin of 8px.
    setBodyInnerHTML(
        "<div id='container' style='position:absolute; margin:4px; border:5px solid; padding:7px;'>"
        "    <div id='staticChild' style='padding-top:666px;'>"
        "        <div style='padding-top:666px;'></div>"
        "        <div id='outerFixed' style='position:fixed; left:100px; top:100px; margin:10px; border:666px; padding:666px;'>"
        "            <div id='target' style='position:fixed; left:-1px; top:-1px; margin:10px; border:666px; padding:666px;'></div>"
        "        </div>"
        "    </div>"
        "</div>");

    LayoutBox* target = toLayoutBox(getLayoutObjectByElementId("target"));
    LayoutBox* outerFixed = toLayoutBox(getLayoutObjectByElementId("outerFixed"));
    LayoutBox* staticChild = toLayoutBox(getLayoutObjectByElementId("staticChild"));
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));
    LayoutBox* body = container->parentBox();
    LayoutBox* html = body->parentBox();
    LayoutBox* view = html->parentBox();
    ASSERT_TRUE(view->isLayoutView());

    FloatPoint mappedPoint = mapLocalToAncestor(target, view, FloatPoint());
    EXPECT_EQ(FloatPoint(9, 9), mappedPoint);

    // Walk each ancestor in the chain separately, to verify each step on the way.
    mappedPoint = mapLocalToAncestor(target, outerFixed, FloatPoint());
    EXPECT_EQ(FloatPoint(-101, -101), mappedPoint);

    mappedPoint = mapLocalToAncestor(outerFixed, staticChild, mappedPoint);
    EXPECT_EQ(FloatPoint(-15, -15), mappedPoint);

    mappedPoint = mapLocalToAncestor(staticChild, container, mappedPoint);
    EXPECT_EQ(FloatPoint(-3, -3), mappedPoint);

    mappedPoint = mapLocalToAncestor(container, body, mappedPoint);
    EXPECT_EQ(FloatPoint(1, 1), mappedPoint);

    mappedPoint = mapLocalToAncestor(body, html, mappedPoint);
    EXPECT_EQ(FloatPoint(9, 9), mappedPoint);

    mappedPoint = mapLocalToAncestor(html, view, mappedPoint);
    EXPECT_EQ(FloatPoint(9, 9), mappedPoint);
}

TEST_F(MapLocalToAncestorTest, MulticolWithText)
{
    setBodyInnerHTML(
        "<div id='multicol' style='-webkit-columns:2; -webkit-column-gap:20px; width:400px; line-height:50px; padding:5px;'>"
        "    <br id='sibling'>"
        "    text"
        "</div>");

    LayoutObject* target = getLayoutObjectByElementId("sibling")->nextSibling();
    ASSERT_TRUE(target->isText());
    LayoutBox* flowThread = toLayoutBox(target->parent());
    ASSERT_TRUE(flowThread->isLayoutFlowThread());
    LayoutBox* multicol = toLayoutBox(getLayoutObjectByElementId("multicol"));

    FloatPoint mappedPoint = mapLocalToAncestor(target, flowThread, FloatPoint(10, 70));
    EXPECT_EQ(FloatPoint(220, 20), mappedPoint);

    mappedPoint = mapLocalToAncestor(flowThread, multicol, mappedPoint);
    EXPECT_EQ(FloatPoint(225, 25), mappedPoint);
}

TEST_F(MapLocalToAncestorTest, MulticolWithInline)
{
    setBodyInnerHTML(
        "<div id='multicol' style='-webkit-columns:2; -webkit-column-gap:20px; width:400px; line-height:50px; padding:5px;'>"
        "    <span id='target'><br>text</span>"
        "</div>");

    LayoutObject* target = getLayoutObjectByElementId("target");
    LayoutBox* flowThread = toLayoutBox(target->parent());
    ASSERT_TRUE(flowThread->isLayoutFlowThread());
    LayoutBox* multicol = toLayoutBox(getLayoutObjectByElementId("multicol"));

    FloatPoint mappedPoint = mapLocalToAncestor(target, flowThread, FloatPoint(10, 70));
    EXPECT_EQ(FloatPoint(220, 20), mappedPoint);

    mappedPoint = mapLocalToAncestor(flowThread, multicol, mappedPoint);
    EXPECT_EQ(FloatPoint(225, 25), mappedPoint);
}

TEST_F(MapLocalToAncestorTest, MulticolWithBlock)
{
    setBodyInnerHTML(
        "<div id='container' style='-webkit-columns:3; -webkit-column-gap:0; column-fill:auto; width:300px; height:100px; border:8px solid; padding:7px;'>"
        "    <div style='height:110px;'></div>"
        "    <div id='target' style='margin:10px; border:13px; padding:13px;'></div>"
        "</div>");

    LayoutBox* target = toLayoutBox(getLayoutObjectByElementId("target"));
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));

    FloatPoint mappedPoint = mapLocalToAncestor(target, container, FloatPoint());
    EXPECT_EQ(FloatPoint(125, 35), mappedPoint);

    // Walk each ancestor in the chain separately, to verify each step on the way.
    LayoutBox* flowThread = target->parentBox();
    ASSERT_TRUE(flowThread->isLayoutFlowThread());

    mappedPoint = mapLocalToAncestor(target, flowThread, FloatPoint());
    EXPECT_EQ(FloatPoint(110, 20), mappedPoint);

    mappedPoint = mapLocalToAncestor(flowThread, container, mappedPoint);
    EXPECT_EQ(FloatPoint(125, 35), mappedPoint);
}

TEST_F(MapLocalToAncestorTest, MulticolWithAbsPosInRelPos)
{
    setBodyInnerHTML(
        "<div id='multicol' style='-webkit-columns:3; -webkit-column-gap:0; column-fill:auto; width:300px; height:100px; border:8px solid; padding:7px;'>"
        "    <div style='height:110px;'></div>"
        "    <div id='relpos' style='position:relative; left:4px; top:4px;'>"
        "        <div id='target' style='position:absolute; left:15px; top:15px; margin:10px; border:13px; padding:13px;'></div>"
        "    </div>"
        "</div>");

    LayoutBox* target = toLayoutBox(getLayoutObjectByElementId("target"));
    LayoutBox* multicol = toLayoutBox(getLayoutObjectByElementId("multicol"));

    FloatPoint mappedPoint = mapLocalToAncestor(target, multicol, FloatPoint());
    EXPECT_EQ(FloatPoint(144, 54), mappedPoint);

    // Walk each ancestor in the chain separately, to verify each step on the way.
    LayoutBox* relpos = toLayoutBox(getLayoutObjectByElementId("relpos"));
    LayoutBox* flowThread = relpos->parentBox();
    ASSERT_TRUE(flowThread->isLayoutFlowThread());

    mappedPoint = mapLocalToAncestor(target, relpos, FloatPoint());
    EXPECT_EQ(FloatPoint(25, 25), mappedPoint);

    mappedPoint = mapLocalToAncestor(relpos, flowThread, mappedPoint);
    EXPECT_EQ(FloatPoint(129, 39), mappedPoint);

    mappedPoint = mapLocalToAncestor(flowThread, multicol, mappedPoint);
    EXPECT_EQ(FloatPoint(144, 54), mappedPoint);
}

TEST_F(MapLocalToAncestorTest, MulticolWithAbsPosNotContained)
{
    setBodyInnerHTML(
        "<div id='container' style='position:relative; margin:666px; border:7px solid; padding:3px;'>"
        "    <div id='multicol' style='-webkit-columns:3; -webkit-column-gap:0; column-fill:auto; width:300px; height:100px; border:8px solid; padding:7px;'>"
        "        <div style='height:110px;'></div>"
        "        <div id='target' style='position:absolute; left:-1px; top:-1px; margin:10px; border:13px; padding:13px;'></div>"
        "    </div>"
        "</div>");

    LayoutBox* target = toLayoutBox(getLayoutObjectByElementId("target"));
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));

    // The multicol container isn't in the containing block chain of the abspos #target.
    FloatPoint mappedPoint = mapLocalToAncestor(target, container, FloatPoint());
    EXPECT_EQ(FloatPoint(16, 16), mappedPoint);

    // Walk each ancestor in the chain separately, to verify each step on the way.
    LayoutBox* multicol = toLayoutBox(getLayoutObjectByElementId("multicol"));
    LayoutBox* flowThread = target->parentBox();
    ASSERT_TRUE(flowThread->isLayoutFlowThread());

    mappedPoint = mapLocalToAncestor(target, flowThread, FloatPoint());
    EXPECT_EQ(FloatPoint(-9, -9), mappedPoint);

    mappedPoint = mapLocalToAncestor(flowThread, multicol, mappedPoint);
    EXPECT_EQ(FloatPoint(6, 6), mappedPoint);

    mappedPoint = mapLocalToAncestor(multicol, container, mappedPoint);
    EXPECT_EQ(FloatPoint(16, 16), mappedPoint);
}

TEST_F(MapLocalToAncestorTest, FlippedBlocksWritingModeWithText)
{
    setBodyInnerHTML(
        "<div style='-webkit-writing-mode:vertical-rl;'>"
        "    <div style='width:13px;'></div>"
        "    <div style='width:200px; height:400px; line-height:50px;'>"
        "        <br id='sibling'>text"
        "    </div>"
        "    <div style='width:5px;'></div>"
        "</div>");

    LayoutObject* br = getLayoutObjectByElementId("sibling");
    LayoutObject* text = br->nextSibling();
    ASSERT_TRUE(text->isText());

    // Map to the nearest container. Flipping should occur.
    FloatPoint mappedPoint = mapLocalToAncestor(text, text->containingBlock(), FloatPoint(75, 10), ApplyContainerFlip);
    EXPECT_EQ(FloatPoint(125, 10), mappedPoint);

    // Map to a container further up in the tree. Flipping should still occur on the nearest
    // container. LayoutObject::mapLocalToAncestor() is called recursively until the ancestor is
    // reached, and the ApplyContainerFlip flag is cleared after having processed the innermost
    // object.
    mappedPoint = mapLocalToAncestor(text, text->containingBlock()->containingBlock(), FloatPoint(75, 10), ApplyContainerFlip);
    EXPECT_EQ(FloatPoint(130, 10), mappedPoint);

    // If the ApplyContainerFlip flag isn't specified, no flipping should take place.
    mappedPoint = mapLocalToAncestor(text, text->containingBlock()->containingBlock(), FloatPoint(75, 10));
    EXPECT_EQ(FloatPoint(80, 10), mappedPoint);
}

TEST_F(MapLocalToAncestorTest, FlippedBlocksWritingModeWithInline)
{
    setBodyInnerHTML(
        "<div style='-webkit-writing-mode:vertical-rl;'>"
        "    <div style='width:13px;'></div>"
        "    <div style='width:200px; height:400px; line-height:50px;'>"
        "        <span>"
        "            <span id='target'><br>text</span>"
        "        </span>"
        "    </div>"
        "    <div style='width:7px;'></div>"
        "</div>");

    LayoutObject* target = getLayoutObjectByElementId("target");
    ASSERT_TRUE(target);

    // First map to the parent SPAN. Nothing special should happen, since flipping occurs at the nearest container.
    FloatPoint mappedPoint = mapLocalToAncestor(target, toLayoutBoxModelObject(target->parent()), FloatPoint(75, 10), ApplyContainerFlip);
    EXPECT_EQ(FloatPoint(75, 10), mappedPoint);

    // Continue to the nearest container. Flipping should occur.
    mappedPoint = mapLocalToAncestor(toLayoutBoxModelObject(target->parent()), target->containingBlock(), mappedPoint, ApplyContainerFlip);
    EXPECT_EQ(FloatPoint(125, 10), mappedPoint);

    // Now map from the innermost inline to the nearest container in one go.
    mappedPoint = mapLocalToAncestor(target, target->containingBlock(), FloatPoint(75, 10), ApplyContainerFlip);
    EXPECT_EQ(FloatPoint(125, 10), mappedPoint);

    // Map to a container further up in the tree. Flipping should still only occur on the nearest container.
    mappedPoint = mapLocalToAncestor(target, target->containingBlock()->containingBlock(), FloatPoint(75, 10), ApplyContainerFlip);
    EXPECT_EQ(FloatPoint(132, 10), mappedPoint);

    // If the ApplyContainerFlip flag isn't specified, no flipping should take place.
    mappedPoint = mapLocalToAncestor(target, target->containingBlock()->containingBlock(), FloatPoint(75, 10));
    EXPECT_EQ(FloatPoint(82, 10), mappedPoint);
}

TEST_F(MapLocalToAncestorTest, FlippedBlocksWritingModeWithBlock)
{
    setBodyInnerHTML(
        "<div id='container' style='-webkit-writing-mode:vertical-rl; border:8px solid; padding:7px; width:200px; height:200px;'>"
        "    <div id='middle' style='border:1px solid;'>"
        "        <div style='width:30px;'></div>"
        "        <div id='target' style='margin:6px; width:25px;'></div>"
        "    </div>"
        "</div>");

    LayoutBox* target = toLayoutBox(getLayoutObjectByElementId("target"));
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));

    FloatPoint mappedPoint = mapLocalToAncestor(target, container, FloatPoint());
    EXPECT_EQ(FloatPoint(153, 22), mappedPoint);

    // Walk each ancestor in the chain separately, to verify each step on the way.
    LayoutBox* middle = toLayoutBox(getLayoutObjectByElementId("middle"));

    mappedPoint = mapLocalToAncestor(target, middle, FloatPoint());
    EXPECT_EQ(FloatPoint(7, 7), mappedPoint);

    mappedPoint = mapLocalToAncestor(middle, container, mappedPoint);
    EXPECT_EQ(FloatPoint(153, 22), mappedPoint);
}

TEST_F(MapLocalToAncestorTest, Table)
{
    setBodyInnerHTML(
        "<style>td { padding: 2px; }</style>"
        "<div id='container' style='border:3px solid;'>"
        "    <table style='margin:9px; border:5px solid; border-spacing:10px;'>"
        "        <thead>"
        "            <tr>"
        "                <td>"
        "                    <div style='width:100px; height:100px;'></div>"
        "                </td>"
        "            </tr>"
        "        </thead>"
        "        <tbody>"
        "            <tr>"
        "                <td>"
        "                    <div style='width:100px; height:100px;'></div>"
        "                 </td>"
        "            </tr>"
        "            <tr>"
        "                <td>"
        "                     <div style='width:100px; height:100px;'></div>"
        "                </td>"
        "                <td>"
        "                    <div id='target' style='width:100px; height:10px;'></div>"
        "                </td>"
        "            </tr>"
        "        </tbody>"
        "    </table>"
        "</div>");

    LayoutBox* target = toLayoutBox(getLayoutObjectByElementId("target"));
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));

    FloatPoint mappedPoint = mapLocalToAncestor(target, container, FloatPoint());
    EXPECT_EQ(FloatPoint(143, 302), mappedPoint);

    // Walk each ancestor in the chain separately, to verify each step on the way.
    LayoutBox* td = target->parentBox();
    ASSERT_TRUE(td->isTableCell());
    mappedPoint = mapLocalToAncestor(target, td, FloatPoint());
    // Cells are middle-aligned by default.
    EXPECT_EQ(FloatPoint(2, 47), mappedPoint);

    LayoutBox* tr = td->parentBox();
    ASSERT_TRUE(tr->isTableRow());
    mappedPoint = mapLocalToAncestor(td, tr, mappedPoint);
    EXPECT_EQ(FloatPoint(126, 47), mappedPoint);

    LayoutBox* tbody = tr->parentBox();
    ASSERT_TRUE(tbody->isTableSection());
    mappedPoint = mapLocalToAncestor(tr, tbody, mappedPoint);
    EXPECT_EQ(FloatPoint(126, 161), mappedPoint);

    LayoutBox* table = tbody->parentBox();
    ASSERT_TRUE(table->isTable());
    mappedPoint = mapLocalToAncestor(tbody, table, mappedPoint);
    EXPECT_EQ(FloatPoint(131, 290), mappedPoint);

    mappedPoint = mapLocalToAncestor(table, container, mappedPoint);
    EXPECT_EQ(FloatPoint(143, 302), mappedPoint);
}

static bool floatValuesAlmostEqual(float expected, float actual)
{
    return fabs(expected - actual) < 0.01;
}

static bool floatQuadsAlmostEqual(const FloatQuad& expected, const FloatQuad& actual)
{
    return floatValuesAlmostEqual(expected.p1().x(), actual.p1().x())
        && floatValuesAlmostEqual(expected.p1().y(), actual.p1().y())
        && floatValuesAlmostEqual(expected.p2().x(), actual.p2().x())
        && floatValuesAlmostEqual(expected.p2().y(), actual.p2().y())
        && floatValuesAlmostEqual(expected.p3().x(), actual.p3().x())
        && floatValuesAlmostEqual(expected.p3().y(), actual.p3().y())
        && floatValuesAlmostEqual(expected.p4().x(), actual.p4().x())
        && floatValuesAlmostEqual(expected.p4().y(), actual.p4().y());
}

// If comparison fails, pretty-print the error using EXPECT_EQ()
#define EXPECT_FLOAT_QUAD_EQ(expected, actual) \
    do { \
        if (!floatQuadsAlmostEqual(expected, actual)) { \
            EXPECT_EQ(expected, actual); \
        } \
    } while (false)


TEST_F(MapLocalToAncestorTest, Transforms)
{
    setBodyInnerHTML(
        "<div id='container'>"
        "    <div id='outerTransform' style='transform:rotate(45deg); width:200px; height:200px;'>"
        "        <div id='innerTransform' style='transform:rotate(45deg); width:200px; height:200px;'>"
        "            <div id='target' style='width:200px; height:200px;'></div>"
        "        </div>"
        "    </div>"
        "</div>");

    LayoutBox* target = toLayoutBox(getLayoutObjectByElementId("target"));
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));

    const FloatQuad initialQuad(FloatPoint(0, 0), FloatPoint(200, 0), FloatPoint(200, 200), FloatPoint(0, 200));
    FloatQuad mappedQuad = mapLocalToAncestor(target, container, initialQuad, UseTransforms);
    EXPECT_FLOAT_QUAD_EQ(FloatQuad(FloatPoint(200, 0), FloatPoint(200, 200), FloatPoint(0, 200), FloatPoint(0, 0)), mappedQuad);

    // Walk each ancestor in the chain separately, to verify each step on the way.
    LayoutBox* innerTransform = toLayoutBox(getLayoutObjectByElementId("innerTransform"));
    LayoutBox* outerTransform = toLayoutBox(getLayoutObjectByElementId("outerTransform"));

    mappedQuad = mapLocalToAncestor(target, innerTransform, initialQuad, UseTransforms);
    EXPECT_FLOAT_QUAD_EQ(FloatQuad(FloatPoint(0, 0), FloatPoint(200, 0), FloatPoint(200, 200), FloatPoint(0, 200)), mappedQuad);

    mappedQuad = mapLocalToAncestor(innerTransform, outerTransform, mappedQuad, UseTransforms);
    // Clockwise rotation by 45 degrees.
    EXPECT_FLOAT_QUAD_EQ(FloatQuad(FloatPoint(100, -41.42), FloatPoint(241.42, 100), FloatPoint(100, 241.42), FloatPoint(-41.42, 100)), mappedQuad);

    mappedQuad = mapLocalToAncestor(outerTransform, container, mappedQuad, UseTransforms);
    // Another clockwise rotation by 45 degrees. So now 90 degrees in total.
    EXPECT_FLOAT_QUAD_EQ(FloatQuad(FloatPoint(200, 0), FloatPoint(200, 200), FloatPoint(0, 200), FloatPoint(0, 0)), mappedQuad);
}

} // namespace blink
