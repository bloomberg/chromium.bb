// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/compositing/PaintArtifactCompositor.h"

#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/layers/layer.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/transform_node.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/PaintArtifact.h"
#include "platform/testing/PictureMatchers.h"
#include "platform/testing/TestPaintArtifact.h"
#include "platform/testing/WebLayerTreeViewImplForTesting.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {
namespace {

using ::testing::Pointee;

gfx::Transform translation(SkMScalar x, SkMScalar y)
{
    gfx::Transform transform;
    transform.Translate(x, y);
    return transform;
}

EffectPaintPropertyNode* dummyRootEffect()
{
    DEFINE_STATIC_REF(EffectPaintPropertyNode, node, EffectPaintPropertyNode::create(nullptr, 1.0));
    return node;
}

class PaintArtifactCompositorTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(true);

        // Delay constructing the compositor until after the feature is set.
        m_paintArtifactCompositor = wrapUnique(new PaintArtifactCompositor);
        m_paintArtifactCompositor->enableExtraDataForTesting();
    }

    void TearDown() override
    {
        m_featuresBackup.restore();
    }

    PaintArtifactCompositor& getPaintArtifactCompositor() { return *m_paintArtifactCompositor; }
    cc::Layer* rootLayer() { return m_paintArtifactCompositor->rootLayer(); }
    void update(const PaintArtifact& artifact) { m_paintArtifactCompositor->update(artifact); }

    size_t contentLayerCount()
    {
        return m_paintArtifactCompositor->getExtraDataForTesting()->contentLayers.size();
    }

    cc::Layer* contentLayerAt(unsigned index)
    {
        return m_paintArtifactCompositor->getExtraDataForTesting()->contentLayers[index].get();
    }

private:
    RuntimeEnabledFeatures::Backup m_featuresBackup;
    std::unique_ptr<PaintArtifactCompositor> m_paintArtifactCompositor;
};

TEST_F(PaintArtifactCompositorTest, EmptyPaintArtifact)
{
    PaintArtifact emptyArtifact;
    update(emptyArtifact);
    EXPECT_TRUE(rootLayer()->children().empty());
}

TEST_F(PaintArtifactCompositorTest, OneChunkWithAnOffset)
{
    TestPaintArtifact artifact;
    artifact.chunk(PaintChunkProperties())
        .rectDrawing(FloatRect(50, -50, 100, 100), Color::white);
    update(artifact.build());

    ASSERT_EQ(1u, rootLayer()->children().size());
    const cc::Layer* child = rootLayer()->child_at(0);
    EXPECT_THAT(child->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::white)));
    EXPECT_EQ(translation(50, -50), child->transform());
    EXPECT_EQ(gfx::Size(100, 100), child->bounds());
}

TEST_F(PaintArtifactCompositorTest, OneTransform)
{
    // A 90 degree clockwise rotation about (100, 100).
    RefPtr<TransformPaintPropertyNode> transform = TransformPaintPropertyNode::create(
        nullptr, TransformationMatrix().rotate(90), FloatPoint3D(100, 100, 0));

    TestPaintArtifact artifact;
    artifact.chunk(transform, nullptr, dummyRootEffect())
        .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
    artifact.chunk(nullptr, nullptr, dummyRootEffect())
        .rectDrawing(FloatRect(0, 0, 100, 100), Color::gray);
    artifact.chunk(transform, nullptr, dummyRootEffect())
        .rectDrawing(FloatRect(100, 100, 200, 100), Color::black);
    update(artifact.build());

    ASSERT_EQ(3u, rootLayer()->children().size());
    {
        const cc::Layer* layer = rootLayer()->child_at(0);
        EXPECT_THAT(layer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::white)));
        gfx::RectF mappedRect(0, 0, 100, 100);
        layer->transform().TransformRect(&mappedRect);
        EXPECT_EQ(gfx::RectF(100, 0, 100, 100), mappedRect);
    }
    {
        const cc::Layer* layer = rootLayer()->child_at(1);
        EXPECT_THAT(layer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::gray)));
        EXPECT_EQ(gfx::Transform(), layer->transform());
    }
    {
        const cc::Layer* layer = rootLayer()->child_at(2);
        EXPECT_THAT(layer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 200, 100), Color::black)));
        gfx::RectF mappedRect(0, 0, 200, 100);
        layer->transform().TransformRect(&mappedRect);
        EXPECT_EQ(gfx::RectF(0, 100, 100, 200), mappedRect);
    }
}

