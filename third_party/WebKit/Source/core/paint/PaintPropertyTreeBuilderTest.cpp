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
#include "wtf/HashMap.h"
#include "wtf/Vector.h"
#include <gtest/gtest.h>

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
        "  </div"
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
        "  </div"
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
        "  </div"
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

} // namespace blink
