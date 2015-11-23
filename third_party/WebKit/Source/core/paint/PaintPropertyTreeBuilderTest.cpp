// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutTreeAsText.h"
#include "core/layout/LayoutView.h"
#include "core/paint/ObjectPaintProperties.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "platform/text/TextStream.h"
#include "public/platform/Platform.h"
#include "public/platform/WebUnitTestSupport.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"

namespace blink {

class PaintPropertyTreeBuilderTest : public RenderingTest {
public:
    PaintPropertyTreeBuilderTest()
        : m_originalSlimmingPaintV2Enabled(RuntimeEnabledFeatures::slimmingPaintV2Enabled()) { }

    void loadTestData(const char* fileName)
    {
        String fullPath(Platform::current()->unitTestSupport()->webKitRootDir());
        fullPath.append("/Source/core/paint/test_data/");
        fullPath.append(fileName);
        WebData inputBuffer = Platform::current()->unitTestSupport()->readFromFile(fullPath);
        setBodyInnerHTML(String(inputBuffer.data(), inputBuffer.size()));
    }

private:
    void SetUp() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(true);
        Settings::setMockScrollbarsEnabled(true);

        RenderingTest::SetUp();
        enableCompositing();
    }

    void TearDown() override
    {
        RenderingTest::TearDown();

        Settings::setMockScrollbarsEnabled(false);
        RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(m_originalSlimmingPaintV2Enabled);
    }

    bool m_originalSlimmingPaintV2Enabled;
};

TEST_F(PaintPropertyTreeBuilderTest, FixedPosition)
{
    loadTestData("fixed-position.html");

    // target1 is a fixed-position element inside an absolute-position scrolling element.
    // It should be attached under the viewport to skip scrolling and offset of the parent.
    Element* target1 = document().getElementById("target1");
    ObjectPaintProperties* target1Properties = target1->layoutObject()->objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate(200, 150), target1Properties->paintOffsetTranslation()->matrix());
    EXPECT_EQ(document().view()->preTranslation(), target1Properties->paintOffsetTranslation()->parent());

    // target2 is a fixed-position element inside a transformed scrolling element.
    // It should be attached under the scrolled box of the transformed element.
    Element* target2 = document().getElementById("target2");
    ObjectPaintProperties* target2Properties = target2->layoutObject()->objectPaintProperties();
    Element* scroller = document().getElementById("scroller");
    ObjectPaintProperties* scrollerProperties = scroller->layoutObject()->objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate(200, 150), target2Properties->paintOffsetTranslation()->matrix());
    EXPECT_EQ(scrollerProperties->scrollTranslation(), target2Properties->paintOffsetTranslation()->parent());
}

TEST_F(PaintPropertyTreeBuilderTest, PositionAndScroll)
{
    loadTestData("position-and-scroll.html");

    Element* scroller = document().getElementById("scroller");
    scroller->scrollTo(0, 100);
    FrameView* frameView = document().view();
    frameView->updateAllLifecyclePhases();
    ObjectPaintProperties* scrollerProperties = scroller->layoutObject()->objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate(0, -100), scrollerProperties->scrollTranslation()->matrix());
    EXPECT_EQ(frameView->scrollTranslation(), scrollerProperties->scrollTranslation()->parent());

    // The relative-positioned element should have accumulated box offset (exclude scrolling),
    // and should be affected by ancestor scroll transforms.
    Element* relPos = document().getElementById("rel-pos");
    ObjectPaintProperties* relPosProperties = relPos->layoutObject()->objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate(680, 1120), relPosProperties->paintOffsetTranslation()->matrix());
    EXPECT_EQ(scrollerProperties->scrollTranslation(), relPosProperties->paintOffsetTranslation()->parent());

    // The absolute-positioned element should not be affected by non-positioned scroller at all.
    Element* absPos = document().getElementById("abs-pos");
    ObjectPaintProperties* absPosProperties = absPos->layoutObject()->objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate(123, 456), absPosProperties->paintOffsetTranslation()->matrix());
    EXPECT_EQ(frameView->scrollTranslation(), absPosProperties->paintOffsetTranslation()->parent());
}

