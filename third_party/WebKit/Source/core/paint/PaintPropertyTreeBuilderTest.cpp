// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutTreeAsText.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/paint/ObjectPaintProperties.h"
#include "platform/graphics/paint/GeometryMapper.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/text/TextStream.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"

namespace blink {

class PaintPropertyTreeBuilderTest : public RenderingTest {
public:
    PaintPropertyTreeBuilderTest()
        : RenderingTest(SingleChildFrameLoaderClient::create())
        , m_originalSlimmingPaintV2Enabled(RuntimeEnabledFeatures::slimmingPaintV2Enabled()) { }

    void loadTestData(const char* fileName)
    {
        String fullPath = testing::blinkRootDir();
        fullPath.append("/Source/core/paint/test_data/");
        fullPath.append(fileName);
        RefPtr<SharedBuffer> inputBuffer = testing::readFromFile(fullPath);
        setBodyInnerHTML(String(inputBuffer->data(), inputBuffer->size()));
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

#define CHECK_VISUAL_RECT(sourceLayoutObject, ancestorLayoutObject, slopFactor) \
do { \
    GeometryMapper geometryMapper; \
    LayoutRect source(sourceLayoutObject->localOverflowRectForPaintInvalidation()); \
    source.moveBy(sourceLayoutObject->objectPaintProperties()->localBorderBoxProperties()->paintOffset); \
    bool success = false; \
    FloatRect actual = geometryMapper.mapToVisualRectInDestinationSpace( \
        FloatRect(source), \
        sourceLayoutObject->objectPaintProperties()->localBorderBoxProperties()->propertyTreeState, \
        ancestorLayoutObject->objectPaintProperties()->localBorderBoxProperties()->propertyTreeState, success); \
    EXPECT_TRUE(success); \
\
    LayoutRect expected = sourceLayoutObject->localOverflowRectForPaintInvalidation(); \
    sourceLayoutObject->mapToVisualRectInAncestorSpace(ancestorLayoutObject, expected); \
    EXPECT_TRUE(expected.contains(LayoutRect(actual))); \
    actual.inflate(slopFactor); \
    EXPECT_TRUE(LayoutRect(actual).contains(expected)); \
} while (0);

#define CHECK_EXACT_VISUAL_RECT(sourceLayoutObject, ancestorLayoutObject) CHECK_VISUAL_RECT(sourceLayoutObject, ancestorLayoutObject, 0)

TEST_F(PaintPropertyTreeBuilderTest, FixedPosition)
{
    loadTestData("fixed-position.html");

    FrameView* frameView = document().view();

    // target1 is a fixed-position element inside an absolute-position scrolling element.
    // It should be attached under the viewport to skip scrolling and offset of the parent.
    Element* target1 = document().getElementById("target1");
    const ObjectPaintProperties* target1Properties = target1->layoutObject()->objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate(200, 150), target1Properties->paintOffsetTranslation()->matrix());
    EXPECT_EQ(frameView->preTranslation(), target1Properties->paintOffsetTranslation()->parent());
    EXPECT_EQ(target1Properties->paintOffsetTranslation(), target1Properties->overflowClip()->localTransformSpace());
    EXPECT_EQ(FloatRoundedRect(0, 0, 100, 100), target1Properties->overflowClip()->clipRect());
    // Likewise, it inherits clip from the viewport, skipping overflow clip of the scroller.
    EXPECT_EQ(frameView->contentClip(), target1Properties->overflowClip()->parent());
    CHECK_EXACT_VISUAL_RECT(target1->layoutObject(), frameView->layoutView());

    // target2 is a fixed-position element inside a transformed scrolling element.
    // It should be attached under the scrolled box of the transformed element.
    Element* target2 = document().getElementById("target2");
    const ObjectPaintProperties* target2Properties = target2->layoutObject()->objectPaintProperties();
    Element* scroller = document().getElementById("scroller");
    const ObjectPaintProperties* scrollerProperties = scroller->layoutObject()->objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate(200, 150), target2Properties->paintOffsetTranslation()->matrix());
    EXPECT_EQ(scrollerProperties->scrollTranslation(), target2Properties->paintOffsetTranslation()->parent());
    EXPECT_EQ(target2Properties->paintOffsetTranslation(), target2Properties->overflowClip()->localTransformSpace());
    EXPECT_EQ(FloatRoundedRect(0, 0, 100, 100), target2Properties->overflowClip()->clipRect());
    EXPECT_EQ(scrollerProperties->overflowClip(), target2Properties->overflowClip()->parent());
    CHECK_EXACT_VISUAL_RECT(target2->layoutObject(), frameView->layoutView());
}

TEST_F(PaintPropertyTreeBuilderTest, PositionAndScroll)
{
    loadTestData("position-and-scroll.html");

    Element* scroller = document().getElementById("scroller");
    scroller->scrollTo(0, 100);
    FrameView* frameView = document().view();
    frameView->updateAllLifecyclePhases();
    const ObjectPaintProperties* scrollerProperties = scroller->layoutObject()->objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate(0, -100), scrollerProperties->scrollTranslation()->matrix());
    EXPECT_EQ(frameView->scrollTranslation(), scrollerProperties->scrollTranslation()->parent());
    EXPECT_EQ(frameView->scrollTranslation(), scrollerProperties->overflowClip()->localTransformSpace());
    EXPECT_EQ(FloatRoundedRect(120, 340, 400, 300), scrollerProperties->overflowClip()->clipRect());
    EXPECT_EQ(frameView->contentClip(), scrollerProperties->overflowClip()->parent());
    CHECK_EXACT_VISUAL_RECT(scroller->layoutObject(), frameView->layoutView());

    // The relative-positioned element should have accumulated box offset (exclude scrolling),
    // and should be affected by ancestor scroll transforms.
    Element* relPos = document().getElementById("rel-pos");
    const ObjectPaintProperties* relPosProperties = relPos->layoutObject()->objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate(680, 1120), relPosProperties->paintOffsetTranslation()->matrix());
    EXPECT_EQ(scrollerProperties->scrollTranslation(), relPosProperties->paintOffsetTranslation()->parent());
    EXPECT_EQ(relPosProperties->transform(), relPosProperties->overflowClip()->localTransformSpace());
    EXPECT_EQ(FloatRoundedRect(0, 0, 100, 200), relPosProperties->overflowClip()->clipRect());
    EXPECT_EQ(scrollerProperties->overflowClip(), relPosProperties->overflowClip()->parent());
    CHECK_EXACT_VISUAL_RECT(relPos->layoutObject(), frameView->layoutView());

    // The absolute-positioned element should not be affected by non-positioned scroller at all.
    Element* absPos = document().getElementById("abs-pos");
    const ObjectPaintProperties* absPosProperties = absPos->layoutObject()->objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate(123, 456), absPosProperties->paintOffsetTranslation()->matrix());
    EXPECT_EQ(frameView->scrollTranslation(), absPosProperties->paintOffsetTranslation()->parent());
    EXPECT_EQ(absPosProperties->transform(), absPosProperties->overflowClip()->localTransformSpace());
    EXPECT_EQ(FloatRoundedRect(0, 0, 300, 400), absPosProperties->overflowClip()->clipRect());
    EXPECT_EQ(frameView->contentClip(), absPosProperties->overflowClip()->parent());
    CHECK_EXACT_VISUAL_RECT(absPos->layoutObject(), frameView->layoutView());
}

TEST_F(PaintPropertyTreeBuilderTest, FrameScrollingTraditional)
{
    setBodyInnerHTML("<style> body { height: 10000px; } </style>");

    document().domWindow()->scrollTo(0, 100);

    FrameView* frameView = document().view();
    frameView->updateAllLifecyclePhases();
    EXPECT_EQ(TransformationMatrix(), frameView->preTranslation()->matrix());
    EXPECT_EQ(frameView->rootTransform(), frameView->preTranslation()->parent());
    EXPECT_EQ(nullptr, frameView->rootTransform()->parent());
    EXPECT_EQ(TransformationMatrix().translate(0, -100), frameView->scrollTranslation()->matrix());
    EXPECT_EQ(frameView->preTranslation(), frameView->scrollTranslation()->parent());
    EXPECT_EQ(frameView->preTranslation(), frameView->contentClip()->localTransformSpace());
    EXPECT_EQ(FloatRoundedRect(0, 0, 800, 600), frameView->contentClip()->clipRect());
    EXPECT_EQ(frameView->rootClip(), frameView->contentClip()->parent());
    EXPECT_EQ(nullptr, frameView->rootClip()->parent());

    LayoutViewItem layoutViewItem = document().layoutViewItem();
    const ObjectPaintProperties* layoutViewProperties = layoutViewItem.objectPaintProperties();
    EXPECT_EQ(nullptr, layoutViewProperties->scrollTranslation());
    CHECK_EXACT_VISUAL_RECT(document().body()->layoutObject(), frameView->layoutView());
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

    LayoutViewItem layoutViewItem = document().layoutViewItem();
    const ObjectPaintProperties* layoutViewProperties = layoutViewItem.objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate(0, -100), layoutViewProperties->scrollTranslation()->matrix());
    EXPECT_EQ(frameView->scrollTranslation(), layoutViewProperties->scrollTranslation()->parent());
    CHECK_EXACT_VISUAL_RECT(document().body()->layoutObject(), frameView->layoutView());
}

TEST_F(PaintPropertyTreeBuilderTest, Perspective)
{
    loadTestData("perspective.html");

    Element* perspective = document().getElementById("perspective");
    const ObjectPaintProperties* perspectiveProperties = perspective->layoutObject()->objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().applyPerspective(100), perspectiveProperties->perspective()->matrix());
    // The perspective origin is the center of the border box plus accumulated paint offset.
    EXPECT_EQ(FloatPoint3D(250, 250, 0), perspectiveProperties->perspective()->origin());
    EXPECT_EQ(document().view()->scrollTranslation(), perspectiveProperties->perspective()->parent());

    // Adding perspective doesn't clear paint offset. The paint offset will be passed down to children.
    Element* inner = document().getElementById("inner");
    const ObjectPaintProperties* innerProperties = inner->layoutObject()->objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate(50, 100), innerProperties->paintOffsetTranslation()->matrix());
    EXPECT_EQ(perspectiveProperties->perspective(), innerProperties->paintOffsetTranslation()->parent());
    CHECK_EXACT_VISUAL_RECT(inner->layoutObject(), document().view()->layoutView());
}

