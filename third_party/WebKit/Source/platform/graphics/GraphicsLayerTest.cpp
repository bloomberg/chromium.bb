/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/GraphicsLayer.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/animation/CompositorAnimationPlayer.h"
#include "platform/animation/CompositorAnimationPlayerClient.h"
#include "platform/animation/CompositorAnimationTimeline.h"
#include "platform/animation/CompositorFloatAnimationCurve.h"
#include "platform/graphics/CompositorFactory.h"
#include "platform/scroll/ScrollableArea.h"
#include "platform/transforms/Matrix3DTransformOperation.h"
#include "platform/transforms/RotateTransformOperation.h"
#include "platform/transforms/TranslateTransformOperation.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebGraphicsContext3D.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebUnitTestSupport.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

namespace {

class MockGraphicsLayerClient : public GraphicsLayerClient {
public:
    IntRect computeInterestRect(const GraphicsLayer*, const IntRect&) const { return IntRect(); }
    void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect&) const override { }
    String debugName(const GraphicsLayer*) const override { return String(); }
};

class GraphicsLayerForTesting : public GraphicsLayer {
public:
    explicit GraphicsLayerForTesting(GraphicsLayerClient* client)
        : GraphicsLayer(client) { }
};

} // anonymous namespace

class GraphicsLayerTest : public testing::Test {
public:
    GraphicsLayerTest()
    {
        m_clipLayer = adoptPtr(new GraphicsLayerForTesting(&m_client));
        m_scrollElasticityLayer = adoptPtr(new GraphicsLayerForTesting(&m_client));
        m_graphicsLayer = adoptPtr(new GraphicsLayerForTesting(&m_client));
        m_clipLayer->addChild(m_scrollElasticityLayer.get());
        m_scrollElasticityLayer->addChild(m_graphicsLayer.get());
        m_graphicsLayer->platformLayer()->setScrollClipLayer(
            m_clipLayer->platformLayer());
        m_platformLayer = m_graphicsLayer->platformLayer();
        m_layerTreeView = adoptPtr(Platform::current()->unitTestSupport()->createLayerTreeViewForTesting());
        ASSERT(m_layerTreeView);
        m_layerTreeView->setRootLayer(*m_clipLayer->platformLayer());
        m_layerTreeView->registerViewportLayers(
            m_scrollElasticityLayer->platformLayer(), m_clipLayer->platformLayer(), m_graphicsLayer->platformLayer(), 0);
        m_layerTreeView->setViewportSize(WebSize(1, 1));
    }

    ~GraphicsLayerTest() override
    {
        m_graphicsLayer.clear();
        m_layerTreeView.clear();
    }

    WebLayerTreeView* layerTreeView() { return m_layerTreeView.get(); }

protected:
    WebLayer* m_platformLayer;
    OwnPtr<GraphicsLayerForTesting> m_graphicsLayer;
    OwnPtr<GraphicsLayerForTesting> m_scrollElasticityLayer;
    OwnPtr<GraphicsLayerForTesting> m_clipLayer;

private:
    OwnPtr<WebLayerTreeView> m_layerTreeView;
    MockGraphicsLayerClient m_client;
};

class AnimationPlayerForTesting : public CompositorAnimationPlayerClient {
public:
    AnimationPlayerForTesting()
    {
        m_compositorPlayer = adoptPtr(CompositorFactory::current().createAnimationPlayer());
    }

    CompositorAnimationPlayer* compositorPlayer() const override
    {
        return m_compositorPlayer.get();
    }

    OwnPtr<CompositorAnimationPlayer> m_compositorPlayer;
};