TEST_F(PaintArtifactCompositorTest, TransformCombining)
{
    // A translation by (5, 5) within a 2x scale about (10, 10).
    RefPtr<TransformPaintPropertyNode> transform1 = TransformPaintPropertyNode::create(
        nullptr, TransformationMatrix().scale(2), FloatPoint3D(10, 10, 0));
    RefPtr<TransformPaintPropertyNode> transform2 = TransformPaintPropertyNode::create(
        transform1, TransformationMatrix().translate(5, 5), FloatPoint3D());

    TestPaintArtifact artifact;
    artifact.chunk(transform1, nullptr, dummyRootEffect())
        .rectDrawing(FloatRect(0, 0, 300, 200), Color::white);
    artifact.chunk(transform2, nullptr, dummyRootEffect())
        .rectDrawing(FloatRect(0, 0, 300, 200), Color::black);
    update(artifact.build());

    ASSERT_EQ(2u, rootLayer()->children().size());
    {
        const cc::Layer* layer = rootLayer()->child_at(0);
        EXPECT_THAT(layer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::white)));
        gfx::RectF mappedRect(0, 0, 300, 200);
        layer->transform().TransformRect(&mappedRect);
        EXPECT_EQ(gfx::RectF(-10, -10, 600, 400), mappedRect);
    }
    {
        const cc::Layer* layer = rootLayer()->child_at(1);
        EXPECT_THAT(layer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::black)));
        gfx::RectF mappedRect(0, 0, 300, 200);
        layer->transform().TransformRect(&mappedRect);
        EXPECT_EQ(gfx::RectF(0, 0, 600, 400), mappedRect);
    }
}

TEST_F(PaintArtifactCompositorTest, LayerOriginCancellation)
{
    RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
        nullptr, nullptr, FloatRoundedRect(100, 100, 100, 100));
    RefPtr<TransformPaintPropertyNode> transform = TransformPaintPropertyNode::create(
        nullptr, TransformationMatrix().scale(2), FloatPoint3D());

    TestPaintArtifact artifact;
    artifact.chunk(transform, clip, nullptr)
        .rectDrawing(FloatRect(12, 34, 56, 78), Color::white);
    update(artifact.build());

    ASSERT_EQ(1u, rootLayer()->children().size());
    cc::Layer* clipLayer = rootLayer()->child_at(0);
    EXPECT_EQ(gfx::Size(100, 100), clipLayer->bounds());
    EXPECT_EQ(translation(100, 100), clipLayer->transform());
    EXPECT_TRUE(clipLayer->masks_to_bounds());

    ASSERT_EQ(1u, clipLayer->children().size());
    cc::Layer* layer = clipLayer->child_at(0);
    EXPECT_EQ(gfx::Size(56, 78), layer->bounds());
    gfx::Transform expectedTransform;
    expectedTransform.Translate(-100, -100);
    expectedTransform.Scale(2, 2);
    expectedTransform.Translate(12, 34);
    EXPECT_EQ(expectedTransform, layer->transform());
}

TEST_F(PaintArtifactCompositorTest, OneClip)
{
    RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
        nullptr, nullptr, FloatRoundedRect(100, 100, 300, 200));

    TestPaintArtifact artifact;
    artifact.chunk(nullptr, clip, nullptr)
        .rectDrawing(FloatRect(220, 80, 300, 200), Color::black);
    update(artifact.build());

    ASSERT_EQ(1u, rootLayer()->children().size());
    cc::Layer* clipLayer = rootLayer()->child_at(0);
    EXPECT_TRUE(clipLayer->masks_to_bounds());
    EXPECT_EQ(gfx::Size(300, 200), clipLayer->bounds());
    EXPECT_EQ(translation(100, 100), clipLayer->transform());

    ASSERT_EQ(1u, clipLayer->children().size());
    const cc::Layer* layer = clipLayer->child_at(0);
    EXPECT_THAT(layer->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::black)));
    EXPECT_EQ(translation(120, -20), layer->transform());
}