TEST_F(PaintPropertyTreeBuilderTest, Transform)
{
    loadTestData("transform.html");

    Element* transform = document().getElementById("transform");
    const ObjectPaintProperties* transformProperties = transform->layoutObject()->objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate3d(123, 456, 789), transformProperties->transform()->matrix());
    EXPECT_EQ(FloatPoint3D(200, 150, 0), transformProperties->transform()->origin());
    EXPECT_EQ(transformProperties->paintOffsetTranslation(), transformProperties->transform()->parent());
    EXPECT_EQ(TransformationMatrix().translate(50, 100), transformProperties->paintOffsetTranslation()->matrix());
    EXPECT_EQ(document().view()->scrollTranslation(), transformProperties->paintOffsetTranslation()->parent());
    CHECK_EXACT_VISUAL_RECT(transform->layoutObject(), document().view()->layoutView());
}

TEST_F(PaintPropertyTreeBuilderTest, RelativePositionInline)
{
    loadTestData("relative-position-inline.html");

    Element* inlineBlock = document().getElementById("inline-block");
    const ObjectPaintProperties* inlineBlockProperties = inlineBlock->layoutObject()->objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate(135, 490), inlineBlockProperties->paintOffsetTranslation()->matrix());
    EXPECT_EQ(document().view()->scrollTranslation(), inlineBlockProperties->paintOffsetTranslation()->parent());
    CHECK_EXACT_VISUAL_RECT(inlineBlock->layoutObject(), document().view()->layoutView());
}

TEST_F(PaintPropertyTreeBuilderTest, NestedOpacityEffect)
{
    setBodyInnerHTML(
        "<div id='nodeWithoutOpacity' style='width: 100px; height: 200px'>"
        "  <div id='childWithOpacity' style='opacity: 0.5; width: 50px; height: 60px;'>"
        "    <div id='grandChildWithoutOpacity' style='width: 20px; height: 30px'>"
        "      <div id='greatGrandChildWithOpacity' style='opacity: 0.2; width: 10px; height: 15px'/>"
        "    </div>"
        "  </div>"
        "</div>");

    LayoutObject* nodeWithoutOpacity = document().getElementById("nodeWithoutOpacity")->layoutObject();
    const ObjectPaintProperties* nodeWithoutOpacityProperties = nodeWithoutOpacity->objectPaintProperties();
    EXPECT_NE(nullptr, nodeWithoutOpacityProperties);
    CHECK_EXACT_VISUAL_RECT(nodeWithoutOpacity, document().view()->layoutView());

    LayoutObject* childWithOpacity = document().getElementById("childWithOpacity")->layoutObject();
    const ObjectPaintProperties* childWithOpacityProperties = childWithOpacity->objectPaintProperties();
    EXPECT_EQ(0.5f, childWithOpacityProperties->effect()->opacity());
    // childWithOpacity is the root effect node.
    EXPECT_NE(nullptr, childWithOpacityProperties->effect()->parent());
    CHECK_EXACT_VISUAL_RECT(childWithOpacity, document().view()->layoutView());

    LayoutObject* grandChildWithoutOpacity = document().getElementById("grandChildWithoutOpacity")->layoutObject();
    EXPECT_NE(nullptr, grandChildWithoutOpacity->objectPaintProperties());
    CHECK_EXACT_VISUAL_RECT(grandChildWithoutOpacity, document().view()->layoutView());

    LayoutObject* greatGrandChildWithOpacity = document().getElementById("greatGrandChildWithOpacity")->layoutObject();
    const ObjectPaintProperties* greatGrandChildWithOpacityProperties = greatGrandChildWithOpacity->objectPaintProperties();
    EXPECT_EQ(0.2f, greatGrandChildWithOpacityProperties->effect()->opacity());
    EXPECT_EQ(childWithOpacityProperties->effect(), greatGrandChildWithOpacityProperties->effect()->parent());
    CHECK_EXACT_VISUAL_RECT(greatGrandChildWithOpacity, document().view()->layoutView());
}

TEST_F(PaintPropertyTreeBuilderTest, TransformNodeDoesNotAffectEffectNodes)
{
    setBodyInnerHTML(
        "<div id='nodeWithOpacity' style='opacity: 0.6' style='width: 100px; height: 200px'>"
        "  <div id='childWithTransform' style='transform: translate3d(10px, 10px, 0px); width: 50px; height: 60px;'>"
        "    <div id='grandChildWithOpacity' style='opacity: 0.4; width: 20px; height: 30px'/>"
        "  </div>"
        "</div>");

    LayoutObject* nodeWithOpacity = document().getElementById("nodeWithOpacity")->layoutObject();
    const ObjectPaintProperties* nodeWithOpacityProperties = nodeWithOpacity->objectPaintProperties();
    EXPECT_EQ(0.6f, nodeWithOpacityProperties->effect()->opacity());
    EXPECT_NE(nullptr, nodeWithOpacityProperties->effect()->parent());
    EXPECT_EQ(nullptr, nodeWithOpacityProperties->transform());
    CHECK_EXACT_VISUAL_RECT(nodeWithOpacity, document().view()->layoutView());

    LayoutObject* childWithTransform = document().getElementById("childWithTransform")->layoutObject();
    const ObjectPaintProperties* childWithTransformProperties = childWithTransform->objectPaintProperties();
    EXPECT_EQ(nullptr, childWithTransformProperties->effect());
    EXPECT_EQ(TransformationMatrix().translate(10, 10), childWithTransformProperties->transform()->matrix());
    CHECK_EXACT_VISUAL_RECT(childWithTransform, document().view()->layoutView());

    LayoutObject* grandChildWithOpacity = document().getElementById("grandChildWithOpacity")->layoutObject();
    const ObjectPaintProperties* grandChildWithOpacityProperties = grandChildWithOpacity->objectPaintProperties();
    EXPECT_EQ(0.4f, grandChildWithOpacityProperties->effect()->opacity());
    EXPECT_EQ(nodeWithOpacityProperties->effect(), grandChildWithOpacityProperties->effect()->parent());
    EXPECT_EQ(nullptr, grandChildWithOpacityProperties->transform());
    CHECK_EXACT_VISUAL_RECT(grandChildWithOpacity, document().view()->layoutView());
}

