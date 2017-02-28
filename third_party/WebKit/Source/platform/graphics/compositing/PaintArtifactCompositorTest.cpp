// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/compositing/PaintArtifactCompositor.h"

#include <memory>

#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/layers/layer.h"
#include "cc/test/fake_compositor_frame_sink.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/GeometryMapper.h"
#include "platform/graphics/paint/PaintArtifact.h"
#include "platform/graphics/paint/ScrollPaintPropertyNode.h"
#include "platform/testing/PaintPropertyTestHelpers.h"
#include "platform/testing/PictureMatchers.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/TestPaintArtifact.h"
#include "platform/testing/WebLayerTreeViewImplForTesting.h"
#include "public/platform/WebLayerScrollClient.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

#define EXPECT_BLINK_FLOAT_RECT_EQ(expected, actual)         \
  do {                                                       \
    EXPECT_FLOAT_EQ((expected).x(), (actual).x());           \
    EXPECT_FLOAT_EQ((expected).y(), (actual).y());           \
    EXPECT_FLOAT_EQ((expected).width(), (actual).width());   \
    EXPECT_FLOAT_EQ((expected).height(), (actual).height()); \
  } while (false)

using ::blink::testing::createOpacityOnlyEffect;
using ::testing::Pointee;

PaintChunkProperties defaultPaintChunkProperties() {
  PropertyTreeState propertyTreeState(TransformPaintPropertyNode::root(),
                                      ClipPaintPropertyNode::root(),
                                      EffectPaintPropertyNode::root());
  return PaintChunkProperties(propertyTreeState);
}

gfx::Transform translation(SkMScalar x, SkMScalar y) {
  gfx::Transform transform;
  transform.Translate(x, y);
  return transform;
}

class WebLayerTreeViewWithCompositorFrameSink
    : public WebLayerTreeViewImplForTesting {
 public:
  WebLayerTreeViewWithCompositorFrameSink(const cc::LayerTreeSettings& settings)
      : WebLayerTreeViewImplForTesting(settings) {}

  // cc::LayerTreeHostClient
  void RequestNewCompositorFrameSink() override {
    layerTreeHost()->SetCompositorFrameSink(
        cc::FakeCompositorFrameSink::Create3d());
  }
};