TEST_F(PaintArtifactCompositorTest, NestedClips)
{
    RefPtr<ClipPaintPropertyNode> clip1 = ClipPaintPropertyNode::create(
        nullptr, nullptr, FloatRoundedRect(100, 100, 700, 700));
    RefPtr<ClipPaintPropertyNode> clip2 = ClipPaintPropertyNode::create(
        clip1, nullptr, FloatRoundedRect(200, 200, 700, 100));

    TestPaintArtifact artifact;
    artifact.chunk(nullptr, clip1, dummyRootEffect())
        .rectDrawing(FloatRect(300, 350, 100, 100), Color::white);
    artifact.chunk(nullptr, clip2, dummyRootEffect())
        .rectDrawing(FloatRect(300, 350, 100, 100), Color::lightGray);
    artifact.chunk(nullptr, clip1, dummyRootEffect())
        .rectDrawing(FloatRect(300, 350, 100, 100), Color::darkGray);
    artifact.chunk(nullptr, clip2, dummyRootEffect())
        .rectDrawing(FloatRect(300, 350, 100, 100), Color::black);
    update(artifact.build());

    ASSERT_EQ(1u, rootLayer()->children().size());
    cc::Layer* clipLayer1 = rootLayer()->child_at(0);
    EXPECT_TRUE(clipLayer1->masks_to_bounds());
    EXPECT_EQ(gfx::Size(700, 700), clipLayer1->bounds());
    EXPECT_EQ(translation(100, 100), clipLayer1->transform());

    ASSERT_EQ(4u, clipLayer1->children().size());
    {
        const cc::Layer* whiteLayer = clipLayer1->child_at(0);
        EXPECT_THAT(whiteLayer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::white)));
        EXPECT_EQ(translation(200, 250), whiteLayer->transform());
    }
    {
        cc::Layer* lightGrayClip = clipLayer1->child_at(1);
        EXPECT_TRUE(lightGrayClip->masks_to_bounds());
        EXPECT_EQ(gfx::Size(700, 100), lightGrayClip->bounds());
        EXPECT_EQ(translation(100, 100), lightGrayClip->transform());
        ASSERT_EQ(1u, lightGrayClip->children().size());
        const cc::Layer* lightGrayLayer = lightGrayClip->child_at(0);
        EXPECT_THAT(lightGrayLayer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::lightGray)));
        EXPECT_EQ(translation(100, 150), lightGrayLayer->transform());
    }
    {
        const cc::Layer* darkGrayLayer = clipLayer1->child_at(2);
        EXPECT_THAT(darkGrayLayer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::darkGray)));
        EXPECT_EQ(translation(200, 250), darkGrayLayer->transform());
    }
    {
        cc::Layer* blackClip = clipLayer1->child_at(3);
        EXPECT_TRUE(blackClip->masks_to_bounds());
        EXPECT_EQ(gfx::Size(700, 100), blackClip->bounds());
        EXPECT_EQ(translation(100, 100), blackClip->transform());
        ASSERT_EQ(1u, blackClip->children().size());
        const cc::Layer* blackLayer = blackClip->child_at(0);
        EXPECT_THAT(blackLayer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::black)));
        EXPECT_EQ(translation(100, 150), blackLayer->transform());
    }
}

TEST_F(PaintArtifactCompositorTest, DeeplyNestedClips)
{
    Vector<RefPtr<ClipPaintPropertyNode>> clips;
    for (unsigned i = 1; i <= 10; i++) {
        clips.append(ClipPaintPropertyNode::create(
            clips.isEmpty() ? nullptr : clips.last(),
            nullptr, FloatRoundedRect(5 * i, 0, 100, 200 - 10 * i)));
    }

    TestPaintArtifact artifact;
    artifact.chunk(nullptr, clips.last(), nullptr)
        .rectDrawing(FloatRect(0, 0, 200, 200), Color::white);
    update(artifact.build());

    // Check the clip layers.
    cc::Layer* layer = rootLayer();
    for (const auto& clipNode : clips) {
        ASSERT_EQ(1u, layer->children().size());
        layer = layer->child_at(0);
        EXPECT_EQ(clipNode->clipRect().rect().width(), layer->bounds().width());
        EXPECT_EQ(clipNode->clipRect().rect().height(), layer->bounds().height());
        EXPECT_EQ(translation(5, 0), layer->transform());
    }

    // Check the drawing layer.
    ASSERT_EQ(1u, layer->children().size());
    cc::Layer* drawingLayer = layer->child_at(0);
    EXPECT_THAT(drawingLayer->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 200, 200), Color::white)));
    EXPECT_EQ(translation(-50, 0), drawingLayer->transform());
}

TEST_F(PaintArtifactCompositorTest, SiblingClips)
{
    RefPtr<ClipPaintPropertyNode> commonClip = ClipPaintPropertyNode::create(
        nullptr, nullptr, FloatRoundedRect(0, 0, 800, 600));
    RefPtr<ClipPaintPropertyNode> clip1 = ClipPaintPropertyNode::create(
        commonClip, nullptr, FloatRoundedRect(0, 0, 400, 600));
    RefPtr<ClipPaintPropertyNode> clip2 = ClipPaintPropertyNode::create(
        commonClip, nullptr, FloatRoundedRect(400, 0, 400, 600));

    TestPaintArtifact artifact;
    artifact.chunk(nullptr, clip1, nullptr)
        .rectDrawing(FloatRect(0, 0, 640, 480), Color::white);
    artifact.chunk(nullptr, clip2, nullptr)
        .rectDrawing(FloatRect(0, 0, 640, 480), Color::black);
    update(artifact.build());

    ASSERT_EQ(1u, rootLayer()->children().size());
    cc::Layer* commonClipLayer = rootLayer()->child_at(0);
    EXPECT_TRUE(commonClipLayer->masks_to_bounds());
    EXPECT_EQ(gfx::Size(800, 600), commonClipLayer->bounds());
    EXPECT_EQ(gfx::Transform(), commonClipLayer->transform());
    ASSERT_EQ(2u, commonClipLayer->children().size());
    {
        cc::Layer* clipLayer1 = commonClipLayer->child_at(0);
        EXPECT_TRUE(clipLayer1->masks_to_bounds());
        EXPECT_EQ(gfx::Size(400, 600), clipLayer1->bounds());
        EXPECT_EQ(gfx::Transform(), clipLayer1->transform());
        ASSERT_EQ(1u, clipLayer1->children().size());
        cc::Layer* whiteLayer = clipLayer1->child_at(0);
        EXPECT_THAT(whiteLayer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 640, 480), Color::white)));
        EXPECT_EQ(gfx::Transform(), whiteLayer->transform());
    }
    {
        cc::Layer* clipLayer2 = commonClipLayer->child_at(1);
        EXPECT_TRUE(clipLayer2->masks_to_bounds());
        EXPECT_EQ(gfx::Size(400, 600), clipLayer2->bounds());
        EXPECT_EQ(translation(400, 0), clipLayer2->transform());
        ASSERT_EQ(1u, clipLayer2->children().size());
        cc::Layer* blackLayer = clipLayer2->child_at(0);
        EXPECT_THAT(blackLayer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 640, 480), Color::black)));
        EXPECT_EQ(translation(-400, 0), blackLayer->transform());
    }
}