TEST_F(PaintPropertyTreeBuilderTest, FrameScrollingTraditional)
{
    setBodyInnerHTML("<style> body { height: 10000px; } </style>");

    document().domWindow()->scrollTo(0, 100);

    FrameView* frameView = document().view();
    frameView->updateAllLifecyclePhases();
    EXPECT_EQ(TransformationMatrix(), frameView->preTranslation()->matrix());
    EXPECT_EQ(nullptr, frameView->preTranslation()->parent());
    EXPECT_EQ(TransformationMatrix().translate(0, -100), frameView->scrollTranslation()->matrix());
    EXPECT_EQ(frameView->preTranslation(), frameView->scrollTranslation()->parent());

    LayoutView* layoutView = document().layoutView();
    ObjectPaintProperties* layoutViewProperties = layoutView->objectPaintProperties();
    EXPECT_EQ(nullptr, layoutViewProperties);
}

// TODO(trchen): Settings::rootLayerScrolls cannot be switched after main frame being created.
// Need to set it during test setup. Besides that, the test still won't work because
// root layer scrolling mode is not compatible with SPv2 at this moment.
// (Duplicate display item ID for FrameView and LayoutView.)
TEST_F(PaintPropertyTreeBuilderTest, DISABLED_FrameScrollingRootLayerScrolls)
{
    document().settings()->setRootLayerScrolls(true);

    setBodyInnerHTML("<style> body { height: 10000px; } </style>");

    document().domWindow()->scrollTo(0, 100);

    FrameView* frameView = document().view();
    frameView->updateAllLifecyclePhases();
    EXPECT_EQ(TransformationMatrix(), frameView->preTranslation()->matrix());
    EXPECT_EQ(nullptr, frameView->preTranslation()->parent());
    EXPECT_EQ(TransformationMatrix(), frameView->scrollTranslation()->matrix());
    EXPECT_EQ(frameView->preTranslation(), frameView->scrollTranslation()->parent());

    LayoutView* layoutView = document().layoutView();
    ObjectPaintProperties* layoutViewProperties = layoutView->objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate(0, -100), layoutViewProperties->scrollTranslation()->matrix());
    EXPECT_EQ(frameView->scrollTranslation(), layoutViewProperties->scrollTranslation()->parent());
}

TEST_F(PaintPropertyTreeBuilderTest, Perspective)
{
    loadTestData("perspective.html");

    Element* perspective = document().getElementById("perspective");
    ObjectPaintProperties* perspectiveProperties = perspective->layoutObject()->objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().applyPerspective(100), perspectiveProperties->perspective()->matrix());
    // The perspective origin is the center of the border box plus accumulated paint offset.
    EXPECT_EQ(FloatPoint3D(250, 250, 0), perspectiveProperties->perspective()->origin());
    EXPECT_EQ(document().view()->scrollTranslation(), perspectiveProperties->perspective()->parent());

    // Adding perspective doesn't clear paint offset. The paint offset will be passed down to children.
    Element* inner = document().getElementById("inner");
    ObjectPaintProperties* innerProperties = inner->layoutObject()->objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate(50, 100), innerProperties->paintOffsetTranslation()->matrix());
    EXPECT_EQ(perspectiveProperties->perspective(), innerProperties->paintOffsetTranslation()->parent());
}

TEST_F(PaintPropertyTreeBuilderTest, Transform)
{
    loadTestData("transform.html");

    Element* transform = document().getElementById("transform");
    ObjectPaintProperties* transformProperties = transform->layoutObject()->objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate3d(123, 456, 789), transformProperties->transform()->matrix());
    EXPECT_EQ(FloatPoint3D(200, 150, 0), transformProperties->transform()->origin());
    EXPECT_EQ(transformProperties->paintOffsetTranslation(), transformProperties->transform()->parent());
    EXPECT_EQ(TransformationMatrix().translate(50, 100), transformProperties->paintOffsetTranslation()->matrix());
    EXPECT_EQ(document().view()->scrollTranslation(), transformProperties->paintOffsetTranslation()->parent());
}

TEST_F(PaintPropertyTreeBuilderTest, RelativePositionInline)
{
    loadTestData("relative-position-inline.html");

    Element* inlineBlock = document().getElementById("inline-block");
    ObjectPaintProperties* inlineBlockProperties = inlineBlock->layoutObject()->objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate(135, 490), inlineBlockProperties->paintOffsetTranslation()->matrix());
    EXPECT_EQ(document().view()->scrollTranslation(), inlineBlockProperties->paintOffsetTranslation()->parent());
}