TEST_F(PaintPropertyTreeBuilderTest, EffectNodesAcrossStackingContext)
{
    setBodyInnerHTML(
        "<div id='nodeWithOpacity' style='opacity: 0.6; width: 100px; height: 200px'>"
        "  <div id='childWithStackingContext' style='position:absolute; width: 50px; height: 60px;'>"
        "    <div id='grandChildWithOpacity' style='opacity: 0.4; width: 20px; height: 30px'/>"
        "  </div>"
        "</div>");

    LayoutObject* nodeWithOpacity = document().getElementById("nodeWithOpacity")->layoutObject();
    const ObjectPaintProperties* nodeWithOpacityProperties = nodeWithOpacity->objectPaintProperties();
    EXPECT_EQ(0.6f, nodeWithOpacityProperties->effect()->opacity());
    EXPECT_NE(nullptr, nodeWithOpacityProperties->effect()->parent());
    EXPECT_EQ(nullptr, nodeWithOpacityProperties->transform());
    CHECK_EXACT_VISUAL_RECT(nodeWithOpacity, document().view()->layoutView());

    LayoutObject* childWithStackingContext = document().getElementById("childWithStackingContext")->layoutObject();
    const ObjectPaintProperties* childWithStackingContextProperties = childWithStackingContext->objectPaintProperties();
    EXPECT_EQ(nullptr, childWithStackingContextProperties->effect());
    EXPECT_EQ(nullptr, childWithStackingContextProperties->transform());
    CHECK_EXACT_VISUAL_RECT(childWithStackingContext, document().view()->layoutView());

    LayoutObject* grandChildWithOpacity = document().getElementById("grandChildWithOpacity")->layoutObject();
    const ObjectPaintProperties* grandChildWithOpacityProperties = grandChildWithOpacity->objectPaintProperties();
    EXPECT_EQ(0.4f, grandChildWithOpacityProperties->effect()->opacity());
    EXPECT_EQ(nodeWithOpacityProperties->effect(), grandChildWithOpacityProperties->effect()->parent());
    EXPECT_EQ(nullptr, grandChildWithOpacityProperties->transform());
    CHECK_EXACT_VISUAL_RECT(grandChildWithOpacity, document().view()->layoutView());
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

    LayoutObject* groupWithOpacity = document().getElementById("groupWithOpacity")->layoutObject();
    const ObjectPaintProperties* groupWithOpacityProperties = groupWithOpacity->objectPaintProperties();
    EXPECT_EQ(0.6f, groupWithOpacityProperties->effect()->opacity());
    EXPECT_NE(nullptr, groupWithOpacityProperties->effect()->parent());

    LayoutObject& rectWithoutOpacity = *document().getElementById("rectWithoutOpacity")->layoutObject();
    const ObjectPaintProperties* rectWithoutOpacityProperties = rectWithoutOpacity.objectPaintProperties();
    EXPECT_EQ(nullptr, rectWithoutOpacityProperties);

    LayoutObject& rectWithOpacity = *document().getElementById("rectWithOpacity")->layoutObject();
    const ObjectPaintProperties* rectWithOpacityProperties = rectWithOpacity.objectPaintProperties();
    EXPECT_EQ(0.4f, rectWithOpacityProperties->effect()->opacity());
    EXPECT_EQ(groupWithOpacityProperties->effect(), rectWithOpacityProperties->effect()->parent());

    // Ensure that opacity nodes are created for LayoutSVGText which inherits from LayoutSVGBlock instead of LayoutSVGModelObject.
    LayoutObject& textWithOpacity = *document().getElementById("textWithOpacity")->layoutObject();
    const ObjectPaintProperties* textWithOpacityProperties = textWithOpacity.objectPaintProperties();
    EXPECT_EQ(0.2f, textWithOpacityProperties->effect()->opacity());
    EXPECT_EQ(groupWithOpacityProperties->effect(), textWithOpacityProperties->effect()->parent());

    // Ensure that opacity nodes are created for LayoutSVGTSpan which inherits from LayoutSVGInline instead of LayoutSVGModelObject.
    LayoutObject& tspanWithOpacity = *document().getElementById("tspanWithOpacity")->layoutObject();
    const ObjectPaintProperties* tspanWithOpacityProperties = tspanWithOpacity.objectPaintProperties();
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
    const ObjectPaintProperties* divWithOpacityProperties = divWithOpacity.objectPaintProperties();
    EXPECT_EQ(0.2f, divWithOpacityProperties->effect()->opacity());
    EXPECT_NE(nullptr, divWithOpacityProperties->effect()->parent());

    LayoutObject& svgRootWithOpacity = *document().getElementById("svgRootWithOpacity")->layoutObject();
    const ObjectPaintProperties* svgRootWithOpacityProperties = svgRootWithOpacity.objectPaintProperties();
    EXPECT_EQ(0.3f, svgRootWithOpacityProperties->effect()->opacity());
    EXPECT_EQ(divWithOpacityProperties->effect(), svgRootWithOpacityProperties->effect()->parent());

    LayoutObject& rectWithOpacity = *document().getElementById("rectWithOpacity")->layoutObject();
    const ObjectPaintProperties* rectWithOpacityProperties = rectWithOpacity.objectPaintProperties();
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
    const ObjectPaintProperties* svgRootWithOpacityProperties = svgRootWithOpacity.objectPaintProperties();
    EXPECT_EQ(0.3f, svgRootWithOpacityProperties->effect()->opacity());
    EXPECT_NE(nullptr, svgRootWithOpacityProperties->effect()->parent());

    LayoutObject& foreignObjectWithOpacity = *document().getElementById("foreignObjectWithOpacity")->layoutObject();
    const ObjectPaintProperties* foreignObjectWithOpacityProperties = foreignObjectWithOpacity.objectPaintProperties();
    EXPECT_EQ(0.4f, foreignObjectWithOpacityProperties->effect()->opacity());
    EXPECT_EQ(svgRootWithOpacityProperties->effect(), foreignObjectWithOpacityProperties->effect()->parent());

    LayoutObject& spanWithOpacity = *document().getElementById("spanWithOpacity")->layoutObject();
    const ObjectPaintProperties* spanWithOpacityProperties = spanWithOpacity.objectPaintProperties();
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
    const ObjectPaintProperties* svgRootWith3dTransformProperties = svgRootWith3dTransform.objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate3d(1, 2, 3), svgRootWith3dTransformProperties->transform()->matrix());
    EXPECT_EQ(FloatPoint3D(50, 50, 0), svgRootWith3dTransformProperties->transform()->origin());
    EXPECT_EQ(svgRootWith3dTransformProperties->paintOffsetTranslation(), svgRootWith3dTransformProperties->transform()->parent());
    EXPECT_EQ(TransformationMatrix().translate(70, 25), svgRootWith3dTransformProperties->paintOffsetTranslation()->matrix());
    EXPECT_EQ(document().view()->scrollTranslation(), svgRootWith3dTransformProperties->paintOffsetTranslation()->parent());

    LayoutObject& rectWith2dTransform = *document().getElementById("rectWith2dTransform")->layoutObject();
    const ObjectPaintProperties* rectWith2dTransformProperties = rectWith2dTransform.objectPaintProperties();
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

TEST_F(PaintPropertyTreeBuilderTest, SVGViewBoxTransform)
{
    setBodyInnerHTML(
        "<style>"
        "  body {"
        "    margin: 0px;"
        "  }"
        "  svg {"
        "    transform: translate3d(1px, 2px, 3px);"
        "    position: absolute;"
        "  }"
        "  rect {"
        "    transform: translate(100px, 100px);"
        "  }"
        "</style>"
        "<svg id='svgWithViewBox' width='100px' height='100px' viewBox='50 50 100 100'>"
        "  <rect id='rect' width='100px' height='100px' />"
        "</svg>");

    LayoutObject& svgWithViewBox = *document().getElementById("svgWithViewBox")->layoutObject();
    const ObjectPaintProperties* svgWithViewBoxProperties = svgWithViewBox.objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate3d(1, 2, 3), svgWithViewBoxProperties->transform()->matrix());
    EXPECT_EQ(TransformationMatrix().translate(-50, -50), svgWithViewBoxProperties->svgLocalToBorderBoxTransform()->matrix());
    EXPECT_EQ(svgWithViewBoxProperties->svgLocalToBorderBoxTransform()->parent(), svgWithViewBoxProperties->transform());

    LayoutObject& rect = *document().getElementById("rect")->layoutObject();
    const ObjectPaintProperties* rectProperties = rect.objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate(100, 100), rectProperties->transform()->matrix());
    EXPECT_EQ(svgWithViewBoxProperties->svgLocalToBorderBoxTransform(), rectProperties->transform()->parent());
}

TEST_F(PaintPropertyTreeBuilderTest, SVGRootPaintOffsetTransformNode)
{
    setBodyInnerHTML(
        "<style>body { margin: 0px; } </style>"
        "<svg id='svg' style='margin-left: 50px; margin-top: 25px; width: 100px; height: 100px;' />");

    LayoutObject& svg = *document().getElementById("svg")->layoutObject();
    const ObjectPaintProperties* svgProperties = svg.objectPaintProperties();
    // Ensure that a paint offset transform is not unnecessarily emitted.
    EXPECT_EQ(nullptr, svgProperties->paintOffsetTranslation());
    EXPECT_EQ(TransformationMatrix().translate(50, 25), svgProperties->svgLocalToBorderBoxTransform()->matrix());
    EXPECT_EQ(document().view()->scrollTranslation(), svgProperties->svgLocalToBorderBoxTransform()->parent());
}

TEST_F(PaintPropertyTreeBuilderTest, SVGRootLocalToBorderBoxTransformNode)
{
    setBodyInnerHTML(
        "<style>"
        "  body { margin: 0px; }"
        "  svg { margin-left: 2px; margin-top: 3px; transform: translate(5px, 7px); border: 11px solid green; }"
        "</style>"
        "<svg id='svg' width='100px' height='100px' viewBox='0 0 13 13'>"
        "  <rect id='rect' transform='translate(17 19)' />"
        "</svg>");

    LayoutObject& svg = *document().getElementById("svg")->layoutObject();
    const ObjectPaintProperties* svgProperties = svg.objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate(2, 3), svgProperties->paintOffsetTranslation()->matrix());
    EXPECT_EQ(TransformationMatrix().translate(5, 7), svgProperties->transform()->matrix());
    EXPECT_EQ(TransformationMatrix().translate(11, 11).scale(100.0 / 13.0), svgProperties->svgLocalToBorderBoxTransform()->matrix());
    EXPECT_EQ(svgProperties->paintOffsetTranslation(), svgProperties->transform()->parent());
    EXPECT_EQ(svgProperties->transform(), svgProperties->svgLocalToBorderBoxTransform()->parent());

    // Ensure the rect's transform is a child of the local to border box transform.
    LayoutObject& rect = *document().getElementById("rect")->layoutObject();
    const ObjectPaintProperties* rectProperties = rect.objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate(17, 19), rectProperties->transform()->matrix());
    EXPECT_EQ(svgProperties->svgLocalToBorderBoxTransform(), rectProperties->transform()->parent());
}

TEST_F(PaintPropertyTreeBuilderTest, SVGNestedViewboxTransforms)
{
    setBodyInnerHTML(
        "<style>body { margin: 0px; } </style>"
        "<svg id='svg' width='100px' height='100px' viewBox='0 0 50 50' style='transform: translate(11px, 11px);'>"
        "  <svg id='nestedSvg' width='50px' height='50px' viewBox='0 0 5 5'>"
        "    <rect id='rect' transform='translate(13 13)' />"
        "  </svg>"
        "</svg>");

    LayoutObject& svg = *document().getElementById("svg")->layoutObject();
    const ObjectPaintProperties* svgProperties = svg.objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate(11, 11), svgProperties->transform()->matrix());
    EXPECT_EQ(TransformationMatrix().scale(2), svgProperties->svgLocalToBorderBoxTransform()->matrix());

    LayoutObject& nestedSvg = *document().getElementById("nestedSvg")->layoutObject();
    const ObjectPaintProperties* nestedSvgProperties = nestedSvg.objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().scale(10), nestedSvgProperties->transform()->matrix());
    EXPECT_EQ(nullptr, nestedSvgProperties->svgLocalToBorderBoxTransform());
    EXPECT_EQ(svgProperties->svgLocalToBorderBoxTransform(), nestedSvgProperties->transform()->parent());

    LayoutObject& rect = *document().getElementById("rect")->layoutObject();
    const ObjectPaintProperties* rectProperties = rect.objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate(13, 13), rectProperties->transform()->matrix());
    EXPECT_EQ(nestedSvgProperties->transform(), rectProperties->transform()->parent());
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
    const ObjectPaintProperties* svgWithTransformProperties = svgWithTransform.objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate3d(1, 2, 3), svgWithTransformProperties->transform()->matrix());

    LayoutObject& divWithTransform = *document().getElementById("divWithTransform")->layoutObject();
    const ObjectPaintProperties* divWithTransformProperties = divWithTransform.objectPaintProperties();
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
    const ObjectPaintProperties* svgProperties = svg.objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate3d(1, 2, 3), svgProperties->transform()->matrix());

    LayoutObject& container = *document().getElementById("container")->layoutObject();
    const ObjectPaintProperties* containerProperties = container.objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate(20, 30), containerProperties->transform()->matrix());
    EXPECT_EQ(svgProperties->transform(), containerProperties->transform()->parent());

    Element* fixed = document().getElementById("fixed");
    const ObjectPaintProperties* fixedProperties = fixed->layoutObject()->objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate(200, 150), fixedProperties->paintOffsetTranslation()->matrix());
    // Ensure the fixed position element is rooted at the nearest transform container.
    EXPECT_EQ(containerProperties->transform(), fixedProperties->paintOffsetTranslation()->parent());
}