TEST_F(PaintArtifactCompositorTest, ForeignLayerPassesThrough)
{
    scoped_refptr<cc::Layer> layer = cc::Layer::Create();

    TestPaintArtifact artifact;
    artifact.chunk(PaintChunkProperties())
        .foreignLayer(FloatPoint(50, 100), IntSize(400, 300), layer);
    update(artifact.build());

    ASSERT_EQ(1u, rootLayer()->children().size());
    EXPECT_EQ(layer, rootLayer()->child_at(0));
    EXPECT_EQ(gfx::Size(400, 300), layer->bounds());
    EXPECT_EQ(translation(50, 100), layer->transform());
}

// Similar to the above, but for the path where we build cc property trees
// directly. This will eventually supersede the above.

class WebLayerTreeViewWithOutputSurface : public WebLayerTreeViewImplForTesting {
public:
    WebLayerTreeViewWithOutputSurface(const cc::LayerTreeSettings& settings)
        : WebLayerTreeViewImplForTesting(settings) {}

    // cc::LayerTreeHostClient
    void RequestNewOutputSurface() override
    {
        layerTreeHost()->SetOutputSurface(cc::FakeOutputSurface::CreateDelegating3d());
    }
};

class PaintArtifactCompositorTestWithPropertyTrees : public PaintArtifactCompositorTest {
protected:
    PaintArtifactCompositorTestWithPropertyTrees()
        : m_taskRunner(new base::TestSimpleTaskRunner)
        , m_taskRunnerHandle(m_taskRunner)
    {
    }

    void SetUp() override
    {
        PaintArtifactCompositorTest::SetUp();

        cc::LayerTreeSettings settings = WebLayerTreeViewImplForTesting::defaultLayerTreeSettings();
        settings.single_thread_proxy_scheduler = false;
        settings.use_layer_lists = true;
        m_webLayerTreeView = wrapUnique(new WebLayerTreeViewWithOutputSurface(settings));
        m_webLayerTreeView->setRootLayer(*getPaintArtifactCompositor().getWebLayer());
    }

    const cc::PropertyTrees& propertyTrees()
    {
        return *m_webLayerTreeView->layerTreeHost()->GetLayerTree()->property_trees();
    }

    const cc::TransformNode& transformNode(const cc::Layer* layer)
    {
        return *propertyTrees().transform_tree.Node(layer->transform_tree_index());
    }