TEST_F(PaintPropertyTreeBuilderTest, NestedOpacityEffect)
{
    setBodyInnerHTML(
        "<div id='nodeWithoutOpacity'>"
        "  <div id='childWithOpacity' style='opacity: 0.5'>"
        "    <div id='grandChildWithoutOpacity'>"
        "      <div id='greatGrandChildWithOpacity' style='opacity: 0.2'/>"
        "    </div>"
        "  </div>"
        "</div>");

    LayoutObject& nodeWithoutOpacity = *document().getElementById("nodeWithoutOpacity")->layoutObject();
    ObjectPaintProperties* nodeWithoutOpacityProperties = nodeWithoutOpacity.objectPaintProperties();
    EXPECT_EQ(nullptr, nodeWithoutOpacityProperties);

    LayoutObject& childWithOpacity = *document().getElementById("childWithOpacity")->layoutObject();
    ObjectPaintProperties* childWithOpacityProperties = childWithOpacity.objectPaintProperties();
    EXPECT_EQ(0.5f, childWithOpacityProperties->effect()->opacity());
    // childWithOpacity is the root effect node.
    EXPECT_EQ(nullptr, childWithOpacityProperties->effect()->parent());

    LayoutObject& grandChildWithoutOpacity = *document().getElementById("grandChildWithoutOpacity")->layoutObject();
    EXPECT_EQ(nullptr, grandChildWithoutOpacity.objectPaintProperties());

    LayoutObject& greatGrandChildWithOpacity = *document().getElementById("greatGrandChildWithOpacity")->layoutObject();
    ObjectPaintProperties* greatGrandChildWithOpacityProperties = greatGrandChildWithOpacity.objectPaintProperties();
    EXPECT_EQ(0.2f, greatGrandChildWithOpacityProperties->effect()->opacity());
    EXPECT_EQ(childWithOpacityProperties->effect(), greatGrandChildWithOpacityProperties->effect()->parent());
}

TEST_F(PaintPropertyTreeBuilderTest, TransformNodeDoesNotAffectEffectNodes)
{
    setBodyInnerHTML(
        "<div id='nodeWithOpacity' style='opacity: 0.6'>"
        "  <div id='childWithTransform' style='transform: translate3d(10px, 10px, 0px);'>"
        "    <div id='grandChildWithOpacity' style='opacity: 0.4'/>"
        "  </div>"
        "</div>");

    LayoutObject& nodeWithOpacity = *document().getElementById("nodeWithOpacity")->layoutObject();
    ObjectPaintProperties* nodeWithOpacityProperties = nodeWithOpacity.objectPaintProperties();
    EXPECT_EQ(0.6f, nodeWithOpacityProperties->effect()->opacity());
    EXPECT_EQ(nullptr, nodeWithOpacityProperties->effect()->parent());
    EXPECT_EQ(nullptr, nodeWithOpacityProperties->transform());

    LayoutObject& childWithTransform = *document().getElementById("childWithTransform")->layoutObject();
    ObjectPaintProperties* childWithTransformProperties = childWithTransform.objectPaintProperties();
    EXPECT_EQ(nullptr, childWithTransformProperties->effect());
    EXPECT_EQ(TransformationMatrix().translate(10, 10), childWithTransformProperties->transform()->matrix());

    LayoutObject& grandChildWithOpacity = *document().getElementById("grandChildWithOpacity")->layoutObject();
    ObjectPaintProperties* grandChildWithOpacityProperties = grandChildWithOpacity.objectPaintProperties();
    EXPECT_EQ(0.4f, grandChildWithOpacityProperties->effect()->opacity());
    EXPECT_EQ(nodeWithOpacityProperties->effect(), grandChildWithOpacityProperties->effect()->parent());
    EXPECT_EQ(nullptr, grandChildWithOpacityProperties->transform());
}