TEST_F(PaintPropertyTreeBuilderTest, ControlClip)
{
    setBodyInnerHTML(
        "<style>"
        "  body {"
        "    margin: 0;"
        "  }"
        "  input {"
        "    border-width: 5px;"
        "    padding: 0;"
        "  }"
        "</style>"
        "<input id='button' type='button' style='width:345px; height:123px' value='some text'/>");

    FrameView* frameView = document().view();
    LayoutObject* button = document().getElementById("button")->layoutObject();
    const ObjectPaintProperties* buttonProperties = button->objectPaintProperties();
    EXPECT_EQ(frameView->scrollTranslation(), buttonProperties->overflowClip()->localTransformSpace());
    EXPECT_EQ(FloatRoundedRect(5, 5, 335, 113), buttonProperties->overflowClip()->clipRect());
    EXPECT_EQ(frameView->contentClip(), buttonProperties->overflowClip()->parent());
    CHECK_EXACT_VISUAL_RECT(button, frameView->layoutView());
}

TEST_F(PaintPropertyTreeBuilderTest, BorderRadiusClip)
{
    setBodyInnerHTML(
        "<style>"
        " body {"
        "   margin: 0px;"
        " }"
        " #div {"
        "   border-radius: 12px 34px 56px 78px;"
        "   border-top: 45px solid;"
        "   border-right: 50px solid;"
        "   border-bottom: 55px solid;"
        "   border-left: 60px solid;"
        "   width: 500px;"
        "   height: 400px;"
        "   overflow: scroll;"
        " }"
        "</style>"
        "<div id='div'></div>");

    FrameView* frameView = document().view();
    LayoutObject* div = document().getElementById("div")->layoutObject();
    const ObjectPaintProperties* divProperties = div->objectPaintProperties();
    EXPECT_EQ(frameView->scrollTranslation(), divProperties->overflowClip()->localTransformSpace());
    // The overflow clip rect includes only the padding box.
    // padding box = border box(500+60+50, 400+45+55) - border outset(60+50, 45+55) - scrollbars(15, 15)
    EXPECT_EQ(FloatRoundedRect(60, 45, 500, 400), divProperties->overflowClip()->clipRect());
    const ClipPaintPropertyNode* borderRadiusClip = divProperties->overflowClip()->parent();
    EXPECT_EQ(frameView->scrollTranslation(), borderRadiusClip->localTransformSpace());
    // The border radius clip is the area enclosed by inner border edge, including the scrollbars.
    // As the border-radius is specified in outer radius, the inner radius is calculated by:
    // inner radius = max(outer radius - border width, 0)
    // In the case that two adjacent borders have different width, the inner radius of the corner
    // may transition from one value to the other. i.e. being an ellipse.
    EXPECT_EQ(
        FloatRoundedRect(
            FloatRect(60, 45, 500, 400), // = border box(610, 500) - border outset(110, 100)
            FloatSize(0, 0), //   (top left)     = max((12, 12) - (60, 45), (0, 0))
            FloatSize(0, 0), //   (top right)    = max((34, 34) - (50, 45), (0, 0))
            FloatSize(18, 23), // (bottom left)  = max((78, 78) - (60, 55), (0, 0))
            FloatSize(6, 1)), //  (bottom right) = max((56, 56) - (50, 55), (0, 0))
        borderRadiusClip->clipRect());
    EXPECT_EQ(frameView->contentClip(), borderRadiusClip->parent());
    CHECK_EXACT_VISUAL_RECT(div, frameView->layoutView());
}

TEST_F(PaintPropertyTreeBuilderTest, TransformNodesAcrossSubframes)
{
    setBodyInnerHTML(
        "<style>body { margin: 0; }</style>"
        "<div id='divWithTransform' style='transform: translate3d(1px, 2px, 3px);'>"
        "  <iframe id='frame'></iframe>"
        "</div>");
    Document& frameDocument = setupChildIframe("frame",
        "<style>body { margin: 0; }</style><div id='transform' style='transform: translate3d(4px, 5px, 6px); width: 100px; height: 200px'></div>");
    FrameView* frameView = document().view();
    frameView->updateAllLifecyclePhases();

    LayoutObject* divWithTransform = document().getElementById("divWithTransform")->layoutObject();
    const ObjectPaintProperties* divWithTransformProperties = divWithTransform->objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate3d(1, 2, 3), divWithTransformProperties->transform()->matrix());
    CHECK_EXACT_VISUAL_RECT(divWithTransform, frameView->layoutView());

    LayoutObject* innerDivWithTransform = frameDocument.getElementById("transform")->layoutObject();
    const ObjectPaintProperties* innerDivWithTransformProperties = innerDivWithTransform->objectPaintProperties();
    auto* innerDivTransform = innerDivWithTransformProperties->transform();
    EXPECT_EQ(TransformationMatrix().translate3d(4, 5, 6), innerDivTransform->matrix());
    CHECK_EXACT_VISUAL_RECT(innerDivWithTransform, frameView->layoutView());

    // Ensure that the inner div's transform is correctly rooted in the root frame's transform tree.
    // This asserts that we have the following tree structure:
    // ...
    //   Transform transform=translation=1.000000,2.000000,3.000000
    //     PreTranslation transform=translation=2.000000,2.000000,0.000000
    //       ScrollTranslation transform=translation=0.000000,0.000000,0.000000
    //         Transform transform=translation=4.000000,5.000000,6.000000
    auto* innerDocumentScrollTranslation = innerDivTransform->parent();
    EXPECT_EQ(TransformationMatrix().translate3d(0, 0, 0), innerDocumentScrollTranslation->matrix());
    auto* iframePreTranslation = innerDocumentScrollTranslation->parent();
    EXPECT_EQ(TransformationMatrix().translate3d(2, 2, 0), iframePreTranslation->matrix());
    EXPECT_EQ(divWithTransformProperties->transform(), iframePreTranslation->parent());
}

TEST_F(PaintPropertyTreeBuilderTest, TransformNodesInTransformedSubframes)
{
    setBodyInnerHTML(
        "<style>body { margin: 0; }</style>"
        "<div id='divWithTransform' style='transform: translate3d(1px, 2px, 3px);'>"
        "  <iframe id='frame' style='transform: translate3d(4px, 5px, 6px); border: 42px solid; margin: 7px;'></iframe>"
        "</div>");
    Document& frameDocument = setupChildIframe("frame",
        "<style>body { margin: 31px; }</style><div id='transform' style='transform: translate3d(7px, 8px, 9px); width: 100px; height: 200px'></div>");
    FrameView* frameView = document().view();
    frameView->updateAllLifecyclePhases();

    // Assert that we have the following tree structure:
    // ...
    //   Transform transform=translation=1.000000,2.000000,3.000000
    //     PaintOffsetTranslation transform=translation=7.000000,7.000000,0.000000
    //       Transform transform=translation=4.000000,5.000000,6.000000
    //         PreTranslation transform=translation=42.000000,42.000000,0.000000
    //           ScrollTranslation transform=translation=0.000000,0.000000,0.000000
    //             PaintOffsetTranslation transform=translation=31.000000,31.000000,0.000000
    //               Transform transform=translation=7.000000,8.000000,9.000000

    LayoutObject* innerDivWithTransform = frameDocument.getElementById("transform")->layoutObject();
    auto* innerDivTransform = innerDivWithTransform->objectPaintProperties()->transform();
    EXPECT_EQ(TransformationMatrix().translate3d(7, 8, 9), innerDivTransform->matrix());
    CHECK_EXACT_VISUAL_RECT(innerDivWithTransform, frameView->layoutView());

    auto* innerDocumentPaintOffsetTranslation = innerDivTransform->parent();
    EXPECT_EQ(TransformationMatrix().translate3d(31, 31, 0), innerDocumentPaintOffsetTranslation->matrix());
    auto* innerDocumentScrollTranslation = innerDocumentPaintOffsetTranslation->parent();
    EXPECT_EQ(TransformationMatrix().translate3d(0, 0, 0), innerDocumentScrollTranslation->matrix());
    auto* iframePreTranslation = innerDocumentScrollTranslation->parent();
    EXPECT_EQ(TransformationMatrix().translate3d(42, 42, 0), iframePreTranslation->matrix());
    auto* iframeTransform = iframePreTranslation->parent();
    EXPECT_EQ(TransformationMatrix().translate3d(4, 5, 6), iframeTransform->matrix());
    auto* iframePaintOffsetTranslation = iframeTransform->parent();
    EXPECT_EQ(TransformationMatrix().translate3d(7, 7, 0), iframePaintOffsetTranslation->matrix());
    auto* divWithTransformTransform = iframePaintOffsetTranslation->parent();
    EXPECT_EQ(TransformationMatrix().translate3d(1, 2, 3), divWithTransformTransform->matrix());

    LayoutObject* divWithTransform = document().getElementById("divWithTransform")->layoutObject();
    EXPECT_EQ(divWithTransformTransform, divWithTransform->objectPaintProperties()->transform());
    CHECK_EXACT_VISUAL_RECT(divWithTransform, frameView->layoutView());
}