class PaintArtifactCompositorTestWithPropertyTrees
    : public ::testing::Test,
      private ScopedSlimmingPaintV2ForTest {
 protected:
  PaintArtifactCompositorTestWithPropertyTrees()
      : ScopedSlimmingPaintV2ForTest(true),
        m_taskRunner(new base::TestSimpleTaskRunner),
        m_taskRunnerHandle(m_taskRunner) {}

  void SetUp() override {
    // Delay constructing the compositor until after the feature is set.
    m_paintArtifactCompositor = PaintArtifactCompositor::create();
    m_paintArtifactCompositor->enableExtraDataForTesting();

    cc::LayerTreeSettings settings =
        WebLayerTreeViewImplForTesting::defaultLayerTreeSettings();
    settings.single_thread_proxy_scheduler = false;
    settings.use_layer_lists = true;
    m_webLayerTreeView =
        WTF::makeUnique<WebLayerTreeViewWithCompositorFrameSink>(settings);
    m_webLayerTreeView->setRootLayer(*m_paintArtifactCompositor->getWebLayer());
  }

  const cc::PropertyTrees& propertyTrees() {
    return *m_webLayerTreeView->layerTreeHost()
                ->property_trees();
  }

  int elementIdToEffectNodeIndex(CompositorElementId elementId) {
    return m_webLayerTreeView->layerTreeHost()
        ->property_trees()
        ->element_id_to_effect_node_index[elementId];
  }

  int elementIdToTransformNodeIndex(CompositorElementId elementId) {
    return m_webLayerTreeView->layerTreeHost()
        ->property_trees()
        ->element_id_to_transform_node_index[elementId];
  }

  int elementIdToScrollNodeIndex(CompositorElementId elementId) {
    return m_webLayerTreeView->layerTreeHost()
        ->property_trees()
        ->element_id_to_scroll_node_index[elementId];
  }

  const cc::TransformNode& transformNode(const cc::Layer* layer) {
    return *propertyTrees().transform_tree.Node(layer->transform_tree_index());
  }

  void update(const PaintArtifact& artifact) {
    std::unique_ptr<GeometryMapper> geometryMapper = GeometryMapper::create();
    m_paintArtifactCompositor->update(artifact, nullptr, false,
                                      *geometryMapper);
    m_webLayerTreeView->layerTreeHost()->LayoutAndUpdateLayers();
  }

  cc::Layer* rootLayer() { return m_paintArtifactCompositor->rootLayer(); }

  size_t contentLayerCount() {
    return m_paintArtifactCompositor->getExtraDataForTesting()
        ->contentLayers.size();
  }

  cc::Layer* contentLayerAt(unsigned index) {
    return m_paintArtifactCompositor->getExtraDataForTesting()
        ->contentLayers[index]
        .get();
  }

 private:
  std::unique_ptr<PaintArtifactCompositor> m_paintArtifactCompositor;
  scoped_refptr<base::TestSimpleTaskRunner> m_taskRunner;
  base::ThreadTaskRunnerHandle m_taskRunnerHandle;
  std::unique_ptr<WebLayerTreeViewWithCompositorFrameSink> m_webLayerTreeView;
};

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, EmptyPaintArtifact) {
  PaintArtifact emptyArtifact;
  update(emptyArtifact);
  EXPECT_TRUE(rootLayer()->children().empty());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, OneChunkWithAnOffset) {
  TestPaintArtifact artifact;
  artifact.chunk(defaultPaintChunkProperties())
      .rectDrawing(FloatRect(50, -50, 100, 100), Color::white);
  update(artifact.build());

  ASSERT_EQ(1u, contentLayerCount());
  const cc::Layer* child = contentLayerAt(0);
  EXPECT_THAT(child->GetPicture(),
              Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::white)));
  EXPECT_EQ(translation(50, -50), child->screen_space_transform());
  EXPECT_EQ(gfx::Size(100, 100), child->bounds());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, OneTransform) {
  // A 90 degree clockwise rotation about (100, 100).
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(
          TransformPaintPropertyNode::root(), TransformationMatrix().rotate(90),
          FloatPoint3D(100, 100, 0), false, 0, CompositingReason3DTransform);

  TestPaintArtifact artifact;
  artifact
      .chunk(transform, ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
  artifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::gray);
  artifact
      .chunk(transform, ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(100, 100, 200, 100), Color::black);
  update(artifact.build());

  ASSERT_EQ(2u, contentLayerCount());
  {
    const cc::Layer* layer = contentLayerAt(0);

    Vector<RectWithColor> rectsWithColor;
    rectsWithColor.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::white));
    rectsWithColor.push_back(
        RectWithColor(FloatRect(100, 100, 200, 100), Color::black));

    EXPECT_THAT(layer->GetPicture(), Pointee(drawsRectangles(rectsWithColor)));
    gfx::RectF mappedRect(0, 0, 100, 100);
    layer->screen_space_transform().TransformRect(&mappedRect);
    EXPECT_EQ(gfx::RectF(100, 0, 100, 100), mappedRect);
  }
  {
    const cc::Layer* layer = contentLayerAt(1);
    EXPECT_THAT(
        layer->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::gray)));
    EXPECT_EQ(gfx::Transform(), layer->screen_space_transform());
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, TransformCombining) {
  // A translation by (5, 5) within a 2x scale about (10, 10).
  RefPtr<TransformPaintPropertyNode> transform1 =
      TransformPaintPropertyNode::create(
          TransformPaintPropertyNode::root(), TransformationMatrix().scale(2),
          FloatPoint3D(10, 10, 0), false, 0, CompositingReason3DTransform);
  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::create(
          transform1, TransformationMatrix().translate(5, 5), FloatPoint3D(),
          false, 0, CompositingReason3DTransform);

  TestPaintArtifact artifact;
  artifact
      .chunk(transform1, ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 300, 200), Color::white);
  artifact
      .chunk(transform2, ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 300, 200), Color::black);
  update(artifact.build());

  ASSERT_EQ(2u, contentLayerCount());
  {
    const cc::Layer* layer = contentLayerAt(0);
    EXPECT_THAT(
        layer->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::white)));
    gfx::RectF mappedRect(0, 0, 300, 200);
    layer->screen_space_transform().TransformRect(&mappedRect);
    EXPECT_EQ(gfx::RectF(-10, -10, 600, 400), mappedRect);
  }
  {
    const cc::Layer* layer = contentLayerAt(1);
    EXPECT_THAT(
        layer->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::black)));
    gfx::RectF mappedRect(0, 0, 300, 200);
    layer->screen_space_transform().TransformRect(&mappedRect);
    EXPECT_EQ(gfx::RectF(0, 0, 600, 400), mappedRect);
  }
  EXPECT_NE(contentLayerAt(0)->transform_tree_index(),
            contentLayerAt(1)->transform_tree_index());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees,
       FlattensInheritedTransform) {
  for (bool transformIsFlattened : {true, false}) {
    SCOPED_TRACE(transformIsFlattened);

    // The flattens_inherited_transform bit corresponds to whether the _parent_
    // transform node flattens the transform. This is because Blink's notion of
    // flattening determines whether content within the node's local transform
    // is flattened, while cc's notion applies in the parent's coordinate space.
    RefPtr<TransformPaintPropertyNode> transform1 =
        TransformPaintPropertyNode::create(TransformPaintPropertyNode::root(),
                                           TransformationMatrix(),
                                           FloatPoint3D());
    RefPtr<TransformPaintPropertyNode> transform2 =
        TransformPaintPropertyNode::create(
            transform1, TransformationMatrix().rotate3d(0, 45, 0),
            FloatPoint3D());
    RefPtr<TransformPaintPropertyNode> transform3 =
        TransformPaintPropertyNode::create(
            transform2, TransformationMatrix().rotate3d(0, 45, 0),
            FloatPoint3D(), transformIsFlattened);

    TestPaintArtifact artifact;
    artifact
        .chunk(transform3, ClipPaintPropertyNode::root(),
               EffectPaintPropertyNode::root())
        .rectDrawing(FloatRect(0, 0, 300, 200), Color::white);
    update(artifact.build());

    ASSERT_EQ(1u, contentLayerCount());
    const cc::Layer* layer = contentLayerAt(0);
    EXPECT_THAT(
        layer->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::white)));

    // The leaf transform node should flatten its inherited transform node
    // if and only if the intermediate rotation transform in the Blink tree
    // flattens.
    const cc::TransformNode* transformNode3 =
        propertyTrees().transform_tree.Node(layer->transform_tree_index());
    EXPECT_EQ(transformIsFlattened,
              transformNode3->flattens_inherited_transform);

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

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, SortingContextID) {
  // Has no 3D rendering context.
  RefPtr<TransformPaintPropertyNode> transform1 =
      TransformPaintPropertyNode::create(TransformPaintPropertyNode::root(),
                                         TransformationMatrix(),
                                         FloatPoint3D());
  // Establishes a 3D rendering context.
  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::create(transform1, TransformationMatrix(),
                                         FloatPoint3D(), false, 1,
                                         CompositingReason3DTransform);
  // Extends the 3D rendering context of transform2.
  RefPtr<TransformPaintPropertyNode> transform3 =
      TransformPaintPropertyNode::create(transform2, TransformationMatrix(),
                                         FloatPoint3D(), false, 1,
                                         CompositingReason3DTransform);
  // Establishes a 3D rendering context distinct from transform2.
  RefPtr<TransformPaintPropertyNode> transform4 =
      TransformPaintPropertyNode::create(transform2, TransformationMatrix(),
                                         FloatPoint3D(), false, 2,
                                         CompositingReason3DTransform);

  TestPaintArtifact artifact;
  artifact
      .chunk(transform1, ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 300, 200), Color::white);
  artifact
      .chunk(transform2, ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 300, 200), Color::lightGray);
  artifact
      .chunk(transform3, ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 300, 200), Color::darkGray);
  artifact
      .chunk(transform4, ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
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
  EXPECT_THAT(
      lightGrayLayer->GetPicture(),
      Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::lightGray)));
  int lightGraySortingContextId =
      transformNode(lightGrayLayer).sorting_context_id;
  EXPECT_NE(0, lightGraySortingContextId);

  // The dark gray layer is 3D sorted with the light gray layer, but has a
  // separate transform node.
  const cc::Layer* darkGrayLayer = contentLayerAt(2);
  EXPECT_THAT(
      darkGrayLayer->GetPicture(),
      Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::darkGray)));
  int darkGraySortingContextId =
      transformNode(darkGrayLayer).sorting_context_id;
  EXPECT_EQ(lightGraySortingContextId, darkGraySortingContextId);
  EXPECT_NE(lightGrayLayer->transform_tree_index(),
            darkGrayLayer->transform_tree_index());

  // The black layer is 3D sorted, but in a separate context from the previous
  // layers.
  const cc::Layer* blackLayer = contentLayerAt(3);
  EXPECT_THAT(blackLayer->GetPicture(),
              Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::black)));
  int blackSortingContextId = transformNode(blackLayer).sorting_context_id;
  EXPECT_NE(0, blackSortingContextId);
  EXPECT_NE(lightGraySortingContextId, blackSortingContextId);
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, OneClip) {
  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      FloatRoundedRect(100, 100, 300, 200));

  TestPaintArtifact artifact;
  artifact
      .chunk(TransformPaintPropertyNode::root(), clip,
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(220, 80, 300, 200), Color::black);
  update(artifact.build());

  ASSERT_EQ(1u, contentLayerCount());
  const cc::Layer* layer = contentLayerAt(0);
  EXPECT_THAT(layer->GetPicture(),
              Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::black)));
  EXPECT_EQ(translation(220, 80), layer->screen_space_transform());

  const cc::ClipNode* clipNode =
      propertyTrees().clip_tree.Node(layer->clip_tree_index());
  EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP, clipNode->clip_type);
  EXPECT_TRUE(clipNode->layers_are_clipped);
  EXPECT_EQ(gfx::RectF(100, 100, 300, 200), clipNode->clip);
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, NestedClips) {
  RefPtr<ClipPaintPropertyNode> clip1 = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      FloatRoundedRect(100, 100, 700, 700),
      CompositingReasonOverflowScrollingTouch);
  RefPtr<ClipPaintPropertyNode> clip2 =
      ClipPaintPropertyNode::create(clip1, TransformPaintPropertyNode::root(),
                                    FloatRoundedRect(200, 200, 700, 700),
                                    CompositingReasonOverflowScrollingTouch);

  TestPaintArtifact artifact;
  artifact
      .chunk(TransformPaintPropertyNode::root(), clip1,
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(300, 350, 100, 100), Color::white);
  artifact
      .chunk(TransformPaintPropertyNode::root(), clip2,
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(300, 350, 100, 100), Color::lightGray);
  artifact
      .chunk(TransformPaintPropertyNode::root(), clip1,
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(300, 350, 100, 100), Color::darkGray);
  artifact
      .chunk(TransformPaintPropertyNode::root(), clip2,
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(300, 350, 100, 100), Color::black);
  update(artifact.build());

  ASSERT_EQ(4u, contentLayerCount());

  const cc::Layer* whiteLayer = contentLayerAt(0);
  EXPECT_THAT(whiteLayer->GetPicture(),
              Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::white)));
  EXPECT_EQ(translation(300, 350), whiteLayer->screen_space_transform());

  const cc::Layer* lightGrayLayer = contentLayerAt(1);
  EXPECT_THAT(
      lightGrayLayer->GetPicture(),
      Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::lightGray)));
  EXPECT_EQ(translation(300, 350), lightGrayLayer->screen_space_transform());

  const cc::Layer* darkGrayLayer = contentLayerAt(2);
  EXPECT_THAT(
      darkGrayLayer->GetPicture(),
      Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::darkGray)));
  EXPECT_EQ(translation(300, 350), darkGrayLayer->screen_space_transform());

  const cc::Layer* blackLayer = contentLayerAt(3);
  EXPECT_THAT(blackLayer->GetPicture(),
              Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::black)));
  EXPECT_EQ(translation(300, 350), blackLayer->screen_space_transform());

  EXPECT_EQ(whiteLayer->clip_tree_index(), darkGrayLayer->clip_tree_index());
  const cc::ClipNode* outerClip =
      propertyTrees().clip_tree.Node(whiteLayer->clip_tree_index());
  EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP, outerClip->clip_type);
  EXPECT_TRUE(outerClip->layers_are_clipped);
  EXPECT_EQ(gfx::RectF(100, 100, 700, 700), outerClip->clip);

  EXPECT_EQ(lightGrayLayer->clip_tree_index(), blackLayer->clip_tree_index());
  const cc::ClipNode* innerClip =
      propertyTrees().clip_tree.Node(blackLayer->clip_tree_index());
  EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP, innerClip->clip_type);
  EXPECT_TRUE(innerClip->layers_are_clipped);
  EXPECT_EQ(gfx::RectF(200, 200, 700, 700), innerClip->clip);
  EXPECT_EQ(outerClip->id, innerClip->parent_id);
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, DeeplyNestedClips) {
  Vector<RefPtr<ClipPaintPropertyNode>> clips;
  for (unsigned i = 1; i <= 10; i++) {
    clips.push_back(ClipPaintPropertyNode::create(
        clips.isEmpty() ? ClipPaintPropertyNode::root() : clips.back(),
        TransformPaintPropertyNode::root(),
        FloatRoundedRect(5 * i, 0, 100, 200 - 10 * i)));
  }

  TestPaintArtifact artifact;
  artifact
      .chunk(TransformPaintPropertyNode::root(), clips.back(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 200, 200), Color::white);
  update(artifact.build());

  // Check the drawing layer.
  ASSERT_EQ(1u, contentLayerCount());
  const cc::Layer* drawingLayer = contentLayerAt(0);
  EXPECT_THAT(drawingLayer->GetPicture(),
              Pointee(drawsRectangle(FloatRect(0, 0, 200, 200), Color::white)));
  EXPECT_EQ(gfx::Transform(), drawingLayer->screen_space_transform());

  // Check the clip nodes.
  const cc::ClipNode* clipNode =
      propertyTrees().clip_tree.Node(drawingLayer->clip_tree_index());
  for (auto it = clips.rbegin(); it != clips.rend(); ++it) {
    const ClipPaintPropertyNode* paintClipNode = it->get();
    EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP, clipNode->clip_type);
    EXPECT_TRUE(clipNode->layers_are_clipped);
    EXPECT_EQ(paintClipNode->clipRect().rect(), clipNode->clip);
    clipNode = propertyTrees().clip_tree.Node(clipNode->parent_id);
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, SiblingClips) {
  RefPtr<ClipPaintPropertyNode> commonClip = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      FloatRoundedRect(0, 0, 800, 600));
  RefPtr<ClipPaintPropertyNode> clip1 = ClipPaintPropertyNode::create(
      commonClip, TransformPaintPropertyNode::root(),
      FloatRoundedRect(0, 0, 400, 600));
  RefPtr<ClipPaintPropertyNode> clip2 = ClipPaintPropertyNode::create(
      commonClip, TransformPaintPropertyNode::root(),
      FloatRoundedRect(400, 0, 400, 600));

  TestPaintArtifact artifact;
  artifact
      .chunk(TransformPaintPropertyNode::root(), clip1,
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 640, 480), Color::white);
  artifact
      .chunk(TransformPaintPropertyNode::root(), clip2,
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 640, 480), Color::black);
  update(artifact.build());

  ASSERT_EQ(2u, contentLayerCount());

  const cc::Layer* whiteLayer = contentLayerAt(0);
  EXPECT_THAT(whiteLayer->GetPicture(),
              Pointee(drawsRectangle(FloatRect(0, 0, 640, 480), Color::white)));
  EXPECT_EQ(gfx::Transform(), whiteLayer->screen_space_transform());
  const cc::ClipNode* whiteClip =
      propertyTrees().clip_tree.Node(whiteLayer->clip_tree_index());
  EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP, whiteClip->clip_type);
  EXPECT_TRUE(whiteClip->layers_are_clipped);
  ASSERT_EQ(gfx::RectF(0, 0, 400, 600), whiteClip->clip);

  const cc::Layer* blackLayer = contentLayerAt(1);
  EXPECT_THAT(blackLayer->GetPicture(),
              Pointee(drawsRectangle(FloatRect(0, 0, 640, 480), Color::black)));
  EXPECT_EQ(gfx::Transform(), blackLayer->screen_space_transform());
  const cc::ClipNode* blackClip =
      propertyTrees().clip_tree.Node(blackLayer->clip_tree_index());
  EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP, blackClip->clip_type);
  EXPECT_TRUE(blackClip->layers_are_clipped);
  ASSERT_EQ(gfx::RectF(400, 0, 400, 600), blackClip->clip);

  EXPECT_EQ(whiteClip->parent_id, blackClip->parent_id);
  const cc::ClipNode* commonClipNode =
      propertyTrees().clip_tree.Node(whiteClip->parent_id);
  EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP,
            commonClipNode->clip_type);
  EXPECT_TRUE(commonClipNode->layers_are_clipped);
  ASSERT_EQ(gfx::RectF(0, 0, 800, 600), commonClipNode->clip);
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees,
       ForeignLayerPassesThrough) {
  scoped_refptr<cc::Layer> layer = cc::Layer::Create();

  TestPaintArtifact testArtifact;
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
  testArtifact.chunk(defaultPaintChunkProperties())
      .foreignLayer(FloatPoint(50, 60), IntSize(400, 300), layer);
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::gray);

  const PaintArtifact& artifact = testArtifact.build();

  ASSERT_EQ(3u, artifact.paintChunks().size());
  PaintArtifactCompositor::PendingLayer pendingLayer1(
      artifact.paintChunks()[0]);
  // Foreign layers can't merge.
  EXPECT_FALSE(PaintArtifactCompositor::canMergeInto(
      artifact, artifact.paintChunks()[1], pendingLayer1));
  PaintArtifactCompositor::PendingLayer pendingLayer2(
      artifact.paintChunks()[1]);
  EXPECT_FALSE(PaintArtifactCompositor::canMergeInto(
      artifact, artifact.paintChunks()[2], pendingLayer2));

  update(artifact);

  ASSERT_EQ(3u, contentLayerCount());
  EXPECT_EQ(layer, contentLayerAt(1));
  EXPECT_EQ(gfx::Size(400, 300), layer->bounds());
  EXPECT_EQ(translation(50, 60), layer->screen_space_transform());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, EffectTreeConversion) {
  RefPtr<EffectPaintPropertyNode> effect1 =
      createOpacityOnlyEffect(EffectPaintPropertyNode::root(), 0.5);
  RefPtr<EffectPaintPropertyNode> effect2 =
      createOpacityOnlyEffect(effect1, 0.3);
  RefPtr<EffectPaintPropertyNode> effect3 =
      createOpacityOnlyEffect(EffectPaintPropertyNode::root(), 0.2);

  TestPaintArtifact artifact;
  artifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             effect2.get())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
  artifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             effect1.get())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
  artifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             effect3.get())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
  update(artifact.build());

  ASSERT_EQ(3u, contentLayerCount());

  const cc::EffectTree& effectTree = propertyTrees().effect_tree;
  // Node #0 reserved for null; #1 for root render surface; #2 for
  // EffectPaintPropertyNode::root(), plus 3 nodes for those created by
  // this test.
  ASSERT_EQ(5u, effectTree.size());

  const cc::EffectNode& convertedRootEffect = *effectTree.Node(1);
  EXPECT_EQ(-1, convertedRootEffect.parent_id);

  const cc::EffectNode& convertedEffect1 = *effectTree.Node(2);
  EXPECT_EQ(convertedRootEffect.id, convertedEffect1.parent_id);
  EXPECT_FLOAT_EQ(0.5, convertedEffect1.opacity);

  const cc::EffectNode& convertedEffect2 = *effectTree.Node(3);
  EXPECT_EQ(convertedEffect1.id, convertedEffect2.parent_id);
  EXPECT_FLOAT_EQ(0.3, convertedEffect2.opacity);

  const cc::EffectNode& convertedEffect3 = *effectTree.Node(4);
  EXPECT_EQ(convertedRootEffect.id, convertedEffect3.parent_id);
  EXPECT_FLOAT_EQ(0.2, convertedEffect3.opacity);

  EXPECT_EQ(convertedEffect2.id, contentLayerAt(0)->effect_tree_index());
  EXPECT_EQ(convertedEffect1.id, contentLayerAt(1)->effect_tree_index());
  EXPECT_EQ(convertedEffect3.id, contentLayerAt(2)->effect_tree_index());
}