TEST_F(PaintPropertyTreeBuilderTest, EffectNodesAcrossStackingContext)
{
    setBodyInnerHTML(
        "<div id='nodeWithOpacity' style='opacity: 0.6'>"
        "  <div id='childWithStackingContext' style='position:absolute;'>"
        "    <div id='grandChildWithOpacity' style='opacity: 0.4'/>"
        "  </div>"
        "</div>");

    LayoutObject& nodeWithOpacity = *document().getElementById("nodeWithOpacity")->layoutObject();
    ObjectPaintProperties* nodeWithOpacityProperties = nodeWithOpacity.objectPaintProperties();
    EXPECT_EQ(0.6f, nodeWithOpacityProperties->effect()->opacity());
    EXPECT_EQ(nullptr, nodeWithOpacityProperties->effect()->parent());
    EXPECT_EQ(nullptr, nodeWithOpacityProperties->transform());

    LayoutObject& childWithStackingContext = *document().getElementById("childWithStackingContext")->layoutObject();
    EXPECT_EQ(nullptr, childWithStackingContext.objectPaintProperties());

    LayoutObject& grandChildWithOpacity = *document().getElementById("grandChildWithOpacity")->layoutObject();
    ObjectPaintProperties* grandChildWithOpacityProperties = grandChildWithOpacity.objectPaintProperties();
    EXPECT_EQ(0.4f, grandChildWithOpacityProperties->effect()->opacity());
    EXPECT_EQ(nodeWithOpacityProperties->effect(), grandChildWithOpacityProperties->effect()->parent());
    EXPECT_EQ(nullptr, grandChildWithOpacityProperties->transform());
}

TEST_F(PaintPropertyTreeBuilderTest, EffectNodesInSVG)
{
    setBodyInnerHTML(
        "<svg id='svgRoot'>"
        "  <g id='groupWithOpacity' opacity='0.6'>"
        "    <rect id='rectWithoutOpacity' />"
        "    <rect id='rectWithOpacity' opacity='0.4' />"
        "    <text id='textWithOpacity' opacity='0.2'>"
        "      <tspan id='tspanWithOpacity' opacity='0.1' />"
        "    </text>"
        "  </g>"
        "</svg>");

    LayoutObject& groupWithOpacity = *document().getElementById("groupWithOpacity")->layoutObject();
    ObjectPaintProperties* groupWithOpacityProperties = groupWithOpacity.objectPaintProperties();
    EXPECT_EQ(0.6f, groupWithOpacityProperties->effect()->opacity());
    EXPECT_EQ(nullptr, groupWithOpacityProperties->effect()->parent());

    LayoutObject& rectWithoutOpacity = *document().getElementById("rectWithoutOpacity")->layoutObject();
    ObjectPaintProperties* rectWithoutOpacityProperties = rectWithoutOpacity.objectPaintProperties();
    EXPECT_EQ(nullptr, rectWithoutOpacityProperties);

    LayoutObject& rectWithOpacity = *document().getElementById("rectWithOpacity")->layoutObject();
    ObjectPaintProperties* rectWithOpacityProperties = rectWithOpacity.objectPaintProperties();
    EXPECT_EQ(0.4f, rectWithOpacityProperties->effect()->opacity());
    EXPECT_EQ(groupWithOpacityProperties->effect(), rectWithOpacityProperties->effect()->parent());

    // Ensure that opacity nodes are created for LayoutSVGText which inherits from LayoutSVGBlock instead of LayoutSVGModelObject.
    LayoutObject& textWithOpacity = *document().getElementById("textWithOpacity")->layoutObject();
    ObjectPaintProperties* textWithOpacityProperties = textWithOpacity.objectPaintProperties();
    EXPECT_EQ(0.2f, textWithOpacityProperties->effect()->opacity());
    EXPECT_EQ(groupWithOpacityProperties->effect(), textWithOpacityProperties->effect()->parent());

    // Ensure that opacity nodes are created for LayoutSVGTSpan which inherits from LayoutSVGInline instead of LayoutSVGModelObject.
    LayoutObject& tspanWithOpacity = *document().getElementById("tspanWithOpacity")->layoutObject();
    ObjectPaintProperties* tspanWithOpacityProperties = tspanWithOpacity.objectPaintProperties();
    EXPECT_EQ(0.1f, tspanWithOpacityProperties->effect()->opacity());
    EXPECT_EQ(textWithOpacityProperties->effect(), tspanWithOpacityProperties->effect()->parent());
}