TEST_F(PaintPropertyTreeBuilderTest, TreeContextClipByNonStackingContext)
{
    // This test verifies the tree builder correctly computes and records the property tree context
    // for a (pseudo) stacking context that is scrolled by a containing block that is not one of
    // the painting ancestors.
    setBodyInnerHTML(
        "<style>body { margin: 0; }</style>"
        "<div id='scroller' style='overflow:scroll; width:400px; height:300px;'>"
        "  <div id='child' style='position:relative; width:100px; height: 200px;'></div>"
        "  <div style='height:10000px;'></div>"
        "</div>"
    );
    FrameView* frameView = document().view();

    LayoutObject* scroller = document().getElementById("scroller")->layoutObject();
    const ObjectPaintProperties* scrollerProperties = scroller->objectPaintProperties();
    LayoutObject* child = document().getElementById("child")->layoutObject();
    const ObjectPaintProperties* childProperties = child->objectPaintProperties();

    EXPECT_EQ(scrollerProperties->overflowClip(), childProperties->localBorderBoxProperties()->propertyTreeState.clip);
    EXPECT_EQ(scrollerProperties->scrollTranslation(), childProperties->localBorderBoxProperties()->propertyTreeState.transform);
    EXPECT_NE(nullptr, childProperties->localBorderBoxProperties()->propertyTreeState.effect);
    CHECK_EXACT_VISUAL_RECT(scroller, frameView->layoutView());
    CHECK_EXACT_VISUAL_RECT(child, frameView->layoutView());
}

TEST_F(PaintPropertyTreeBuilderTest, TreeContextUnclipFromParentStackingContext)
{
    // This test verifies the tree builder correctly computes and records the property tree context
    // for a (pseudo) stacking context that has a scrolling painting ancestor that is not its
    // containing block (thus should not be scrolled by it).

    setBodyInnerHTML(
        "<style>body { margin: 0; }</style>"
        "<div id='scroller' style='overflow:scroll; opacity:0.5;'>"
        "  <div id='child' style='position:absolute; left:0; top:0; width: 100px; height: 200px'></div>"
        "  <div style='height:10000px;'></div>"
        "</div>"
    );

    FrameView* frameView = document().view();
    LayoutObject* scroller = document().getElementById("scroller")->layoutObject();
    const ObjectPaintProperties* scrollerProperties = scroller->objectPaintProperties();
    LayoutObject* child = document().getElementById("child")->layoutObject();
    const ObjectPaintProperties* childProperties = child->objectPaintProperties();

    EXPECT_EQ(frameView->contentClip(), childProperties->localBorderBoxProperties()->propertyTreeState.clip);
    EXPECT_EQ(frameView->scrollTranslation(), childProperties->localBorderBoxProperties()->propertyTreeState.transform);
    EXPECT_EQ(scrollerProperties->effect(), childProperties->localBorderBoxProperties()->propertyTreeState.effect);
    CHECK_EXACT_VISUAL_RECT(scroller, frameView->layoutView());
    CHECK_EXACT_VISUAL_RECT(child, frameView->layoutView());
}

TEST_F(PaintPropertyTreeBuilderTest, TableCellLayoutLocation)
{
    // This test verifies that the border box space of a table cell is being correctly computed.
    // Table cells have weird location adjustment in our layout/paint implementation.
    setBodyInnerHTML(
        "<style>"
        "  body {"
        "    margin: 0;"
        "  }"
        "  table {"
        "    border-spacing: 0;"
        "    margin: 20px;"
        "    padding: 40px;"
        "    border: 10px solid black;"
        "  }"
        "  td {"
        "    width: 100px;"
        "    height: 100px;"
        "    padding: 0;"
        "  }"
        "  #target {"
        "    position: relative;"
        "    width: 100px;"
        "    height: 100px;"
        "  }"
        "</style>"
        "<table>"
        "  <tr><td></td><td></td></tr>"
        "  <tr><td></td><td><div id='target'></div></td></tr>"
        "</table>"
    );

    FrameView* frameView = document().view();
    LayoutObject* target = document().getElementById("target")->layoutObject();
    const ObjectPaintProperties* targetProperties = target->objectPaintProperties();

    EXPECT_EQ(LayoutPoint(170, 170), targetProperties->localBorderBoxProperties()->paintOffset);
    EXPECT_EQ(frameView->scrollTranslation(), targetProperties->localBorderBoxProperties()->propertyTreeState.transform);
    CHECK_EXACT_VISUAL_RECT(target, frameView->layoutView());
}

TEST_F(PaintPropertyTreeBuilderTest, CSSClipFixedPositionDescendant)
{
    // This test verifies that clip tree hierarchy being generated correctly for the hard case
    // such that a fixed position element getting clipped by an absolute position CSS clip.
    setBodyInnerHTML(
        "<style>"
        "  #clip {"
        "    position: absolute;"
        "    left: 123px;"
        "    top: 456px;"
        "    clip: rect(10px, 80px, 70px, 40px);"
        "    width: 100px;"
        "    height: 100px;"
        "  }"
        "  #fixed {"
        "    position: fixed;"
        "    left: 654px;"
        "    top: 321px;"
        "    width: 10px;"
        "    heght: 20px"
        "  }"
        "</style>"
        "<div id='clip'><div id='fixed'></div></div>"
    );
    LayoutRect localClipRect(40, 10, 40, 60);
    LayoutRect absoluteClipRect = localClipRect;
    absoluteClipRect.move(123, 456);

    FrameView* frameView = document().view();

    LayoutObject* clip = document().getElementById("clip")->layoutObject();
    const ObjectPaintProperties* clipProperties = clip->objectPaintProperties();
    EXPECT_EQ(frameView->contentClip(), clipProperties->cssClip()->parent());
    EXPECT_EQ(frameView->scrollTranslation(), clipProperties->cssClip()->localTransformSpace());
    EXPECT_EQ(FloatRoundedRect(FloatRect(absoluteClipRect)), clipProperties->cssClip()->clipRect());
    // TODO(chrishtr): GeomeryMapper rports 40x60 size (i.e. including clip), whereas the old code reports 100x100 (unclipped).
    // Fix.
    // CHECK_EXACT_VISUAL_RECT(clip, frameView->layoutView());

    LayoutObject* fixed = document().getElementById("fixed")->layoutObject();
    const ObjectPaintProperties* fixedProperties = fixed->objectPaintProperties();
    EXPECT_EQ(clipProperties->cssClip(), fixedProperties->localBorderBoxProperties()->propertyTreeState.clip);
    EXPECT_EQ(frameView->preTranslation(), fixedProperties->localBorderBoxProperties()->propertyTreeState.transform->parent());
    EXPECT_EQ(TransformationMatrix().translate(654, 321), fixedProperties->localBorderBoxProperties()->propertyTreeState.transform->matrix());
    EXPECT_EQ(LayoutPoint(), fixedProperties->localBorderBoxProperties()->paintOffset);
    CHECK_EXACT_VISUAL_RECT(fixed, frameView->layoutView());
}

TEST_F(PaintPropertyTreeBuilderTest, CSSClipFixedPositionDescendantNonShared)
{
    // This test is similar to CSSClipFixedPositionDescendant above, except that
    // now we have a parent overflow clip that should be escaped by the fixed descendant.
    setBodyInnerHTML(
        "<style>"
        "  body {"
        "    margin: 0;"
        "  }"
        "  #overflow {"
        "    position: relative;"
        "    width: 50px;"
        "    height: 50px;"
        "    overflow: scroll;"
        "  }"
        "  #clip {"
        "    position: absolute;"
        "    left: 123px;"
        "    top: 456px;"
        "    clip: rect(10px, 80px, 70px, 40px);"
        "    width: 100px;"
        "    height: 100px;"
        "  }"
        "  #fixed {"
        "    position: fixed;"
        "    left: 654px;"
        "    top: 321px;"
        "  }"
        "</style>"
        "<div id='overflow'><div id='clip'><div id='fixed'></div></div></div>"
    );
    LayoutRect localClipRect(40, 10, 40, 60);
    LayoutRect absoluteClipRect = localClipRect;
    absoluteClipRect.move(123, 456);

    FrameView* frameView = document().view();

    LayoutObject* overflow = document().getElementById("overflow")->layoutObject();
    const ObjectPaintProperties* overflowProperties = overflow->objectPaintProperties();
    EXPECT_EQ(frameView->contentClip(), overflowProperties->overflowClip()->parent());
    EXPECT_EQ(frameView->scrollTranslation(), overflowProperties->scrollTranslation()->parent());
    CHECK_EXACT_VISUAL_RECT(overflow, frameView->layoutView());

    LayoutObject* clip = document().getElementById("clip")->layoutObject();
    const ObjectPaintProperties* clipProperties = clip->objectPaintProperties();
    EXPECT_EQ(overflowProperties->overflowClip(), clipProperties->cssClip()->parent());
    EXPECT_EQ(overflowProperties->scrollTranslation(), clipProperties->cssClip()->localTransformSpace());
    EXPECT_EQ(FloatRoundedRect(FloatRect(absoluteClipRect)), clipProperties->cssClip()->clipRect());
    EXPECT_EQ(frameView->contentClip(), clipProperties->cssClipFixedPosition()->parent());
    EXPECT_EQ(overflowProperties->scrollTranslation(), clipProperties->cssClipFixedPosition()->localTransformSpace());
    EXPECT_EQ(FloatRoundedRect(FloatRect(absoluteClipRect)), clipProperties->cssClipFixedPosition()->clipRect());
    CHECK_EXACT_VISUAL_RECT(clip, frameView->layoutView());

    LayoutObject* fixed = document().getElementById("fixed")->layoutObject();
    const ObjectPaintProperties* fixedProperties = fixed->objectPaintProperties();
    EXPECT_EQ(clipProperties->cssClipFixedPosition(), fixedProperties->localBorderBoxProperties()->propertyTreeState.clip);
    EXPECT_EQ(frameView->preTranslation(), fixedProperties->localBorderBoxProperties()->propertyTreeState.transform->parent());
    EXPECT_EQ(TransformationMatrix().translate(654, 321), fixedProperties->localBorderBoxProperties()->propertyTreeState.transform->matrix());
    EXPECT_EQ(LayoutPoint(), fixedProperties->localBorderBoxProperties()->paintOffset);
    CHECK_EXACT_VISUAL_RECT(fixed, frameView->layoutView());
}