class FakeScrollClient : public WebLayerScrollClient {
 public:
  FakeScrollClient() : didScrollCount(0) {}

  void didScroll(const gfx::ScrollOffset& scrollOffset) final {
    didScrollCount++;
    lastScrollOffset = scrollOffset;
  };

  gfx::ScrollOffset lastScrollOffset;
  unsigned didScrollCount;
};

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, OneScrollNode) {
  FakeScrollClient scrollClient;

  CompositorElementId expectedCompositorElementId = CompositorElementId(2, 0);
  RefPtr<TransformPaintPropertyNode> scrollTranslation =
      TransformPaintPropertyNode::createScrollTranslation(
          TransformPaintPropertyNode::root(),
          TransformationMatrix().translate(7, 9), FloatPoint3D(), false, 0,
          CompositingReasonNone, expectedCompositorElementId,
          ScrollPaintPropertyNode::root(), IntSize(11, 13), IntSize(27, 31),
          true, false, 0 /* mainThreadScrollingReasons */, &scrollClient);

  TestPaintArtifact artifact;
  artifact
      .chunk(scrollTranslation, ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(11, 13, 17, 19), Color::white);
  update(artifact.build());

  const cc::ScrollTree& scrollTree = propertyTrees().scroll_tree;
  // Node #0 reserved for null; #1 for root render surface.
  ASSERT_EQ(3u, scrollTree.size());
  const cc::ScrollNode& scrollNode = *scrollTree.Node(2);
  EXPECT_EQ(gfx::Size(11, 13), scrollNode.scroll_clip_layer_bounds);
  EXPECT_EQ(gfx::Size(27, 31), scrollNode.bounds);
  EXPECT_TRUE(scrollNode.user_scrollable_horizontal);
  EXPECT_FALSE(scrollNode.user_scrollable_vertical);
  EXPECT_EQ(1, scrollNode.parent_id);
  EXPECT_EQ(expectedCompositorElementId, scrollNode.element_id);
  EXPECT_EQ(scrollNode.id,
            elementIdToScrollNodeIndex(expectedCompositorElementId));
  EXPECT_EQ(expectedCompositorElementId, contentLayerAt(0)->element_id());

  const cc::TransformTree& transformTree = propertyTrees().transform_tree;
  const cc::TransformNode& transformNode =
      *transformTree.Node(scrollNode.transform_id);
  EXPECT_TRUE(transformNode.local.IsIdentity());
  EXPECT_EQ(gfx::ScrollOffset(-7, -9), transformNode.scroll_offset);

  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            scrollNode.main_thread_scrolling_reasons);

  auto* layer = contentLayerAt(0);
  EXPECT_EQ(layer->id(), scrollNode.owning_layer_id);
  auto scrollNodeIndexIt =
      propertyTrees().layer_id_to_scroll_node_index.find(layer->id());
  EXPECT_EQ(scrollNodeIndexIt->second, scrollNode.id);

  EXPECT_EQ(0u, scrollClient.didScrollCount);
  layer->SetScrollOffsetFromImplSide(gfx::ScrollOffset(1, 2));
  EXPECT_EQ(1u, scrollClient.didScrollCount);
  EXPECT_EQ(gfx::ScrollOffset(1, 2), scrollClient.lastScrollOffset);
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, TransformUnderScrollNode) {
  RefPtr<TransformPaintPropertyNode> scrollTranslation =
      TransformPaintPropertyNode::createScrollTranslation(
          TransformPaintPropertyNode::root(),
          TransformationMatrix().translate(7, 9), FloatPoint3D(), false, 0,
          CompositingReasonNone, CompositorElementId(),
          ScrollPaintPropertyNode::root(), IntSize(11, 13), IntSize(27, 31),
          true, false, 0 /* mainThreadScrollingReasons */, nullptr);

  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(
          scrollTranslation, TransformationMatrix(), FloatPoint3D(), false, 0,
          CompositingReason3DTransform);

  TestPaintArtifact artifact;
  artifact
      .chunk(scrollTranslation, ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(2, 4, 6, 8), Color::black)
      .chunk(transform, ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(1, 3, 5, 7), Color::white);
  update(artifact.build());

  const cc::ScrollTree& scrollTree = propertyTrees().scroll_tree;
  // Node #0 reserved for null; #1 for root render surface.
  ASSERT_EQ(3u, scrollTree.size());
  const cc::ScrollNode& scrollNode = *scrollTree.Node(2);

  // Both layers should refer to the same scroll tree node.
  EXPECT_EQ(scrollNode.id, contentLayerAt(0)->scroll_tree_index());
  EXPECT_EQ(scrollNode.id, contentLayerAt(1)->scroll_tree_index());

  const cc::TransformTree& transformTree = propertyTrees().transform_tree;
  const cc::TransformNode& scrollTransformNode =
      *transformTree.Node(scrollNode.transform_id);
  // The layers have different transform nodes.
  EXPECT_EQ(scrollTransformNode.id, contentLayerAt(0)->transform_tree_index());
  EXPECT_NE(scrollTransformNode.id, contentLayerAt(1)->transform_tree_index());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, NestedScrollNodes) {
  RefPtr<EffectPaintPropertyNode> effect =
      createOpacityOnlyEffect(EffectPaintPropertyNode::root(), 0.5);

  CompositorElementId expectedCompositorElementIdA = CompositorElementId(2, 0);
  RefPtr<TransformPaintPropertyNode> scrollTranslationA =
      TransformPaintPropertyNode::createScrollTranslation(
          TransformPaintPropertyNode::root(),
          TransformationMatrix().translate(11, 13), FloatPoint3D(), false, 0,
          CompositingReasonNone, expectedCompositorElementIdA,
          ScrollPaintPropertyNode::root(), IntSize(2, 3), IntSize(5, 7), false,
          true, MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects,
          nullptr);

  CompositorElementId expectedCompositorElementIdB = CompositorElementId(3, 0);
  RefPtr<TransformPaintPropertyNode> scrollTranslationB =
      TransformPaintPropertyNode::createScrollTranslation(
          scrollTranslationA, TransformationMatrix().translate(37, 41),
          FloatPoint3D(), false, 0, CompositingReasonNone,
          expectedCompositorElementIdB, scrollTranslationA->scrollNode(),
          IntSize(19, 23), IntSize(29, 31), true, false,
          0 /* mainThreadScrollingReasons */, nullptr);
  TestPaintArtifact artifact;
  artifact.chunk(scrollTranslationA, ClipPaintPropertyNode::root(), effect)
      .rectDrawing(FloatRect(7, 11, 13, 17), Color::white);
  artifact.chunk(scrollTranslationB, ClipPaintPropertyNode::root(), effect)
      .rectDrawing(FloatRect(1, 2, 3, 5), Color::white);
  update(artifact.build());

  const cc::ScrollTree& scrollTree = propertyTrees().scroll_tree;
  // Node #0 reserved for null; #1 for root render surface.
  ASSERT_EQ(3u, scrollTree.size());
  const cc::ScrollNode& scrollNode = *scrollTree.Node(2);
  EXPECT_EQ(gfx::Size(2, 3), scrollNode.scroll_clip_layer_bounds);
  EXPECT_EQ(gfx::Size(5, 7), scrollNode.bounds);
  EXPECT_FALSE(scrollNode.user_scrollable_horizontal);
  EXPECT_TRUE(scrollNode.user_scrollable_vertical);
  EXPECT_EQ(1, scrollNode.parent_id);
  EXPECT_EQ(expectedCompositorElementIdA, scrollNode.element_id);
  EXPECT_EQ(scrollNode.id,
            elementIdToScrollNodeIndex(expectedCompositorElementIdA));

  EXPECT_EQ(expectedCompositorElementIdA, contentLayerAt(0)->element_id());

  const cc::TransformTree& transformTree = propertyTrees().transform_tree;
  const cc::TransformNode& transformNodeA =
      *transformTree.Node(scrollNode.transform_id);
  EXPECT_TRUE(transformNodeA.local.IsIdentity());
  EXPECT_EQ(gfx::ScrollOffset(-11, -13), transformNodeA.scroll_offset);

  EXPECT_TRUE(scrollNode.main_thread_scrolling_reasons &
              MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, MergeSimpleChunks) {
  TestPaintArtifact testArtifact;
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 200, 300), Color::gray);

  const PaintArtifact& artifact = testArtifact.build();

  ASSERT_EQ(2u, artifact.paintChunks().size());
  PaintArtifactCompositor::PendingLayer pendingLayer(artifact.paintChunks()[0]);
  EXPECT_TRUE(PaintArtifactCompositor::canMergeInto(
      artifact, artifact.paintChunks()[1], pendingLayer));

  update(artifact);

  ASSERT_EQ(1u, contentLayerCount());
  {
    Vector<RectWithColor> rectsWithColor;
    rectsWithColor.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::white));
    rectsWithColor.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::gray));

    const cc::Layer* layer = contentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(), Pointee(drawsRectangles(rectsWithColor)));
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, MergeClip) {
  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      FloatRoundedRect(10, 20, 50, 60));

  TestPaintArtifact testArtifact;
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), clip.get(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 200, 300), Color::black);
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 300, 400), Color::gray);

  const PaintArtifact& artifact = testArtifact.build();

  ASSERT_EQ(3u, artifact.paintChunks().size());
  PaintArtifactCompositor::PendingLayer pendingLayer(artifact.paintChunks()[0]);
  EXPECT_TRUE(PaintArtifactCompositor::canMergeInto(
      artifact, artifact.paintChunks()[1], pendingLayer));
  pendingLayer.add(artifact.paintChunks()[1], nullptr);
  EXPECT_TRUE(PaintArtifactCompositor::canMergeInto(
      artifact, artifact.paintChunks()[2], pendingLayer));

  update(artifact);

  ASSERT_EQ(1u, contentLayerCount());
  {
    Vector<RectWithColor> rectsWithColor;
    rectsWithColor.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::white));
    // Clip is applied to this PaintChunk.
    rectsWithColor.push_back(
        RectWithColor(FloatRect(10, 20, 50, 60), Color::black));
    rectsWithColor.push_back(
        RectWithColor(FloatRect(0, 0, 300, 400), Color::gray));

    const cc::Layer* layer = contentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(), Pointee(drawsRectangles(rectsWithColor)));
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, Merge2DTransform) {
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(
          TransformPaintPropertyNode::root(),
          TransformationMatrix().translate(50, 50), FloatPoint3D(100, 100, 0),
          false, 0);

  TestPaintArtifact testArtifact;
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
  testArtifact
      .chunk(transform.get(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::black);
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 200, 300), Color::gray);

  const PaintArtifact& artifact = testArtifact.build();

  ASSERT_EQ(3u, artifact.paintChunks().size());
  PaintArtifactCompositor::PendingLayer pendingLayer(artifact.paintChunks()[0]);
  EXPECT_TRUE(PaintArtifactCompositor::canMergeInto(
      artifact, artifact.paintChunks()[1], pendingLayer));
  pendingLayer.add(artifact.paintChunks()[1], nullptr);
  EXPECT_TRUE(PaintArtifactCompositor::canMergeInto(
      artifact, artifact.paintChunks()[2], pendingLayer));

  update(artifact);

  ASSERT_EQ(1u, contentLayerCount());
  {
    Vector<RectWithColor> rectsWithColor;
    rectsWithColor.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::white));
    // Transform is applied to this PaintChunk.
    rectsWithColor.push_back(
        RectWithColor(FloatRect(50, 50, 100, 100), Color::black));
    rectsWithColor.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::gray));

    const cc::Layer* layer = contentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(), Pointee(drawsRectangles(rectsWithColor)));
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees,
       Merge2DTransformDirectAncestor) {
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(
          TransformPaintPropertyNode::root(), TransformationMatrix(),
          FloatPoint3D(), false, 0, CompositingReason3DTransform);

  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::create(
          transform.get(), TransformationMatrix().translate(50, 50),
          FloatPoint3D(100, 100, 0), false, 0);

  TestPaintArtifact testArtifact;
  testArtifact
      .chunk(transform.get(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
  // The second chunk can merge into the first because it has a descendant
  // state of the first's transform and no direct compositing reason.
  testArtifact
      .chunk(transform2.get(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::black);

  const PaintArtifact& artifact = testArtifact.build();

  ASSERT_EQ(2u, artifact.paintChunks().size());
  PaintArtifactCompositor::PendingLayer pendingLayer(artifact.paintChunks()[0]);
  EXPECT_TRUE(PaintArtifactCompositor::canMergeInto(
      artifact, artifact.paintChunks()[1], pendingLayer));

  update(artifact);

  ASSERT_EQ(1u, contentLayerCount());
  {
    Vector<RectWithColor> rectsWithColor;
    rectsWithColor.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::white));
    // Transform is applied to this PaintChunk.
    rectsWithColor.push_back(
        RectWithColor(FloatRect(50, 50, 100, 100), Color::black));

    const cc::Layer* layer = contentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(), Pointee(drawsRectangles(rectsWithColor)));
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, MergeTransformOrigin) {
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(TransformPaintPropertyNode::root(),
                                         TransformationMatrix().rotate(45),
                                         FloatPoint3D(100, 100, 0), false, 0);

  TestPaintArtifact testArtifact;
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
  testArtifact
      .chunk(transform.get(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::black);
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 200, 300), Color::gray);

  const PaintArtifact& artifact = testArtifact.build();

  ASSERT_EQ(3u, artifact.paintChunks().size());
  PaintArtifactCompositor::PendingLayer pendingLayer(artifact.paintChunks()[0]);
  EXPECT_TRUE(PaintArtifactCompositor::canMergeInto(
      artifact, artifact.paintChunks()[1], pendingLayer));
  pendingLayer.add(artifact.paintChunks()[1], nullptr);
  EXPECT_TRUE(PaintArtifactCompositor::canMergeInto(
      artifact, artifact.paintChunks()[2], pendingLayer));
  update(artifact);

  ASSERT_EQ(1u, contentLayerCount());
  {
    Vector<RectWithColor> rectsWithColor;
    rectsWithColor.push_back(
        RectWithColor(FloatRect(0, 42, 100, 100), Color::white));
    // Transform is applied to this PaintChunk.
    rectsWithColor.push_back(RectWithColor(
        FloatRect(29.2893, 0.578644, 141.421, 141.421), Color::black));
    rectsWithColor.push_back(
        RectWithColor(FloatRect(00, 42, 200, 300), Color::gray));

    const cc::Layer* layer = contentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(), Pointee(drawsRectangles(rectsWithColor)));
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, MergeOpacity) {
  float opacity = 2.0 / 255.0;
  RefPtr<EffectPaintPropertyNode> effect =
      createOpacityOnlyEffect(EffectPaintPropertyNode::root(), opacity);

  TestPaintArtifact testArtifact;
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             effect.get())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::black);
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 200, 300), Color::gray);

  const PaintArtifact& artifact = testArtifact.build();

  ASSERT_EQ(3u, artifact.paintChunks().size());
  PaintArtifactCompositor::PendingLayer pendingLayer(artifact.paintChunks()[0]);
  EXPECT_TRUE(PaintArtifactCompositor::canMergeInto(
      artifact, artifact.paintChunks()[1], pendingLayer));
  pendingLayer.add(artifact.paintChunks()[1], nullptr);
  EXPECT_TRUE(PaintArtifactCompositor::canMergeInto(
      artifact, artifact.paintChunks()[2], pendingLayer));

  update(artifact);

  ASSERT_EQ(1u, contentLayerCount());
  {
    Vector<RectWithColor> rectsWithColor;
    rectsWithColor.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::white));
    // Transform is applied to this PaintChunk.
    rectsWithColor.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100),
                      Color(Color::black).combineWithAlpha(opacity).rgb()));
    rectsWithColor.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::gray));

    const cc::Layer* layer = contentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(), Pointee(drawsRectangles(rectsWithColor)));
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, MergeNested) {
  // Tests merging of an opacity effect, inside of a clip, inside of a
  // transform.

  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(
          TransformPaintPropertyNode::root(),
          TransformationMatrix().translate(50, 50), FloatPoint3D(100, 100, 0),
          false, 0);

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), transform.get(),
      FloatRoundedRect(10, 20, 50, 60));

  float opacity = 2.0 / 255.0;
  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::create(
      EffectPaintPropertyNode::root(), transform.get(), clip.get(),
      ColorFilterNone, CompositorFilterOperations(), opacity,
      SkBlendMode::kSrcOver);

  TestPaintArtifact testArtifact;
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
  testArtifact.chunk(transform.get(), clip.get(), effect.get())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::black);
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 200, 300), Color::gray);

  const PaintArtifact& artifact = testArtifact.build();

  ASSERT_EQ(3u, artifact.paintChunks().size());
  PaintArtifactCompositor::PendingLayer pendingLayer(artifact.paintChunks()[0]);
  EXPECT_TRUE(PaintArtifactCompositor::canMergeInto(
      artifact, artifact.paintChunks()[1], pendingLayer));
  pendingLayer.add(artifact.paintChunks()[1], nullptr);
  EXPECT_TRUE(PaintArtifactCompositor::canMergeInto(
      artifact, artifact.paintChunks()[2], pendingLayer));
  update(artifact);

  ASSERT_EQ(1u, contentLayerCount());
  {
    Vector<RectWithColor> rectsWithColor;
    rectsWithColor.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::white));
    // Transform is applied to this PaintChunk.
    rectsWithColor.push_back(
        RectWithColor(FloatRect(60, 70, 50, 60),
                      Color(Color::black).combineWithAlpha(opacity).rgb()));
    rectsWithColor.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::gray));

    const cc::Layer* layer = contentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(), Pointee(drawsRectangles(rectsWithColor)));
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, ClipPushedUp) {
  // Tests merging of an element which has a clipapplied to it,
  // but has an ancestor transform of them. This can happen for fixed-
  // or absolute-position elements which escape scroll transforms.

  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(
          TransformPaintPropertyNode::root(),
          TransformationMatrix().translate(20, 25), FloatPoint3D(100, 100, 0),
          false, 0);

  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::create(
          transform.get(), TransformationMatrix().translate(20, 25),
          FloatPoint3D(100, 100, 0), false, 0);

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), transform2.get(),
      FloatRoundedRect(10, 20, 50, 60));

  TestPaintArtifact testArtifact;
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), clip.get(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 300, 400), Color::black);
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 200, 300), Color::gray);

  const PaintArtifact& artifact = testArtifact.build();

  ASSERT_EQ(3u, artifact.paintChunks().size());
  PaintArtifactCompositor::PendingLayer pendingLayer(artifact.paintChunks()[0]);
  EXPECT_TRUE(PaintArtifactCompositor::canMergeInto(
      artifact, artifact.paintChunks()[1], pendingLayer));
  pendingLayer.add(artifact.paintChunks()[1], nullptr);
  EXPECT_TRUE(PaintArtifactCompositor::canMergeInto(
      artifact, artifact.paintChunks()[2], pendingLayer));
  update(artifact);

  ASSERT_EQ(1u, contentLayerCount());
  {
    Vector<RectWithColor> rectsWithColor;
    rectsWithColor.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::white));
    // The two transforms (combined translation of (40, 50)) are applied here,
    // before clipping.
    rectsWithColor.push_back(
        RectWithColor(FloatRect(50, 70, 50, 60), Color(Color::black)));
    rectsWithColor.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::gray));

    const cc::Layer* layer = contentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(), Pointee(drawsRectangles(rectsWithColor)));
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, EffectPushedUp) {
  // Tests merging of an element which has an effect applied to it,
  // but has an ancestor transform of them. This can happen for fixed-
  // or absolute-position elements which escape scroll transforms.

  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(
          TransformPaintPropertyNode::root(),
          TransformationMatrix().translate(20, 25), FloatPoint3D(100, 100, 0),
          false, 0);

  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::create(
          transform.get(), TransformationMatrix().translate(20, 25),
          FloatPoint3D(100, 100, 0), false, 0);

  float opacity = 2.0 / 255.0;
  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::create(
      EffectPaintPropertyNode::root(), transform2.get(),
      ClipPaintPropertyNode::root(), ColorFilterNone,
      CompositorFilterOperations(), opacity, SkBlendMode::kSrcOver);

  TestPaintArtifact testArtifact;
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             effect.get())
      .rectDrawing(FloatRect(0, 0, 300, 400), Color::black);
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 200, 300), Color::gray);

  const PaintArtifact& artifact = testArtifact.build();

  ASSERT_EQ(3u, artifact.paintChunks().size());
  PaintArtifactCompositor::PendingLayer pendingLayer(artifact.paintChunks()[0]);
  EXPECT_TRUE(PaintArtifactCompositor::canMergeInto(
      artifact, artifact.paintChunks()[1], pendingLayer));
  pendingLayer.add(artifact.paintChunks()[1], nullptr);
  EXPECT_TRUE(PaintArtifactCompositor::canMergeInto(
      artifact, artifact.paintChunks()[2], pendingLayer));
  update(artifact);

  ASSERT_EQ(1u, contentLayerCount());
  {
    Vector<RectWithColor> rectsWithColor;
    rectsWithColor.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::white));
    rectsWithColor.push_back(
        RectWithColor(FloatRect(0, 0, 300, 400),
                      Color(Color::black).combineWithAlpha(opacity).rgb()));
    rectsWithColor.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::gray));

    const cc::Layer* layer = contentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(), Pointee(drawsRectangles(rectsWithColor)));
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, EffectAndClipPushedUp) {
  // Tests merging of an element which has an effect applied to it,
  // but has an ancestor transform of them. This can happen for fixed-
  // or absolute-position elements which escape scroll transforms.

  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(
          TransformPaintPropertyNode::root(),
          TransformationMatrix().translate(20, 25), FloatPoint3D(100, 100, 0),
          false, 0);

  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::create(
          transform.get(), TransformationMatrix().translate(20, 25),
          FloatPoint3D(100, 100, 0), false, 0);

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), transform.get(),
      FloatRoundedRect(10, 20, 50, 60));

  float opacity = 2.0 / 255.0;
  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::create(
      EffectPaintPropertyNode::root(), transform2.get(), clip.get(),
      ColorFilterNone, CompositorFilterOperations(), opacity,
      SkBlendMode::kSrcOver);

  TestPaintArtifact testArtifact;
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), clip.get(), effect.get())
      .rectDrawing(FloatRect(0, 0, 300, 400), Color::black);
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 200, 300), Color::gray);

  const PaintArtifact& artifact = testArtifact.build();

  ASSERT_EQ(3u, artifact.paintChunks().size());
  PaintArtifactCompositor::PendingLayer pendingLayer(artifact.paintChunks()[0]);
  EXPECT_TRUE(PaintArtifactCompositor::canMergeInto(
      artifact, artifact.paintChunks()[1], pendingLayer));
  pendingLayer.add(artifact.paintChunks()[1], nullptr);
  EXPECT_TRUE(PaintArtifactCompositor::canMergeInto(
      artifact, artifact.paintChunks()[2], pendingLayer));
  update(artifact);

  ASSERT_EQ(1u, contentLayerCount());
  {
    Vector<RectWithColor> rectsWithColor;
    rectsWithColor.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::white));
    // The clip is under |transform| but not |transform2|, so only an adjustment
    // of (20, 25) occurs.
    rectsWithColor.push_back(
        RectWithColor(FloatRect(30, 45, 50, 60),
                      Color(Color::black).combineWithAlpha(opacity).rgb()));
    rectsWithColor.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::gray));

    const cc::Layer* layer = contentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(), Pointee(drawsRectangles(rectsWithColor)));
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, ClipAndEffectNoTransform) {
  // Tests merging of an element which has a clip and effect in the root
  // transform space.

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      FloatRoundedRect(10, 20, 50, 60));

  float opacity = 2.0 / 255.0;
  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::create(
      EffectPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      clip.get(), ColorFilterNone, CompositorFilterOperations(), opacity,
      SkBlendMode::kSrcOver);

  TestPaintArtifact testArtifact;
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), clip.get(), effect.get())
      .rectDrawing(FloatRect(0, 0, 300, 400), Color::black);
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 200, 300), Color::gray);

  const PaintArtifact& artifact = testArtifact.build();

  ASSERT_EQ(3u, artifact.paintChunks().size());
  PaintArtifactCompositor::PendingLayer pendingLayer(artifact.paintChunks()[0]);
  EXPECT_TRUE(PaintArtifactCompositor::canMergeInto(
      artifact, artifact.paintChunks()[1], pendingLayer));
  pendingLayer.add(artifact.paintChunks()[1], nullptr);
  EXPECT_TRUE(PaintArtifactCompositor::canMergeInto(
      artifact, artifact.paintChunks()[2], pendingLayer));
  update(artifact);

  ASSERT_EQ(1u, contentLayerCount());
  {
    Vector<RectWithColor> rectsWithColor;
    rectsWithColor.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::white));
    rectsWithColor.push_back(
        RectWithColor(FloatRect(10, 20, 50, 60),
                      Color(Color::black).combineWithAlpha(opacity).rgb()));
    rectsWithColor.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::gray));

    const cc::Layer* layer = contentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(), Pointee(drawsRectangles(rectsWithColor)));
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, TwoClips) {
  // Tests merging of an element which has two clips in the root
  // transform space.

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      FloatRoundedRect(20, 30, 10, 20));

  RefPtr<ClipPaintPropertyNode> clip2 = ClipPaintPropertyNode::create(
      clip.get(), TransformPaintPropertyNode::root(),
      FloatRoundedRect(10, 20, 50, 60));

  TestPaintArtifact testArtifact;
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), clip2.get(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 300, 400), Color::black);
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 200, 300), Color::gray);

  const PaintArtifact& artifact = testArtifact.build();

  ASSERT_EQ(3u, artifact.paintChunks().size());
  PaintArtifactCompositor::PendingLayer pendingLayer(artifact.paintChunks()[0]);
  EXPECT_TRUE(PaintArtifactCompositor::canMergeInto(
      artifact, artifact.paintChunks()[1], pendingLayer));
  pendingLayer.add(artifact.paintChunks()[1], nullptr);
  EXPECT_TRUE(PaintArtifactCompositor::canMergeInto(
      artifact, artifact.paintChunks()[2], pendingLayer));
  update(artifact);

  ASSERT_EQ(1u, contentLayerCount());
  {
    Vector<RectWithColor> rectsWithColor;
    rectsWithColor.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::white));
    // The interesction of the two clips is (20, 30, 10, 20).
    rectsWithColor.push_back(
        RectWithColor(FloatRect(20, 30, 10, 20), Color(Color::black)));
    rectsWithColor.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::gray));

    const cc::Layer* layer = contentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(), Pointee(drawsRectangles(rectsWithColor)));
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, TwoTransformsClipBetween) {
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(
          TransformPaintPropertyNode::root(),
          TransformationMatrix().translate(20, 25), FloatPoint3D(100, 100, 0),
          false, 0);
  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      FloatRoundedRect(0, 0, 50, 60));
  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::create(
          transform.get(), TransformationMatrix().translate(20, 25),
          FloatPoint3D(100, 100, 0), false, 0);
  TestPaintArtifact testArtifact;
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
  testArtifact
      .chunk(transform2.get(), clip.get(), EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 300, 400), Color::black);
  testArtifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 200, 300), Color::gray);
  const PaintArtifact& artifact = testArtifact.build();
  ASSERT_EQ(3u, artifact.paintChunks().size());
  PaintArtifactCompositor::PendingLayer pendingLayer(artifact.paintChunks()[0]);
  EXPECT_TRUE(PaintArtifactCompositor::canMergeInto(
      artifact, artifact.paintChunks()[1], pendingLayer));
  pendingLayer.add(artifact.paintChunks()[1], nullptr);
  EXPECT_TRUE(PaintArtifactCompositor::canMergeInto(
      artifact, artifact.paintChunks()[2], pendingLayer));
  update(artifact);
  ASSERT_EQ(1u, contentLayerCount());
  {
    Vector<RectWithColor> rectsWithColor;
    rectsWithColor.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::white));
    rectsWithColor.push_back(
        RectWithColor(FloatRect(40, 50, 50, 60), Color(Color::black)));
    rectsWithColor.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::gray));
    const cc::Layer* layer = contentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(), Pointee(drawsRectangles(rectsWithColor)));
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, OverlapTransform) {
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(
          TransformPaintPropertyNode::root(),
          TransformationMatrix().translate(50, 50), FloatPoint3D(100, 100, 0),
          false, 0, CompositingReason3DTransform);

  TestPaintArtifact testArtifact;
  testArtifact.chunk(defaultPaintChunkProperties())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
  testArtifact
      .chunk(transform.get(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::black);
  testArtifact.chunk(defaultPaintChunkProperties())
      .rectDrawing(FloatRect(0, 0, 200, 300), Color::gray);

  const PaintArtifact& artifact = testArtifact.build();

  ASSERT_EQ(3u, artifact.paintChunks().size());
  PaintArtifactCompositor::PendingLayer pendingLayer(artifact.paintChunks()[0]);

  EXPECT_FALSE(PaintArtifactCompositor::canMergeInto(
      artifact, artifact.paintChunks()[1], pendingLayer));

  PaintArtifactCompositor::PendingLayer pendingLayer2(
      artifact.paintChunks()[1]);
  EXPECT_FALSE(PaintArtifactCompositor::canMergeInto(
      artifact, artifact.paintChunks()[2], pendingLayer2));

  std::unique_ptr<GeometryMapper> geometryMapper = GeometryMapper::create();
  EXPECT_TRUE(PaintArtifactCompositor::mightOverlap(
      artifact.paintChunks()[2], pendingLayer2, *geometryMapper));

  update(artifact);

  // The third paint chunk overlaps the second but can't merge due to
  // incompatible transform. The second paint chunk can't merge into the first
  // due to a direct compositing reason.
  ASSERT_EQ(3u, contentLayerCount());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, MightOverlap) {
  PaintChunk paintChunk;
  paintChunk.properties = defaultPaintChunkProperties();
  paintChunk.bounds = FloatRect(0, 0, 100, 100);

  PaintChunk paintChunk2;
  paintChunk2.properties = defaultPaintChunkProperties();
  paintChunk2.bounds = FloatRect(0, 0, 100, 100);

  std::unique_ptr<GeometryMapper> geometryMapper = GeometryMapper::create();
  PaintArtifactCompositor::PendingLayer pendingLayer(paintChunk);
  EXPECT_TRUE(PaintArtifactCompositor::mightOverlap(paintChunk2, pendingLayer,
                                                    *geometryMapper));

  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(
          TransformPaintPropertyNode::root(),
          TransformationMatrix().translate(99, 0), FloatPoint3D(100, 100, 0),
          false);

  paintChunk2.properties.propertyTreeState.setTransform(transform.get());
  EXPECT_TRUE(PaintArtifactCompositor::mightOverlap(paintChunk2, pendingLayer,
                                                    *geometryMapper));

  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::create(
          TransformPaintPropertyNode::root(),
          TransformationMatrix().translate(100, 0), FloatPoint3D(100, 100, 0),
          false);
  paintChunk2.properties.propertyTreeState.setTransform(transform2.get());

  EXPECT_FALSE(PaintArtifactCompositor::mightOverlap(paintChunk2, pendingLayer,
                                                     *geometryMapper));
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, PendingLayer) {
  PaintChunk chunk1;
  chunk1.properties.propertyTreeState = PropertyTreeState(
      TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
      EffectPaintPropertyNode::root());
  chunk1.properties.backfaceHidden = true;
  chunk1.knownToBeOpaque = true;
  chunk1.bounds = FloatRect(0, 0, 30, 40);

  PaintArtifactCompositor::PendingLayer pendingLayer(chunk1);

  EXPECT_TRUE(pendingLayer.backfaceHidden);
  EXPECT_TRUE(pendingLayer.knownToBeOpaque);
  EXPECT_BLINK_FLOAT_RECT_EQ(FloatRect(0, 0, 30, 40), pendingLayer.bounds);

  PaintChunk chunk2;
  chunk2.properties.propertyTreeState = chunk1.properties.propertyTreeState;
  chunk2.properties.backfaceHidden = true;
  chunk2.knownToBeOpaque = true;
  chunk2.bounds = FloatRect(10, 20, 30, 40);
  pendingLayer.add(chunk2, nullptr);

  EXPECT_TRUE(pendingLayer.backfaceHidden);
  // Bounds not equal to one PaintChunk.
  EXPECT_FALSE(pendingLayer.knownToBeOpaque);
  EXPECT_BLINK_FLOAT_RECT_EQ(FloatRect(0, 0, 40, 60), pendingLayer.bounds);

  PaintChunk chunk3;
  chunk3.properties.propertyTreeState = chunk1.properties.propertyTreeState;
  chunk3.properties.backfaceHidden = true;
  chunk3.knownToBeOpaque = true;
  chunk3.bounds = FloatRect(-5, -25, 20, 20);
  pendingLayer.add(chunk3, nullptr);

  EXPECT_TRUE(pendingLayer.backfaceHidden);
  EXPECT_FALSE(pendingLayer.knownToBeOpaque);
  EXPECT_BLINK_FLOAT_RECT_EQ(FloatRect(-5, -25, 45, 85), pendingLayer.bounds);
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, PendingLayerWithGeometry) {
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(
          TransformPaintPropertyNode::root(),
          TransformationMatrix().translate(20, 25), FloatPoint3D(100, 100, 0),
          false, 0);

  PaintChunk chunk1;
  chunk1.properties.propertyTreeState = PropertyTreeState(
      TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
      EffectPaintPropertyNode::root());
  chunk1.bounds = FloatRect(0, 0, 30, 40);

  PaintArtifactCompositor::PendingLayer pendingLayer(chunk1);

  EXPECT_BLINK_FLOAT_RECT_EQ(FloatRect(0, 0, 30, 40), pendingLayer.bounds);

  PaintChunk chunk2;
  chunk2.properties.propertyTreeState = chunk1.properties.propertyTreeState;
  chunk2.properties.propertyTreeState.setTransform(transform);
  chunk2.bounds = FloatRect(0, 0, 50, 60);
  std::unique_ptr<GeometryMapper> geometryMapper = GeometryMapper::create();
  pendingLayer.add(chunk2, geometryMapper.get());

  EXPECT_BLINK_FLOAT_RECT_EQ(FloatRect(0, 0, 70, 85), pendingLayer.bounds);
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, PendingLayerKnownOpaque) {
  PaintChunk chunk1;
  chunk1.properties.propertyTreeState = PropertyTreeState(
      TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
      EffectPaintPropertyNode::root());
  chunk1.bounds = FloatRect(0, 0, 30, 40);
  chunk1.knownToBeOpaque = false;
  PaintArtifactCompositor::PendingLayer pendingLayer(chunk1);

  EXPECT_FALSE(pendingLayer.knownToBeOpaque);

  PaintChunk chunk2;
  chunk2.properties.propertyTreeState = chunk1.properties.propertyTreeState;
  chunk2.bounds = FloatRect(0, 0, 25, 35);
  chunk2.knownToBeOpaque = true;
  pendingLayer.add(chunk2, nullptr);

  // Chunk 2 doesn't cover the entire layer, so not opaque.
  EXPECT_FALSE(pendingLayer.knownToBeOpaque);

  PaintChunk chunk3;
  chunk3.properties.propertyTreeState = chunk1.properties.propertyTreeState;
  chunk3.bounds = FloatRect(0, 0, 50, 60);
  chunk3.knownToBeOpaque = true;
  pendingLayer.add(chunk3, nullptr);

  // Chunk 3 covers the entire layer, so now it's opaque.
  EXPECT_TRUE(pendingLayer.knownToBeOpaque);
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, TransformWithElementId) {
  CompositorElementId expectedCompositorElementId(2, 0);
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(
          TransformPaintPropertyNode::root(), TransformationMatrix().rotate(90),
          FloatPoint3D(100, 100, 0), false, 0, CompositingReason3DTransform,
          expectedCompositorElementId);

  TestPaintArtifact artifact;
  artifact
      .chunk(transform, ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(100, 100, 200, 100), Color::black);
  update(artifact.build());

  EXPECT_EQ(2, elementIdToTransformNodeIndex(expectedCompositorElementId));
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, EffectWithElementId) {
  CompositorElementId expectedCompositorElementId(2, 0);
  float opacity = 2.0 / 255.0;
  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::create(
      EffectPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      ClipPaintPropertyNode::root(), ColorFilterNone,
      CompositorFilterOperations(), opacity, SkBlendMode::kSrcOver,
      CompositingReasonNone, expectedCompositorElementId);

  TestPaintArtifact artifact;
  artifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             effect.get())
      .rectDrawing(FloatRect(100, 100, 200, 100), Color::black);
  update(artifact.build());

  EXPECT_EQ(2, elementIdToEffectNodeIndex(expectedCompositorElementId));
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, CompositedLuminanceMask) {
  RefPtr<EffectPaintPropertyNode> masked = EffectPaintPropertyNode::create(
      EffectPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      ClipPaintPropertyNode::root(), ColorFilterNone,
      CompositorFilterOperations(), 1.0, SkBlendMode::kSrcOver,
      CompositingReasonIsolateCompositedDescendants);
  RefPtr<EffectPaintPropertyNode> masking = EffectPaintPropertyNode::create(
      masked, TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
      ColorFilterLuminanceToAlpha, CompositorFilterOperations(), 1.0,
      SkBlendMode::kDstIn, CompositingReasonSquashingDisallowed);

  TestPaintArtifact artifact;
  artifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             masked.get())
      .rectDrawing(FloatRect(100, 100, 200, 200), Color::gray);
  artifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             masking.get())
      .rectDrawing(FloatRect(150, 150, 100, 100), Color::white);
  update(artifact.build());

  ASSERT_EQ(2u, contentLayerCount());

  const cc::Layer* maskedLayer = contentLayerAt(0);
  EXPECT_THAT(maskedLayer->GetPicture(),
              Pointee(drawsRectangle(FloatRect(0, 0, 200, 200), Color::gray)));
  EXPECT_EQ(translation(100, 100), maskedLayer->screen_space_transform());
  EXPECT_EQ(gfx::Size(200, 200), maskedLayer->bounds());
  const cc::EffectNode* maskedGroup =
      propertyTrees().effect_tree.Node(maskedLayer->effect_tree_index());
  EXPECT_TRUE(maskedGroup->has_render_surface);

  const cc::Layer* maskingLayer = contentLayerAt(1);
  EXPECT_THAT(maskingLayer->GetPicture(),
              Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::white)));
  EXPECT_EQ(translation(150, 150), maskingLayer->screen_space_transform());
  EXPECT_EQ(gfx::Size(100, 100), maskingLayer->bounds());
  const cc::EffectNode* maskingGroup =
      propertyTrees().effect_tree.Node(maskingLayer->effect_tree_index());
  EXPECT_TRUE(maskingGroup->has_render_surface);
  EXPECT_EQ(maskedGroup->id, maskingGroup->parent_id);
  ASSERT_EQ(1u, maskingGroup->filters.size());
  EXPECT_EQ(cc::FilterOperation::REFERENCE, maskingGroup->filters.at(0).type());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees,
       UpdateProducesNewSequenceNumber) {
  // A 90 degree clockwise rotation about (100, 100).
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(
          TransformPaintPropertyNode::root(), TransformationMatrix().rotate(90),
          FloatPoint3D(100, 100, 0), false, 0, CompositingReason3DTransform);

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      FloatRoundedRect(100, 100, 300, 200));

  RefPtr<EffectPaintPropertyNode> effect =
      createOpacityOnlyEffect(EffectPaintPropertyNode::root(), 0.5);

  TestPaintArtifact artifact;
  artifact.chunk(transform, clip, effect)
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
  artifact
      .chunk(TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
             EffectPaintPropertyNode::root())
      .rectDrawing(FloatRect(0, 0, 100, 100), Color::gray);
  update(artifact.build());

  // Two content layers for the differentiated rect drawings and three dummy
  // layers for each of the transform, clip and effect nodes.
  EXPECT_EQ(5u, rootLayer()->children().size());
  EXPECT_EQ(1, propertyTrees().sequence_number);
  for (auto layer : rootLayer()->children()) {
    EXPECT_EQ(1, layer->property_tree_sequence_number());
  }

  update(artifact.build());

  EXPECT_EQ(5u, rootLayer()->children().size());
  EXPECT_EQ(2, propertyTrees().sequence_number);
  for (auto layer : rootLayer()->children()) {
    EXPECT_EQ(2, layer->property_tree_sequence_number());
  }

  update(artifact.build());

  EXPECT_EQ(5u, rootLayer()->children().size());
  EXPECT_EQ(3, propertyTrees().sequence_number);
  for (auto layer : rootLayer()->children()) {
    EXPECT_EQ(3, layer->property_tree_sequence_number());
  }
}

}  // namespace blink