    void update(const PaintArtifact& artifact)
    {
        PaintArtifactCompositorTest::update(artifact);
        m_webLayerTreeView->layerTreeHost()->LayoutAndUpdateLayers();
    }

private:
    scoped_refptr<base::TestSimpleTaskRunner> m_taskRunner;
    base::ThreadTaskRunnerHandle m_taskRunnerHandle;
    std::unique_ptr<WebLayerTreeViewWithOutputSurface> m_webLayerTreeView;
};

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, EmptyPaintArtifact)
{
    PaintArtifact emptyArtifact;
    update(emptyArtifact);
    EXPECT_TRUE(rootLayer()->children().empty());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, OneChunkWithAnOffset)
{
    TestPaintArtifact artifact;
    artifact.chunk(PaintChunkProperties())
        .rectDrawing(FloatRect(50, -50, 100, 100), Color::white);
    update(artifact.build());

    ASSERT_EQ(1u, contentLayerCount());
    const cc::Layer* child = contentLayerAt(0);
    EXPECT_THAT(child->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::white)));
    EXPECT_EQ(translation(50, -50), child->screen_space_transform());
    EXPECT_EQ(gfx::Size(100, 100), child->bounds());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, OneTransform)
{
    // A 90 degree clockwise rotation about (100, 100).
    RefPtr<TransformPaintPropertyNode> transform = TransformPaintPropertyNode::create(
        nullptr, TransformationMatrix().rotate(90), FloatPoint3D(100, 100, 0));

    TestPaintArtifact artifact;
    artifact.chunk(transform, nullptr, dummyRootEffect())
        .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
    artifact.chunk(nullptr, nullptr, dummyRootEffect())
        .rectDrawing(FloatRect(0, 0, 100, 100), Color::gray);
    artifact.chunk(transform, nullptr, dummyRootEffect())
        .rectDrawing(FloatRect(100, 100, 200, 100), Color::black);
    update(artifact.build());

    ASSERT_EQ(3u, contentLayerCount());
    {
        const cc::Layer* layer = contentLayerAt(0);
        EXPECT_THAT(layer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::white)));
        gfx::RectF mappedRect(0, 0, 100, 100);
        layer->screen_space_transform().TransformRect(&mappedRect);
        EXPECT_EQ(gfx::RectF(100, 0, 100, 100), mappedRect);
    }
    {
        const cc::Layer* layer = contentLayerAt(1);
        EXPECT_THAT(layer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::gray)));
        EXPECT_EQ(gfx::Transform(), layer->screen_space_transform());
    }
    {
        const cc::Layer* layer = contentLayerAt(2);
        EXPECT_THAT(layer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 200, 100), Color::black)));
        gfx::RectF mappedRect(0, 0, 200, 100);
        layer->screen_space_transform().TransformRect(&mappedRect);
        EXPECT_EQ(gfx::RectF(0, 100, 100, 200), mappedRect);
    }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, TransformCombining)
{
    // A translation by (5, 5) within a 2x scale about (10, 10).
    RefPtr<TransformPaintPropertyNode> transform1 = TransformPaintPropertyNode::create(
        nullptr, TransformationMatrix().scale(2), FloatPoint3D(10, 10, 0));
    RefPtr<TransformPaintPropertyNode> transform2 = TransformPaintPropertyNode::create(
        transform1, TransformationMatrix().translate(5, 5), FloatPoint3D());

    TestPaintArtifact artifact;
    artifact.chunk(transform1, nullptr, dummyRootEffect())
        .rectDrawing(FloatRect(0, 0, 300, 200), Color::white);
    artifact.chunk(transform2, nullptr, dummyRootEffect())
        .rectDrawing(FloatRect(0, 0, 300, 200), Color::black);
    update(artifact.build());

    ASSERT_EQ(2u, contentLayerCount());
    {
        const cc::Layer* layer = contentLayerAt(0);
        EXPECT_THAT(layer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::white)));
        gfx::RectF mappedRect(0, 0, 300, 200);
        layer->screen_space_transform().TransformRect(&mappedRect);
        EXPECT_EQ(gfx::RectF(-10, -10, 600, 400), mappedRect);
    }
    {
        const cc::Layer* layer = contentLayerAt(1);
        EXPECT_THAT(layer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::black)));
        gfx::RectF mappedRect(0, 0, 300, 200);
        layer->screen_space_transform().TransformRect(&mappedRect);
        EXPECT_EQ(gfx::RectF(0, 0, 600, 400), mappedRect);
    }
    EXPECT_NE(
        contentLayerAt(0)->transform_tree_index(),
        contentLayerAt(1)->transform_tree_index());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, FlattensInheritedTransform)
{
    for (bool transformIsFlattened : { true, false }) {
        SCOPED_TRACE(transformIsFlattened);

        // The flattens_inherited_transform bit corresponds to whether the _parent_
        // transform node flattens the transform. This is because Blink's notion of
        // flattening determines whether content within the node's local transform
        // is flattened, while cc's notion applies in the parent's coordinate space.
        RefPtr<TransformPaintPropertyNode> transform1 = TransformPaintPropertyNode::create(
            nullptr, TransformationMatrix(), FloatPoint3D());
        RefPtr<TransformPaintPropertyNode> transform2 = TransformPaintPropertyNode::create(
            transform1, TransformationMatrix().rotate3d(0, 45, 0), FloatPoint3D());
        RefPtr<TransformPaintPropertyNode> transform3 = TransformPaintPropertyNode::create(
            transform2, TransformationMatrix().rotate3d(0, 45, 0), FloatPoint3D(),
            transformIsFlattened);

        TestPaintArtifact artifact;
        artifact.chunk(transform3, nullptr, nullptr)
            .rectDrawing(FloatRect(0, 0, 300, 200), Color::white);
        update(artifact.build());

        ASSERT_EQ(1u, contentLayerCount());
        const cc::Layer* layer = contentLayerAt(0);
        EXPECT_THAT(layer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::white)));

        // The leaf transform node should flatten its inherited transform node
        // if and only if the intermediate rotation transform in the Blink tree
        // flattens.
        const cc::TransformNode* transformNode3 = propertyTrees().transform_tree.Node(layer->transform_tree_index());
        EXPECT_EQ(transformIsFlattened, transformNode3->flattens_inherited_transform);

        // Given this, we should expect the correct screen space transform for
        // each case. If the transform was flattened, we should see it getting
        // an effective horizontal scale of 1/sqrt(2) each time, thus it gets
        // half as wide. If the transform was not flattened, we should see an
        // empty rectangle (as the total 90 degree rotation makes it
        // perpendicular to the viewport).
        gfx::RectF rect(0, 0, 100, 100);
        layer->screen_space_transform().TransformRect(&rect);
        if (transformIsFlattened)
            EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 50, 100), rect);
        else
            EXPECT_TRUE(rect.IsEmpty());
    }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, SortingContextID)
{
    // Has no 3D rendering context.
    RefPtr<TransformPaintPropertyNode> transform1 = TransformPaintPropertyNode::create(
        nullptr, TransformationMatrix(), FloatPoint3D());
    // Establishes a 3D rendering context.
    RefPtr<TransformPaintPropertyNode> transform2 = TransformPaintPropertyNode::create(
        transform1, TransformationMatrix(), FloatPoint3D(), false, 1);
    // Extends the 3D rendering context of transform2.
    RefPtr<TransformPaintPropertyNode> transform3 = TransformPaintPropertyNode::create(
        transform2, TransformationMatrix(), FloatPoint3D(), false, 1);
    // Establishes a 3D rendering context distinct from transform2.
    RefPtr<TransformPaintPropertyNode> transform4 = TransformPaintPropertyNode::create(
        transform2, TransformationMatrix(), FloatPoint3D(), false, 2);

    TestPaintArtifact artifact;
    artifact.chunk(transform1, nullptr, dummyRootEffect())
        .rectDrawing(FloatRect(0, 0, 300, 200), Color::white);
    artifact.chunk(transform2, nullptr, dummyRootEffect())
        .rectDrawing(FloatRect(0, 0, 300, 200), Color::lightGray);
    artifact.chunk(transform3, nullptr, dummyRootEffect())
        .rectDrawing(FloatRect(0, 0, 300, 200), Color::darkGray);
    artifact.chunk(transform4, nullptr, dummyRootEffect())
        .rectDrawing(FloatRect(0, 0, 300, 200), Color::black);
    update(artifact.build());

    ASSERT_EQ(4u, contentLayerCount());

    // The white layer is not 3D sorted.
    const cc::Layer* whiteLayer = contentLayerAt(0);
    EXPECT_THAT(whiteLayer->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::white)));
    int whiteSortingContextId = transformNode(whiteLayer).sorting_context_id;
    EXPECT_EQ(whiteLayer->sorting_context_id(), whiteSortingContextId);
    EXPECT_EQ(0, whiteSortingContextId);

    // The light gray layer is 3D sorted.
    const cc::Layer* lightGrayLayer = contentLayerAt(1);
    EXPECT_THAT(lightGrayLayer->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::lightGray)));
    int lightGraySortingContextId = transformNode(lightGrayLayer).sorting_context_id;
    EXPECT_EQ(lightGrayLayer->sorting_context_id(), lightGraySortingContextId);
    EXPECT_NE(0, lightGraySortingContextId);

    // The dark gray layer is 3D sorted with the light gray layer, but has a
    // separate transform node.
    const cc::Layer* darkGrayLayer = contentLayerAt(2);
    EXPECT_THAT(darkGrayLayer->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::darkGray)));
    int darkGraySortingContextId = transformNode(darkGrayLayer).sorting_context_id;
    EXPECT_EQ(darkGrayLayer->sorting_context_id(), darkGraySortingContextId);
    EXPECT_EQ(lightGraySortingContextId, darkGraySortingContextId);
    EXPECT_NE(lightGrayLayer->transform_tree_index(), darkGrayLayer->transform_tree_index());

    // The black layer is 3D sorted, but in a separate context from the previous
    // layers.
    const cc::Layer* blackLayer = contentLayerAt(3);
    EXPECT_THAT(blackLayer->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::black)));
    int blackSortingContextId = transformNode(blackLayer).sorting_context_id;
    EXPECT_EQ(blackLayer->sorting_context_id(), blackSortingContextId);
    EXPECT_NE(0, blackSortingContextId);
    EXPECT_NE(lightGraySortingContextId, blackSortingContextId);
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, OneClip)
{
    RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
        nullptr, nullptr, FloatRoundedRect(100, 100, 300, 200));

    TestPaintArtifact artifact;
    artifact.chunk(nullptr, clip, nullptr)
        .rectDrawing(FloatRect(220, 80, 300, 200), Color::black);
    update(artifact.build());

    ASSERT_EQ(1u, contentLayerCount());
    const cc::Layer* layer = contentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::black)));
    EXPECT_EQ(translation(220, 80), layer->screen_space_transform());

    const cc::ClipNode* clipNode = propertyTrees().clip_tree.Node(layer->clip_tree_index());
    EXPECT_TRUE(clipNode->applies_local_clip);
    EXPECT_TRUE(clipNode->layers_are_clipped);
    EXPECT_EQ(gfx::RectF(100, 100, 300, 200), clipNode->clip);
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, NestedClips)
{
    RefPtr<ClipPaintPropertyNode> clip1 = ClipPaintPropertyNode::create(
        nullptr, nullptr, FloatRoundedRect(100, 100, 700, 700));
    RefPtr<ClipPaintPropertyNode> clip2 = ClipPaintPropertyNode::create(
        clip1, nullptr, FloatRoundedRect(200, 200, 700, 100));

    TestPaintArtifact artifact;
    artifact.chunk(nullptr, clip1, dummyRootEffect())
        .rectDrawing(FloatRect(300, 350, 100, 100), Color::white);
    artifact.chunk(nullptr, clip2, dummyRootEffect())
        .rectDrawing(FloatRect(300, 350, 100, 100), Color::lightGray);
    artifact.chunk(nullptr, clip1, dummyRootEffect())
        .rectDrawing(FloatRect(300, 350, 100, 100), Color::darkGray);
    artifact.chunk(nullptr, clip2, dummyRootEffect())
        .rectDrawing(FloatRect(300, 350, 100, 100), Color::black);
    update(artifact.build());

    ASSERT_EQ(4u, contentLayerCount());

    const cc::Layer* whiteLayer = contentLayerAt(0);
    EXPECT_THAT(whiteLayer->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::white)));
    EXPECT_EQ(translation(300, 350), whiteLayer->screen_space_transform());

    const cc::Layer* lightGrayLayer = contentLayerAt(1);
    EXPECT_THAT(lightGrayLayer->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::lightGray)));
    EXPECT_EQ(translation(300, 350), lightGrayLayer->screen_space_transform());

    const cc::Layer* darkGrayLayer = contentLayerAt(2);
    EXPECT_THAT(darkGrayLayer->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::darkGray)));
    EXPECT_EQ(translation(300, 350), darkGrayLayer->screen_space_transform());

    const cc::Layer* blackLayer = contentLayerAt(3);
    EXPECT_THAT(blackLayer->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::black)));
    EXPECT_EQ(translation(300, 350), blackLayer->screen_space_transform());

    EXPECT_EQ(whiteLayer->clip_tree_index(), darkGrayLayer->clip_tree_index());
    const cc::ClipNode* outerClip = propertyTrees().clip_tree.Node(whiteLayer->clip_tree_index());
    EXPECT_TRUE(outerClip->applies_local_clip);
    EXPECT_TRUE(outerClip->layers_are_clipped);
    EXPECT_EQ(gfx::RectF(100, 100, 700, 700), outerClip->clip);

    EXPECT_EQ(lightGrayLayer->clip_tree_index(), blackLayer->clip_tree_index());
    const cc::ClipNode* innerClip = propertyTrees().clip_tree.Node(blackLayer->clip_tree_index());
    EXPECT_TRUE(innerClip->applies_local_clip);
    EXPECT_TRUE(innerClip->layers_are_clipped);
    EXPECT_EQ(gfx::RectF(200, 200, 700, 100), innerClip->clip);
    EXPECT_EQ(outerClip->id, innerClip->parent_id);
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, DeeplyNestedClips)
{
    Vector<RefPtr<ClipPaintPropertyNode>> clips;
    for (unsigned i = 1; i <= 10; i++) {
        clips.append(ClipPaintPropertyNode::create(
            clips.isEmpty() ? nullptr : clips.last(),
            nullptr, FloatRoundedRect(5 * i, 0, 100, 200 - 10 * i)));
    }

    TestPaintArtifact artifact;
    artifact.chunk(nullptr, clips.last(), dummyRootEffect())
        .rectDrawing(FloatRect(0, 0, 200, 200), Color::white);
    update(artifact.build());

    // Check the drawing layer.
    ASSERT_EQ(1u, contentLayerCount());
    const cc::Layer* drawingLayer = contentLayerAt(0);
    EXPECT_THAT(drawingLayer->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 200, 200), Color::white)));
    EXPECT_EQ(gfx::Transform(), drawingLayer->screen_space_transform());

    // Check the clip nodes.
    const cc::ClipNode* clipNode = propertyTrees().clip_tree.Node(drawingLayer->clip_tree_index());
    for (auto it = clips.rbegin(); it != clips.rend(); ++it) {
        const ClipPaintPropertyNode* paintClipNode = it->get();
        EXPECT_TRUE(clipNode->applies_local_clip);
        EXPECT_TRUE(clipNode->layers_are_clipped);
        EXPECT_EQ(paintClipNode->clipRect().rect(), clipNode->clip);
        clipNode = propertyTrees().clip_tree.Node(clipNode->parent_id);
    }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, SiblingClips)
{
    RefPtr<ClipPaintPropertyNode> commonClip = ClipPaintPropertyNode::create(
        nullptr, nullptr, FloatRoundedRect(0, 0, 800, 600));
    RefPtr<ClipPaintPropertyNode> clip1 = ClipPaintPropertyNode::create(
        commonClip, nullptr, FloatRoundedRect(0, 0, 400, 600));
    RefPtr<ClipPaintPropertyNode> clip2 = ClipPaintPropertyNode::create(
        commonClip, nullptr, FloatRoundedRect(400, 0, 400, 600));

    TestPaintArtifact artifact;
    artifact.chunk(nullptr, clip1, dummyRootEffect())
        .rectDrawing(FloatRect(0, 0, 640, 480), Color::white);
    artifact.chunk(nullptr, clip2, dummyRootEffect())
        .rectDrawing(FloatRect(0, 0, 640, 480), Color::black);
    update(artifact.build());

    ASSERT_EQ(2u, contentLayerCount());

    const cc::Layer* whiteLayer = contentLayerAt(0);
    EXPECT_THAT(whiteLayer->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 640, 480), Color::white)));
    EXPECT_EQ(gfx::Transform(), whiteLayer->screen_space_transform());
    const cc::ClipNode* whiteClip = propertyTrees().clip_tree.Node(whiteLayer->clip_tree_index());
    EXPECT_TRUE(whiteClip->applies_local_clip);
    EXPECT_TRUE(whiteClip->layers_are_clipped);
    ASSERT_EQ(gfx::RectF(0, 0, 400, 600), whiteClip->clip);

    const cc::Layer* blackLayer = contentLayerAt(1);
    EXPECT_THAT(blackLayer->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 640, 480), Color::black)));
    EXPECT_EQ(gfx::Transform(), blackLayer->screen_space_transform());
    const cc::ClipNode* blackClip = propertyTrees().clip_tree.Node(blackLayer->clip_tree_index());
    EXPECT_TRUE(blackClip->applies_local_clip);
    EXPECT_TRUE(blackClip->layers_are_clipped);
    ASSERT_EQ(gfx::RectF(400, 0, 400, 600), blackClip->clip);

    EXPECT_EQ(whiteClip->parent_id, blackClip->parent_id);
    const cc::ClipNode* commonClipNode = propertyTrees().clip_tree.Node(whiteClip->parent_id);
    EXPECT_TRUE(commonClipNode->applies_local_clip);
    EXPECT_TRUE(commonClipNode->layers_are_clipped);
    ASSERT_EQ(gfx::RectF(0, 0, 800, 600), commonClipNode->clip);
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, ForeignLayerPassesThrough)
{
    scoped_refptr<cc::Layer> layer = cc::Layer::Create();

    TestPaintArtifact artifact;
    artifact.chunk(PaintChunkProperties())
        .foreignLayer(FloatPoint(50, 100), IntSize(400, 300), layer);
    update(artifact.build());

    ASSERT_EQ(1u, contentLayerCount());
    EXPECT_EQ(layer, contentLayerAt(0));
    EXPECT_EQ(gfx::Size(400, 300), layer->bounds());
    EXPECT_EQ(translation(50, 100), layer->screen_space_transform());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, EffectTreeConversion)
{
    RefPtr<EffectPaintPropertyNode> effect1 = EffectPaintPropertyNode::create(dummyRootEffect(), 0.5);
    RefPtr<EffectPaintPropertyNode> effect2 = EffectPaintPropertyNode::create(effect1, 0.3);
    RefPtr<EffectPaintPropertyNode> effect3 = EffectPaintPropertyNode::create(dummyRootEffect(), 0.2);

    TestPaintArtifact artifact;
    artifact.chunk(nullptr, nullptr, effect2.get())
        .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
    artifact.chunk(nullptr, nullptr, effect1.get())
        .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
    artifact.chunk(nullptr, nullptr, effect3.get())
        .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
    update(artifact.build());

    ASSERT_EQ(3u, contentLayerCount());

    const cc::EffectTree& effectTree = propertyTrees().effect_tree;
    // Node #0 reserved for null; #1 for root render surface; #2 for dummyRootEffect,
    // plus 3 nodes for those created by this test.
    ASSERT_EQ(6u, effectTree.size());

    const cc::EffectNode& convertedDummyRootEffect = *effectTree.Node(2);
    EXPECT_EQ(1, convertedDummyRootEffect.parent_id);

    const cc::EffectNode& convertedEffect1 = *effectTree.Node(3);
    EXPECT_EQ(convertedDummyRootEffect.id, convertedEffect1.parent_id);
    EXPECT_FLOAT_EQ(0.5, convertedEffect1.opacity);

    const cc::EffectNode& convertedEffect2 = *effectTree.Node(4);
    EXPECT_EQ(convertedEffect1.id, convertedEffect2.parent_id);
    EXPECT_FLOAT_EQ(0.3, convertedEffect2.opacity);

    const cc::EffectNode& convertedEffect3 = *effectTree.Node(5);
    EXPECT_EQ(convertedDummyRootEffect.id, convertedEffect3.parent_id);
    EXPECT_FLOAT_EQ(0.2, convertedEffect3.opacity);

    EXPECT_EQ(convertedEffect2.id, contentLayerAt(0)->effect_tree_index());
    EXPECT_EQ(convertedEffect1.id, contentLayerAt(1)->effect_tree_index());
    EXPECT_EQ(convertedEffect3.id, contentLayerAt(2)->effect_tree_index());
}

} // namespace
} // namespace blink