TEST_F(PaintPropertyTreeBuilderTest, ColumnSpannerUnderRelativePositioned)
{
    setBodyInnerHTML(
        "<div style='columns: 3; position: absolute; top: 44px; left: 55px;'>"
        "  <div style='position: relative; top: 100px; left: 100px'>"
        "    <div id='spanner' style='column-span: all; opacity: 0.5; width: 100px; height: 100px;'></div>"
        "  </div>"
        "</div>"
    );

    LayoutObject* spanner = getLayoutObjectByElementId("spanner");
    EXPECT_EQ(LayoutPoint(55, 44), spanner->objectPaintProperties()->localBorderBoxProperties()->paintOffset);
    CHECK_EXACT_VISUAL_RECT(spanner, document().view()->layoutView());
}

TEST_F(PaintPropertyTreeBuilderTest, FractionalPaintOffset)
{
    setBodyInnerHTML(
        "<style>"
        "  * { margin: 0; }"
        "  div { position: absolute; }"
        "</style>"
        "<div id='a' style='width: 70px; height: 70px; left: 0.1px; top: 0.3px;'>"
        "  <div id='b' style='width: 40px; height: 40px; left: 0.5px; top: 11.1px;'></div>"
        "</div>"
    );
    FrameView* frameView = document().view();

    LayoutObject* a = document().getElementById("a")->layoutObject();
    const ObjectPaintProperties* aProperties = a->objectPaintProperties();
    LayoutPoint aPaintOffset = LayoutPoint(FloatPoint(0.1, 0.3));
    EXPECT_EQ(aPaintOffset, aProperties->localBorderBoxProperties()->paintOffset);
    CHECK_EXACT_VISUAL_RECT(a, frameView->layoutView());

    LayoutObject* b = document().getElementById("b")->layoutObject();
    const ObjectPaintProperties* bProperties = b->objectPaintProperties();
    LayoutPoint bPaintOffset = aPaintOffset + LayoutPoint(FloatPoint(0.5, 11.1));
    EXPECT_EQ(bPaintOffset, bProperties->localBorderBoxProperties()->paintOffset);
    CHECK_EXACT_VISUAL_RECT(a, frameView->layoutView());
}

TEST_F(PaintPropertyTreeBuilderTest, PaintOffsetWithBasicPixelSnapping)
{
    setBodyInnerHTML(
        "<style>"
        "  * { margin: 0; }"
        "  div { position: relative; }"
        "</style>"
        "<div id='a' style='width: 70px; height: 70px; left: 0.3px; top: 0.3px;'>"
        "  <div id='b' style='width: 40px; height: 40px; transform: translateZ(0);'>"
        "    <div id='c' style='width: 40px; height: 40px; left: 0.1px; top: 0.1px;'></div>"
        "  </div>"
        "</div>"
    );
    FrameView* frameView = document().view();

    LayoutObject* b = document().getElementById("b")->layoutObject();
    const ObjectPaintProperties* bProperties = b->objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate3d(0, 0, 0), bProperties->transform()->matrix());
    // The paint offset transform should be snapped from (0.3,0.3) to (0,0).
    EXPECT_EQ(TransformationMatrix().translate(0, 0), bProperties->transform()->parent()->matrix());
    // The residual subpixel adjustment should be (0.3,0.3) - (0,0) = (0.3,0.3).
    LayoutPoint subpixelAccumulation = LayoutPoint(FloatPoint(0.3, 0.3));
    EXPECT_EQ(subpixelAccumulation, bProperties->localBorderBoxProperties()->paintOffset);
    CHECK_EXACT_VISUAL_RECT(b, frameView->layoutView());

    // c should be painted starting at subpixelAccumulation + (0.1,0.1) = (0.4,0.4).
    LayoutObject* c = document().getElementById("c")->layoutObject();
    LayoutPoint cPaintOffset = subpixelAccumulation + LayoutPoint(FloatPoint(0.1, 0.1));
    const ObjectPaintProperties* cProperties = c->objectPaintProperties();
    EXPECT_EQ(cPaintOffset, cProperties->localBorderBoxProperties()->paintOffset);
    // Visual rects via the non-paint properties system use enclosingIntRect before applying transforms,
    // because they are computed bottom-up and therefore can't apply pixel snapping. Therefore apply a
    // slop of 1px.
    CHECK_VISUAL_RECT(c, frameView->layoutView(), 1);
}

TEST_F(PaintPropertyTreeBuilderTest, PaintOffsetWithPixelSnappingThroughTransform)
{
    setBodyInnerHTML(
        "<style>"
        "  * { margin: 0; }"
        "  div { position: relative; }"
        "</style>"
        "<div id='a' style='width: 70px; height: 70px; left: 0.7px; top: 0.7px;'>"
        "  <div id='b' style='width: 40px; height: 40px; transform: translateZ(0);'>"
        "    <div id='c' style='width: 40px; height: 40px; left: 0.7px; top: 0.7px;'></div>"
        "  </div>"
        "</div>"
    );
    FrameView* frameView = document().view();

    LayoutObject* b = document().getElementById("b")->layoutObject();
    const ObjectPaintProperties* bProperties = b->objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate3d(0, 0, 0), bProperties->transform()->matrix());
    // The paint offset transform should be snapped from (0.7,0.7) to (1,1).
    EXPECT_EQ(TransformationMatrix().translate(1, 1), bProperties->transform()->parent()->matrix());
    // The residual subpixel adjustment should be (0.7,0.7) - (1,1) = (-0.3,-0.3).
    LayoutPoint subpixelAccumulation = LayoutPoint(LayoutPoint(FloatPoint(0.7, 0.7)) - LayoutPoint(1, 1));
    EXPECT_EQ(subpixelAccumulation, bProperties->localBorderBoxProperties()->paintOffset);
    CHECK_EXACT_VISUAL_RECT(b, frameView->layoutView());

    // c should be painted starting at subpixelAccumulation + (0.7,0.7) = (0.4,0.4).
    LayoutObject* c = document().getElementById("c")->layoutObject();
    LayoutPoint cPaintOffset = subpixelAccumulation + LayoutPoint(FloatPoint(0.7, 0.7));
    const ObjectPaintProperties* cProperties = c->objectPaintProperties();
    EXPECT_EQ(cPaintOffset, cProperties->localBorderBoxProperties()->paintOffset);
    // Visual rects via the non-paint properties system use enclosingIntRect before applying transforms,
    // because they are computed bottom-up and therefore can't apply pixel snapping. Therefore apply a
    // slop of 1px.
    CHECK_VISUAL_RECT(c, frameView->layoutView(), 1);
}

TEST_F(PaintPropertyTreeBuilderTest, PaintOffsetWithPixelSnappingThroughMultipleTransforms)
{
    setBodyInnerHTML(
        "<style>"
        "  * { margin: 0; }"
        "  div { position: relative; }"
        "</style>"
        "<div id='a' style='width: 70px; height: 70px; left: 0.7px; top: 0.7px;'>"
        "  <div id='b' style='width: 40px; height: 40px; transform: translate3d(5px, 7px, 0);'>"
        "    <div id='c' style='width: 40px; height: 40px; transform: translate3d(11px, 13px, 0);'>"
        "      <div id='d' style='width: 40px; height: 40px; left: 0.7px; top: 0.7px;'></div>"
        "    </div>"
        "  </div>"
        "</div>"
    );
    FrameView* frameView = document().view();

    LayoutObject* b = document().getElementById("b")->layoutObject();
    const ObjectPaintProperties* bProperties = b->objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate3d(5, 7, 0), bProperties->transform()->matrix());
    // The paint offset transform should be snapped from (0.7,0.7) to (1,1).
    EXPECT_EQ(TransformationMatrix().translate(1, 1), bProperties->transform()->parent()->matrix());
    // The residual subpixel adjustment should be (0.7,0.7) - (1,1) = (-0.3,-0.3).
    LayoutPoint subpixelAccumulation = LayoutPoint(LayoutPoint(FloatPoint(0.7, 0.7)) - LayoutPoint(1, 1));
    EXPECT_EQ(subpixelAccumulation, bProperties->localBorderBoxProperties()->paintOffset);
    CHECK_EXACT_VISUAL_RECT(b, frameView->layoutView());

    LayoutObject* c = document().getElementById("c")->layoutObject();
    const ObjectPaintProperties* cProperties = c->objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate3d(11, 13, 0), cProperties->transform()->matrix());
    // The paint offset should be (-0.3,-0.3) but the paint offset transform should still be at
    // (0,0) because it should be snapped.
    EXPECT_EQ(TransformationMatrix().translate(0, 0), cProperties->transform()->parent()->matrix());
    // The residual subpixel adjustment should still be (-0.3,-0.3).
    EXPECT_EQ(subpixelAccumulation, cProperties->localBorderBoxProperties()->paintOffset);
    CHECK_EXACT_VISUAL_RECT(c, frameView->layoutView());

    // d should be painted starting at subpixelAccumulation + (0.7,0.7) = (0.4,0.4).
    LayoutObject* d = document().getElementById("d")->layoutObject();
    LayoutPoint dPaintOffset = subpixelAccumulation + LayoutPoint(FloatPoint(0.7, 0.7));
    const ObjectPaintProperties* dProperties = d->objectPaintProperties();
    EXPECT_EQ(dPaintOffset, dProperties->localBorderBoxProperties()->paintOffset);
    // Visual rects via the non-paint properties system use enclosingIntRect before applying transforms,
    // because they are computed bottom-up and therefore can't apply pixel snapping. Therefore apply a
    // slop of 1px.
    CHECK_VISUAL_RECT(d, frameView->layoutView(), 1);
}