TEST_F(GraphicsLayerTest, updateLayerShouldFlattenTransformWithAnimations)
{
    ASSERT_FALSE(m_platformLayer->hasActiveAnimation());

    OwnPtr<CompositorFloatAnimationCurve> curve = adoptPtr(CompositorFactory::current().createFloatAnimationCurve());
    curve->add(CompositorFloatKeyframe(0.0, 0.0));
    OwnPtr<CompositorAnimation> floatAnimation(adoptPtr(CompositorFactory::current().createAnimation(*curve, CompositorAnimation::TargetPropertyOpacity)));
    int animationId = floatAnimation->id();

    if (RuntimeEnabledFeatures::compositorAnimationTimelinesEnabled()) {
        OwnPtr<CompositorAnimationTimeline> compositorTimeline = adoptPtr(CompositorFactory::current().createAnimationTimeline());
        AnimationPlayerForTesting player;

        layerTreeView()->attachCompositorAnimationTimeline(compositorTimeline->animationTimeline());
        compositorTimeline->playerAttached(player);

        player.compositorPlayer()->attachLayer(m_platformLayer);
        ASSERT_TRUE(player.compositorPlayer()->isLayerAttached());

        player.compositorPlayer()->addAnimation(floatAnimation.leakPtr());

        ASSERT_TRUE(m_platformLayer->hasActiveAnimation());

        m_graphicsLayer->setShouldFlattenTransform(false);

        m_platformLayer = m_graphicsLayer->platformLayer();
        ASSERT_TRUE(m_platformLayer);

        ASSERT_TRUE(m_platformLayer->hasActiveAnimation());
        player.compositorPlayer()->removeAnimation(animationId);
        ASSERT_FALSE(m_platformLayer->hasActiveAnimation());

        m_graphicsLayer->setShouldFlattenTransform(true);

        m_platformLayer = m_graphicsLayer->platformLayer();
        ASSERT_TRUE(m_platformLayer);

        ASSERT_FALSE(m_platformLayer->hasActiveAnimation());

        player.compositorPlayer()->detachLayer();
        ASSERT_FALSE(player.compositorPlayer()->isLayerAttached());

        compositorTimeline->playerDestroyed(player);
        layerTreeView()->detachCompositorAnimationTimeline(compositorTimeline->animationTimeline());
    } else {
        ASSERT_TRUE(m_platformLayer->addAnimation(floatAnimation->releaseCCAnimation()));

        ASSERT_TRUE(m_platformLayer->hasActiveAnimation());

        m_graphicsLayer->setShouldFlattenTransform(false);

        m_platformLayer = m_graphicsLayer->platformLayer();
        ASSERT_TRUE(m_platformLayer);

        ASSERT_TRUE(m_platformLayer->hasActiveAnimation());
        m_platformLayer->removeAnimation(animationId);
        ASSERT_FALSE(m_platformLayer->hasActiveAnimation());

        m_graphicsLayer->setShouldFlattenTransform(true);

        m_platformLayer = m_graphicsLayer->platformLayer();
        ASSERT_TRUE(m_platformLayer);

        ASSERT_FALSE(m_platformLayer->hasActiveAnimation());
    }
}

class FakeScrollableArea : public NoBaseWillBeGarbageCollectedFinalized<FakeScrollableArea>, public ScrollableArea {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(FakeScrollableArea);
public:
    static PassOwnPtrWillBeRawPtr<FakeScrollableArea> create()
    {
        return adoptPtrWillBeNoop(new FakeScrollableArea);
    }

    bool isActive() const override { return false; }
    int scrollSize(ScrollbarOrientation) const override { return 100; }
    bool isScrollCornerVisible() const override { return false; }
    IntRect scrollCornerRect() const override { return IntRect(); }
    int visibleWidth() const override { return 10; }
    int visibleHeight() const override { return 10; }
    IntSize contentsSize() const override { return IntSize(100, 100); }
    bool scrollbarsCanBeActive() const override { return false; }
    IntRect scrollableAreaBoundingBox() const override { return IntRect(); }
    void scrollControlWasSetNeedsPaintInvalidation() override { }
    bool userInputScrollable(ScrollbarOrientation) const override { return true; }
    bool shouldPlaceVerticalScrollbarOnLeft() const override { return false; }
    int pageStep(ScrollbarOrientation) const override { return 0; }
    IntPoint minimumScrollPosition() const override { return IntPoint(); }
    IntPoint maximumScrollPosition() const override
    {
        return IntPoint(contentsSize().width() - visibleWidth(), contentsSize().height() - visibleHeight());
    }

    void setScrollOffset(const IntPoint& scrollOffset, ScrollType) override { m_scrollPosition = scrollOffset; }
    void setScrollOffset(const DoublePoint& scrollOffset, ScrollType) override { m_scrollPosition = scrollOffset; }
    DoublePoint scrollPositionDouble() const override { return m_scrollPosition; }
    IntPoint scrollPosition() const override { return flooredIntPoint(m_scrollPosition); }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        ScrollableArea::trace(visitor);
    }

private:
    DoublePoint m_scrollPosition;
};

TEST_F(GraphicsLayerTest, applyScrollToScrollableArea)
{
    OwnPtrWillBeRawPtr<FakeScrollableArea> scrollableArea = FakeScrollableArea::create();
    m_graphicsLayer->setScrollableArea(scrollableArea.get(), false);

    WebDoublePoint scrollPosition(7, 9);
    m_platformLayer->setScrollPositionDouble(scrollPosition);
    m_graphicsLayer->didScroll();

    EXPECT_FLOAT_EQ(scrollPosition.x, scrollableArea->scrollPositionDouble().x());
    EXPECT_FLOAT_EQ(scrollPosition.y, scrollableArea->scrollPositionDouble().y());
}

} // namespace blink