TEST_F(PaintPropertyTreeBuilderTest, EffectNodesAcrossHTMLSVGBoundary)
{
    setBodyInnerHTML(
        "<div id='divWithOpacity' style='opacity: 0.2;'>"
        "  <svg id='svgRootWithOpacity' style='opacity: 0.3;'>"
        "    <rect id='rectWithOpacity' opacity='0.4' />"
        "  </svg>"
        "</div>");

    LayoutObject& divWithOpacity = *document().getElementById("divWithOpacity")->layoutObject();
    ObjectPaintProperties* divWithOpacityProperties = divWithOpacity.objectPaintProperties();
    EXPECT_EQ(0.2f, divWithOpacityProperties->effect()->opacity());
    EXPECT_EQ(nullptr, divWithOpacityProperties->effect()->parent());

    LayoutObject& svgRootWithOpacity = *document().getElementById("svgRootWithOpacity")->layoutObject();
    ObjectPaintProperties* svgRootWithOpacityProperties = svgRootWithOpacity.objectPaintProperties();
    EXPECT_EQ(0.3f, svgRootWithOpacityProperties->effect()->opacity());
    EXPECT_EQ(divWithOpacityProperties->effect(), svgRootWithOpacityProperties->effect()->parent());

    LayoutObject& rectWithOpacity = *document().getElementById("rectWithOpacity")->layoutObject();
    ObjectPaintProperties* rectWithOpacityProperties = rectWithOpacity.objectPaintProperties();
    EXPECT_EQ(0.4f, rectWithOpacityProperties->effect()->opacity());
    EXPECT_EQ(svgRootWithOpacityProperties->effect(), rectWithOpacityProperties->effect()->parent());
}

TEST_F(PaintPropertyTreeBuilderTest, EffectNodesAcrossSVGHTMLBoundary)
{
    setBodyInnerHTML(
        "<svg id='svgRootWithOpacity' style='opacity: 0.3;'>"
        "  <foreignObject id='foreignObjectWithOpacity' opacity='0.4'>"
        "    <body>"
        "      <span id='spanWithOpacity' style='opacity: 0.5'/>"
        "    </body>"
        "  </foreignObject>"
        "</svg>");

    LayoutObject& svgRootWithOpacity = *document().getElementById("svgRootWithOpacity")->layoutObject();
    ObjectPaintProperties* svgRootWithOpacityProperties = svgRootWithOpacity.objectPaintProperties();
    EXPECT_EQ(0.3f, svgRootWithOpacityProperties->effect()->opacity());
    EXPECT_EQ(nullptr, svgRootWithOpacityProperties->effect()->parent());

    LayoutObject& foreignObjectWithOpacity = *document().getElementById("foreignObjectWithOpacity")->layoutObject();
    ObjectPaintProperties* foreignObjectWithOpacityProperties = foreignObjectWithOpacity.objectPaintProperties();
    EXPECT_EQ(0.4f, foreignObjectWithOpacityProperties->effect()->opacity());
    EXPECT_EQ(svgRootWithOpacityProperties->effect(), foreignObjectWithOpacityProperties->effect()->parent());

    LayoutObject& spanWithOpacity = *document().getElementById("spanWithOpacity")->layoutObject();
    ObjectPaintProperties* spanWithOpacityProperties = spanWithOpacity.objectPaintProperties();
    EXPECT_EQ(0.5f, spanWithOpacityProperties->effect()->opacity());
    EXPECT_EQ(foreignObjectWithOpacityProperties->effect(), spanWithOpacityProperties->effect()->parent());
}

TEST_F(PaintPropertyTreeBuilderTest, TransformNodesInSVG)
{
    setBodyInnerHTML(
        "<style>"
        "  body {"
        "    margin: 0px;"
        "  }"
        "  svg {"
        "    margin-left: 50px;"
        "    transform: translate3d(1px, 2px, 3px);"
        "    position: absolute;"
        "    left: 20px;"
        "    top: 25px;"
        "  }"
        "  rect {"
        "    transform: translate(100px, 100px) rotate(45deg);"
        "    transform-origin: 50px 25px;"
        "  }"
        "</style>"
        "<svg id='svgRootWith3dTransform' width='100px' height='100px'>"
        "  <rect id='rectWith2dTransform' width='100px' height='100px' />"
        "</svg>");

    LayoutObject& svgRootWith3dTransform = *document().getElementById("svgRootWith3dTransform")->layoutObject();
    ObjectPaintProperties* svgRootWith3dTransformProperties = svgRootWith3dTransform.objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate3d(1, 2, 3), svgRootWith3dTransformProperties->transform()->matrix());
    EXPECT_EQ(FloatPoint3D(50, 50, 0), svgRootWith3dTransformProperties->transform()->origin());
    EXPECT_EQ(svgRootWith3dTransformProperties->paintOffsetTranslation(), svgRootWith3dTransformProperties->transform()->parent());
    EXPECT_EQ(TransformationMatrix().translate(70, 25), svgRootWith3dTransformProperties->paintOffsetTranslation()->matrix());
    EXPECT_EQ(document().view()->scrollTranslation(), svgRootWith3dTransformProperties->paintOffsetTranslation()->parent());

    LayoutObject& rectWith2dTransform = *document().getElementById("rectWith2dTransform")->layoutObject();
    ObjectPaintProperties* rectWith2dTransformProperties = rectWith2dTransform.objectPaintProperties();
    TransformationMatrix matrix;
    matrix.translate(100, 100);
    matrix.rotate(45);
    // SVG's transform origin is baked into the transform.
    matrix.applyTransformOrigin(50, 25, 0);
    EXPECT_EQ(matrix, rectWith2dTransformProperties->transform()->matrix());
    EXPECT_EQ(FloatPoint3D(0, 0, 0), rectWith2dTransformProperties->transform()->origin());
    // SVG does not use paint offset.
    EXPECT_EQ(nullptr, rectWith2dTransformProperties->paintOffsetTranslation());
}