TEST_F(PaintPropertyTreeBuilderTest, PaintOffsetWithPixelSnappingWithFixedPos)
{
    setBodyInnerHTML(
        "<style>"
        "  * { margin: 0; }"
        "</style>"
        "<div id='a' style='width: 70px; height: 70px; left: 0.7px; position: relative;'>"
        "  <div id='b' style='width: 40px; height: 40px; transform: translateZ(0); position: relative;'>"
        "    <div id='fixed' style='width: 40px; height: 40px; position: fixed;'>"
        "      <div id='d' style='width: 40px; height: 40px; left: 0.7px; position: relative;'></div>"
        "    </div>"
        "  </div>"
        "</div>"
    );
    FrameView* frameView = document().view();

    LayoutObject* b = document().getElementById("b")->layoutObject();
    const ObjectPaintProperties* bProperties = b->objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate(0, 0), bProperties->transform()->matrix());
    // The paint offset transform should be snapped from (0.7,0) to (1,0).
    EXPECT_EQ(TransformationMatrix().translate(1, 0), bProperties->transform()->parent()->matrix());
    // The residual subpixel adjustment should be (0.7,0) - (1,0) = (-0.3,0).
    LayoutPoint subpixelAccumulation = LayoutPoint(LayoutPoint(FloatPoint(0.7, 0)) - LayoutPoint(1, 0));
    EXPECT_EQ(subpixelAccumulation, bProperties->localBorderBoxProperties()->paintOffset);
    CHECK_EXACT_VISUAL_RECT(b, frameView->layoutView());

    LayoutObject* fixed = document().getElementById("fixed")->layoutObject();
    const ObjectPaintProperties* fixedProperties = fixed->objectPaintProperties();
    // The residual subpixel adjustment should still be (-0.3,0).
    EXPECT_EQ(subpixelAccumulation, fixedProperties->localBorderBoxProperties()->paintOffset);
    CHECK_EXACT_VISUAL_RECT(fixed, frameView->layoutView());

    // d should be painted starting at subpixelAccumulation + (0.7,0) = (0.4,0).
    LayoutObject* d = document().getElementById("d")->layoutObject();
    LayoutPoint dPaintOffset = subpixelAccumulation + LayoutPoint(FloatPoint(0.7, 0));
    const ObjectPaintProperties* dProperties = d->objectPaintProperties();
    EXPECT_EQ(dPaintOffset, dProperties->localBorderBoxProperties()->paintOffset);
    // Visual rects via the non-paint properties system use enclosingIntRect before applying transforms,
    // because they are computed bottom-up and therefore can't apply pixel snapping. Therefore apply a
    // slop of 1px.
    CHECK_VISUAL_RECT(d, frameView->layoutView(), 1);
}

TEST_F(PaintPropertyTreeBuilderTest, SvgPixelSnappingShouldResetPaintOffset)
{
    setBodyInnerHTML(
        "<svg id='svg' style='position: relative; left: 0.1px; transform: matrix(1, 0, 0, 1, 0, 0);'>"
        "    <rect id='rect' transform='translate(1, 1)'/>"
        "</svg>");

    LayoutObject& svgWithTransform = *document().getElementById("svg")->layoutObject();
    const ObjectPaintProperties* svgWithTransformProperties = svgWithTransform.objectPaintProperties();
    EXPECT_EQ(TransformationMatrix(), svgWithTransformProperties->transform()->matrix());
    EXPECT_EQ(LayoutPoint(FloatPoint(0.1, 0)), svgWithTransformProperties->localBorderBoxProperties()->paintOffset);
    EXPECT_EQ(nullptr, svgWithTransformProperties->svgLocalToBorderBoxTransform());

    LayoutObject& rectWithTransform = *document().getElementById("rect")->layoutObject();
    const ObjectPaintProperties* rectWithTransformProperties = rectWithTransform.objectPaintProperties();
    EXPECT_EQ(TransformationMatrix().translate(1, 1), rectWithTransformProperties->transform()->matrix());

    // Ensure there is no PaintOffset transform between the rect and the svg's transform.
    EXPECT_EQ(svgWithTransformProperties->transform(), rectWithTransformProperties->transform()->parent());
}

TEST_F(PaintPropertyTreeBuilderTest, NoRenderingContextByDefault)
{
    setBodyInnerHTML("<div style='transform: translateZ(0)'></div>");

    const ObjectPaintProperties* properties = document().body()->firstChild()->layoutObject()->objectPaintProperties();
    ASSERT_TRUE(properties->transform());
    EXPECT_FALSE(properties->transform()->hasRenderingContext());
}

TEST_F(PaintPropertyTreeBuilderTest, Preserve3DCreatesSharedRenderingContext)
{
    setBodyInnerHTML(
        "<div style='transform-style: preserve-3d'>"
        "  <div id='a' style='transform: translateZ(0); width: 30px; height: 40px'></div>"
        "  <div id='b' style='transform: translateZ(0); width: 20px; height: 10px'></div>"
        "</div>");
    FrameView* frameView = document().view();

    LayoutObject* a = document().getElementById("a")->layoutObject();
    const ObjectPaintProperties* aProperties = a->objectPaintProperties();
    LayoutObject* b = document().getElementById("b")->layoutObject();
    const ObjectPaintProperties* bProperties = b->objectPaintProperties();
    ASSERT_TRUE(aProperties->transform() && bProperties->transform());
    EXPECT_NE(aProperties->transform(), bProperties->transform());
    EXPECT_TRUE(aProperties->transform()->hasRenderingContext());
    EXPECT_TRUE(bProperties->transform()->hasRenderingContext());
    EXPECT_EQ(aProperties->transform()->renderingContextID(), bProperties->transform()->renderingContextID());
    CHECK_EXACT_VISUAL_RECT(a, frameView->layoutView());
    CHECK_EXACT_VISUAL_RECT(b, frameView->layoutView());
}

TEST_F(PaintPropertyTreeBuilderTest, FlatTransformStyleEndsRenderingContext)
{
    setBodyInnerHTML(
        "<div style='transform-style: preserve-3d'>"
        "  <div id='a' style='transform: translateZ(0); width: 30px; height: 40px'>"
        "    <div id='b' style='transform: translateZ(0); width: 10px; height: 20px'></div>"
        "  </div>"
        "</div>");
    FrameView* frameView = document().view();

    LayoutObject* a = document().getElementById("a")->layoutObject();
    const ObjectPaintProperties* aProperties = a->objectPaintProperties();
    LayoutObject* b = document().getElementById("b")->layoutObject();
    const ObjectPaintProperties* bProperties = b->objectPaintProperties();
    ASSERT_FALSE(a->styleRef().preserves3D());

    ASSERT_TRUE(aProperties->transform() && bProperties->transform());

    // #a should participate in a rendering context (due to its parent), but its
    // child #b should not.
    EXPECT_TRUE(aProperties->transform()->hasRenderingContext());
    EXPECT_FALSE(bProperties->transform()->hasRenderingContext());
    CHECK_EXACT_VISUAL_RECT(a, frameView->layoutView());
    CHECK_EXACT_VISUAL_RECT(b, frameView->layoutView());
}

TEST_F(PaintPropertyTreeBuilderTest, NestedRenderingContexts)
{
    setBodyInnerHTML(
        "<div style='transform-style: preserve-3d'>"
        "  <div id='a' style='transform: translateZ(0); width: 50px; height: 60px'>"
        "    <div style='transform-style: preserve-3d; width: 30px; height: 40px'>"
        "      <div id='b' style='transform: translateZ(0); width: 10px; height: 20px'>"
        "    </div>"
        "  </div>"
        "</div>");
    FrameView* frameView = document().view();

    LayoutObject* a = document().getElementById("a")->layoutObject();
    const ObjectPaintProperties* aProperties = a->objectPaintProperties();
    LayoutObject* b = document().getElementById("b")->layoutObject();
    const ObjectPaintProperties* bProperties = b->objectPaintProperties();
    ASSERT_FALSE(a->styleRef().preserves3D());
    ASSERT_TRUE(aProperties->transform() && bProperties->transform());

    // #a should participate in a rendering context (due to its parent). Its
    // child does preserve 3D, but since #a does not, #a's rendering context is
    // not passed on to its children. Thus #b ends up in a separate rendering
    // context rooted at its parent.
    EXPECT_TRUE(aProperties->transform()->hasRenderingContext());
    EXPECT_TRUE(bProperties->transform()->hasRenderingContext());
    EXPECT_NE(aProperties->transform()->renderingContextID(), bProperties->transform()->renderingContextID());
    CHECK_EXACT_VISUAL_RECT(a, frameView->layoutView());
    CHECK_EXACT_VISUAL_RECT(b, frameView->layoutView());
}

// Returns true if the first node has the second as an ancestor.
static bool nodeHasAncestor(const TransformPaintPropertyNode* node, const TransformPaintPropertyNode* ancestor)
{
    while (node) {
        if (node == ancestor)
            return true;
        node = node->parent();
    }
    return false;
}

// Returns true if some node will flatten the transform due to |node| before it
// is inherited by |node| (including if node->flattensInheritedTransform()).
static bool someNodeFlattensTransform(const TransformPaintPropertyNode* node, const TransformPaintPropertyNode* ancestor)
{
    while (node != ancestor) {
        if (node->flattensInheritedTransform())
            return true;
        node = node->parent();
    }
    return false;
}

TEST_F(PaintPropertyTreeBuilderTest, FlatTransformStylePropagatesToChildren)
{
    setBodyInnerHTML(
        "<div id='a' style='transform: translateZ(0); transform-style: flat; width: 30px; height: 40px'>"
        "  <div id='b' style='transform: translateZ(0); width: 10px; height: 10px'></div>"
        "</div>");
    FrameView* frameView = document().view();

    LayoutObject* a = document().getElementById("a")->layoutObject();
    LayoutObject* b = document().getElementById("b")->layoutObject();
    const auto* aTransform = a->objectPaintProperties()->transform();
    ASSERT_TRUE(aTransform);
    const auto* bTransform = b->objectPaintProperties()->transform();
    ASSERT_TRUE(bTransform);
    ASSERT_TRUE(nodeHasAncestor(bTransform, aTransform));

    // Some node must flatten the inherited transform from #a before it reaches
    // #b's transform.
    EXPECT_TRUE(someNodeFlattensTransform(bTransform, aTransform));
    CHECK_EXACT_VISUAL_RECT(a, frameView->layoutView());
    CHECK_EXACT_VISUAL_RECT(b, frameView->layoutView());
}

TEST_F(PaintPropertyTreeBuilderTest, Preserve3DTransformStylePropagatesToChildren)
{
    setBodyInnerHTML(
        "<div id='a' style='transform: translateZ(0); transform-style: preserve-3d; width: 30px; height: 40px'>"
        "  <div id='b' style='transform: translateZ(0); width: 10px; height: 10px'></div>"
        "</div>");
    FrameView* frameView = document().view();

    LayoutObject* a = document().getElementById("a")->layoutObject();
    LayoutObject* b = document().getElementById("b")->layoutObject();
    const auto* aTransform = a->objectPaintProperties()->transform();
    ASSERT_TRUE(aTransform);
    const auto* bTransform = b->objectPaintProperties()->transform();
    ASSERT_TRUE(bTransform);
    ASSERT_TRUE(nodeHasAncestor(bTransform, aTransform));

    // No node may flatten the inherited transform from #a before it reaches
    // #b's transform.
    EXPECT_FALSE(someNodeFlattensTransform(bTransform, aTransform));
    CHECK_EXACT_VISUAL_RECT(a, frameView->layoutView());
    CHECK_EXACT_VISUAL_RECT(b, frameView->layoutView());
}

TEST_F(PaintPropertyTreeBuilderTest, PerspectiveIsNotFlattened)
{
    // It's necessary to make nodes from the one that applies perspective to
    // ones that combine with it preserve 3D. Otherwise, the perspective doesn't
    // do anything.
    setBodyInnerHTML(
        "<div id='a' style='perspective: 800px; width: 30px; height: 40px'>"
        "  <div id='b' style='transform: translateZ(0); width: 10px; height: 20px'></div>"
        "</div>");
    FrameView* frameView = document().view();

    LayoutObject* a = document().getElementById("a")->layoutObject();
    LayoutObject* b = document().getElementById("b")->layoutObject();
    const ObjectPaintProperties* aProperties = a->objectPaintProperties();
    const ObjectPaintProperties* bProperties = b->objectPaintProperties();
    const TransformPaintPropertyNode* aPerspective = aProperties->perspective();
    ASSERT_TRUE(aPerspective);
    const TransformPaintPropertyNode* bTransform = bProperties->transform();
    ASSERT_TRUE(bTransform);
    ASSERT_TRUE(nodeHasAncestor(bTransform, aPerspective));
    EXPECT_FALSE(someNodeFlattensTransform(bTransform, aPerspective));
    CHECK_EXACT_VISUAL_RECT(a, frameView->layoutView());
    CHECK_EXACT_VISUAL_RECT(b, frameView->layoutView());
}

TEST_F(PaintPropertyTreeBuilderTest, PerspectiveDoesNotEstablishRenderingContext)
{
    // It's necessary to make nodes from the one that applies perspective to
    // ones that combine with it preserve 3D. Otherwise, the perspective doesn't
    // do anything.
    setBodyInnerHTML(
        "<div id='a' style='perspective: 800px; width: 30px; height: 40px'>"
        "  <div id='b' style='transform: translateZ(0); width: 10px; height: 20px'></div>"
        "</div>");
    FrameView* frameView = document().view();

    LayoutObject* a = document().getElementById("a")->layoutObject();
    LayoutObject* b = document().getElementById("b")->layoutObject();
    const ObjectPaintProperties* aProperties = a->objectPaintProperties();
    const ObjectPaintProperties* bProperties = b->objectPaintProperties();
    const TransformPaintPropertyNode* aPerspective = aProperties->perspective();
    ASSERT_TRUE(aPerspective);
    EXPECT_FALSE(aPerspective->hasRenderingContext());
    const TransformPaintPropertyNode* bTransform = bProperties->transform();
    ASSERT_TRUE(bTransform);
    EXPECT_FALSE(bTransform->hasRenderingContext());
    CHECK_EXACT_VISUAL_RECT(a, frameView->layoutView());
    CHECK_EXACT_VISUAL_RECT(b, frameView->layoutView());
}

TEST_F(PaintPropertyTreeBuilderTest, CachedProperties)
{
    setBodyInnerHTML(
        "<div id='a' style='transform: translate(33px, 44px); width: 50px; height: 60px'>"
        "  <div id='b' style='transform: translate(55px, 66px); width: 30px; height: 40px'>"
        "    <div id='c' style='transform: translate(77px, 88px); width: 10px; height: 20px'>C<div>"
        "  </div>"
        "</div>");
    FrameView* frameView = document().view();

    Element* a = document().getElementById("a");
    const ObjectPaintProperties* aProperties = a->layoutObject()->objectPaintProperties();
    const TransformPaintPropertyNode* aTransformNode = aProperties->transform();
    EXPECT_EQ(TransformationMatrix().translate(33, 44), aTransformNode->matrix());

    Element* b = document().getElementById("b");
    const ObjectPaintProperties* bProperties = b->layoutObject()->objectPaintProperties();
    const TransformPaintPropertyNode* bTransformNode = bProperties->transform();
    EXPECT_EQ(TransformationMatrix().translate(55, 66), bTransformNode->matrix());

    Element* c = document().getElementById("c");
    const ObjectPaintProperties* cProperties = c->layoutObject()->objectPaintProperties();
    const TransformPaintPropertyNode* cTransformNode = cProperties->transform();
    EXPECT_EQ(TransformationMatrix().translate(77, 88), cTransformNode->matrix());

    CHECK_EXACT_VISUAL_RECT(a->layoutObject(), frameView->layoutView());
    CHECK_EXACT_VISUAL_RECT(b->layoutObject(), frameView->layoutView());
    CHECK_EXACT_VISUAL_RECT(c->layoutObject(), frameView->layoutView());

    // Change transform of b. B's transform node should be a new node with the new value,
    // and a and c's transform nodes should be unchanged (with c's parent adjusted).
    b->setAttribute(HTMLNames::styleAttr, "transform: translate(111px, 222px)");
    document().view()->updateAllLifecyclePhases();

    EXPECT_EQ(aProperties, a->layoutObject()->objectPaintProperties());
    EXPECT_EQ(aTransformNode, aProperties->transform());

    EXPECT_EQ(bProperties, b->layoutObject()->objectPaintProperties());
    bTransformNode = bProperties->transform();
    EXPECT_EQ(TransformationMatrix().translate(111, 222), bTransformNode->matrix());
    EXPECT_EQ(aTransformNode, bTransformNode->parent());

    EXPECT_EQ(cProperties, c->layoutObject()->objectPaintProperties());
    EXPECT_EQ(cTransformNode, cProperties->transform());
    EXPECT_EQ(bTransformNode, cTransformNode->parent());

    CHECK_EXACT_VISUAL_RECT(a->layoutObject(), frameView->layoutView());
    CHECK_EXACT_VISUAL_RECT(b->layoutObject(), frameView->layoutView());
    CHECK_EXACT_VISUAL_RECT(c->layoutObject(), frameView->layoutView());

    // Remove transform from b. B's transform node should be removed from the tree,
    // and a and c's transform nodes should be unchanged (with c's parent adjusted).
    b->setAttribute(HTMLNames::styleAttr, "");
    document().view()->updateAllLifecyclePhases();

    EXPECT_EQ(aProperties, a->layoutObject()->objectPaintProperties());
    EXPECT_EQ(aTransformNode, aProperties->transform());

    EXPECT_EQ(bProperties, b->layoutObject()->objectPaintProperties());
    EXPECT_EQ(nullptr, bProperties->transform());

    EXPECT_EQ(cProperties, c->layoutObject()->objectPaintProperties());
    EXPECT_EQ(cTransformNode, cProperties->transform());
    EXPECT_EQ(aTransformNode, cTransformNode->parent());

    CHECK_EXACT_VISUAL_RECT(a->layoutObject(), frameView->layoutView());
    CHECK_EXACT_VISUAL_RECT(b->layoutObject(), frameView->layoutView());
    CHECK_EXACT_VISUAL_RECT(c->layoutObject(), frameView->layoutView());

    // Re-add transform to b. B's transform node should be inserted into the tree,
    // and a and c's transform nodes should be unchanged (with c's parent adjusted).
    b->setAttribute(HTMLNames::styleAttr, "transform: translate(4px, 5px)");
    document().view()->updateAllLifecyclePhases();

    EXPECT_EQ(aProperties, a->layoutObject()->objectPaintProperties());
    EXPECT_EQ(aTransformNode, aProperties->transform());

    EXPECT_EQ(bProperties, b->layoutObject()->objectPaintProperties());
    bTransformNode = bProperties->transform();
    EXPECT_EQ(TransformationMatrix().translate(4, 5), bTransformNode->matrix());
    EXPECT_EQ(aTransformNode, bTransformNode->parent());

    EXPECT_EQ(cProperties, c->layoutObject()->objectPaintProperties());
    EXPECT_EQ(cTransformNode, cProperties->transform());
    EXPECT_EQ(bTransformNode, cTransformNode->parent());

    CHECK_EXACT_VISUAL_RECT(a->layoutObject(), frameView->layoutView());
    CHECK_EXACT_VISUAL_RECT(b->layoutObject(), frameView->layoutView());
    CHECK_EXACT_VISUAL_RECT(c->layoutObject(), frameView->layoutView());
}

} // namespace blink