TEST_F(PaintPropertyTreeBuilderTest, SVGRootPaintOffsetTransformNode)
{
    setBodyInnerHTML(
        "<style>body { margin: 0px; } </style>"
        "<svg id='svg' style='margin-left: 50px; margin-top: 25px; width: 100px; height: 100px;' />");

    LayoutObject& svg = *document().getElementById("svg")->layoutObject();
    ObjectPaintProperties* svgProperties = svg.objectPaintProperties();
    // Ensure that a paint offset transform is emitted for SVG, even without a CSS transform.
    EXPECT_EQ(nullptr, svgProperties->transform());
    EXPECT_EQ(TransformationMatrix().translate(50, 25), svgProperties->paintOffsetTranslation()->matrix());
    EXPECT_EQ(document().view()->scrollTranslation(), svgProperties->paintOffsetTranslation()->parent());
}

TEST_F(PaintPropertyTreeBuilderTest, TransformNodesAcrossSVGHTMLBoundary)
{
    setBodyInnerHTML(
        "<style> body { margin: 0px; } </style>"
        "<svg id='svgWithTransform' style='transform: translate3d(1px, 2px, 3px);'>"
        "  <foreignObject>"
        "    <body>"
        "      <div id='divWithTransform' style='transform: translate3d(3px, 4px, 5px);'></div>"
        "    </body>"
        "  </foreignObject>"
        "</svg>");

    LayoutObject& svgWithTransform = *document().getElementById("svgWithTransform")->layoutObject();
    ObjectPaintProperties* svgWithTransformProperties = svgWithTransform.objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate3d(1, 2, 3), svgWithTransformProperties->transform()->matrix());

    LayoutObject& divWithTransform = *document().getElementById("divWithTransform")->layoutObject();
    ObjectPaintProperties* divWithTransformProperties = divWithTransform.objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate3d(3, 4, 5), divWithTransformProperties->transform()->matrix());
    // Ensure the div's transform node is a child of the svg's transform node.
    EXPECT_EQ(svgWithTransformProperties->transform(), divWithTransformProperties->transform()->parent());
}

TEST_F(PaintPropertyTreeBuilderTest, FixedTransformAncestorAcrossSVGHTMLBoundary)
{
    setBodyInnerHTML(
        "<style> body { margin: 0px; } </style>"
        "<svg id='svg' style='transform: translate3d(1px, 2px, 3px);'>"
        "  <g id='container' transform='translate(20 30)'>"
        "    <foreignObject>"
        "      <body>"
        "        <div id='fixed' style='position: fixed; left: 200px; top: 150px;'></div>"
        "      </body>"
        "    </foreignObject>"
        "  </g>"
        "</svg>");

    LayoutObject& svg = *document().getElementById("svg")->layoutObject();
    ObjectPaintProperties* svgProperties = svg.objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate3d(1, 2, 3), svgProperties->transform()->matrix());

    LayoutObject& container = *document().getElementById("container")->layoutObject();
    ObjectPaintProperties* containerProperties = container.objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate(20, 30), containerProperties->transform()->matrix());
    EXPECT_EQ(svgProperties->transform(), containerProperties->transform()->parent());

    Element* fixed = document().getElementById("fixed");
    ObjectPaintProperties* fixedProperties = fixed->layoutObject()->objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate(200, 150), fixedProperties->paintOffsetTranslation()->matrix());
    // Ensure the fixed position element is rooted at the nearest transform container.
    EXPECT_EQ(containerProperties->transform(), fixedProperties->paintOffsetTranslation()->parent());
}

} // namespace blink
