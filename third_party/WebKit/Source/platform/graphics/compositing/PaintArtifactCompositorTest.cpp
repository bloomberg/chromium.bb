// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/compositing/PaintArtifactCompositor.h"

#include <memory>

#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/layers/layer.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/PaintArtifact.h"
#include "platform/graphics/paint/ScrollPaintPropertyNode.h"
#include "platform/testing/FakeDisplayItemClient.h"
#include "platform/testing/PaintPropertyTestHelpers.h"
#include "platform/testing/PictureMatchers.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/TestPaintArtifact.h"
#include "platform/testing/WebLayerTreeViewImplForTesting.h"
#include "public/platform/WebLayerScrollClient.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using ::blink::testing::CreateOpacityOnlyEffect;
using ::testing::Pointee;

PaintChunk::Id DefaultId() {
  DEFINE_STATIC_LOCAL(FakeDisplayItemClient, fake_client, ());
  return PaintChunk::Id(fake_client, DisplayItem::kDrawingFirst);
}

PaintChunkProperties DefaultPaintChunkProperties() {
  PropertyTreeState property_tree_state(TransformPaintPropertyNode::Root(),
                                        ClipPaintPropertyNode::Root(),
                                        EffectPaintPropertyNode::Root());
  return PaintChunkProperties(property_tree_state);
}

PaintChunk DefaultChunk() {
  return PaintChunk(0, 1, DefaultId(), DefaultPaintChunkProperties());
}

gfx::Transform Translation(SkMScalar x, SkMScalar y) {
  gfx::Transform transform;
  transform.Translate(x, y);
  return transform;
}

class WebLayerTreeViewWithLayerTreeFrameSink
    : public WebLayerTreeViewImplForTesting {
 public:
  WebLayerTreeViewWithLayerTreeFrameSink(const cc::LayerTreeSettings& settings)
      : WebLayerTreeViewImplForTesting(settings) {}

  // cc::LayerTreeHostClient
  void RequestNewLayerTreeFrameSink() override {
    GetLayerTreeHost()->SetLayerTreeFrameSink(
        cc::FakeLayerTreeFrameSink::Create3d());
  }
};

class PaintArtifactCompositorTestWithPropertyTrees
    : public ::testing::Test,
      private ScopedSlimmingPaintV2ForTest {
 protected:
  PaintArtifactCompositorTestWithPropertyTrees()
      : ScopedSlimmingPaintV2ForTest(true),
        task_runner_(new base::TestSimpleTaskRunner),
        task_runner_handle_(task_runner_) {}

  void SetUp() override {
    // Delay constructing the compositor until after the feature is set.
    paint_artifact_compositor_ = PaintArtifactCompositor::Create();
    paint_artifact_compositor_->EnableExtraDataForTesting();

    cc::LayerTreeSettings settings =
        WebLayerTreeViewImplForTesting::DefaultLayerTreeSettings();
    settings.single_thread_proxy_scheduler = false;
    settings.use_layer_lists = true;
    web_layer_tree_view_ =
        WTF::MakeUnique<WebLayerTreeViewWithLayerTreeFrameSink>(settings);
    web_layer_tree_view_->SetRootLayer(
        *paint_artifact_compositor_->GetWebLayer());
  }

  const cc::PropertyTrees& GetPropertyTrees() {
    return *web_layer_tree_view_->GetLayerTreeHost()->property_trees();
  }

  const cc::LayerTreeHost& GetLayerTreeHost() {
    return *web_layer_tree_view_->GetLayerTreeHost();
  }

  int ElementIdToEffectNodeIndex(CompositorElementId element_id) {
    return web_layer_tree_view_->GetLayerTreeHost()
        ->property_trees()
        ->element_id_to_effect_node_index[element_id];
  }

  int ElementIdToTransformNodeIndex(CompositorElementId element_id) {
    return web_layer_tree_view_->GetLayerTreeHost()
        ->property_trees()
        ->element_id_to_transform_node_index[element_id];
  }

  int ElementIdToScrollNodeIndex(CompositorElementId element_id) {
    return web_layer_tree_view_->GetLayerTreeHost()
        ->property_trees()
        ->element_id_to_scroll_node_index[element_id];
  }

  const cc::TransformNode& GetTransformNode(const cc::Layer* layer) {
    return *GetPropertyTrees().transform_tree.Node(
        layer->transform_tree_index());
  }

  void Update(const PaintArtifact& artifact) {
    CompositorElementIdSet element_ids;
    Update(artifact, element_ids);
  }

  void Update(const PaintArtifact& artifact,
              CompositorElementIdSet& element_ids) {
    paint_artifact_compositor_->Update(artifact, element_ids);
    web_layer_tree_view_->GetLayerTreeHost()->LayoutAndUpdateLayers();
  }

  cc::Layer* RootLayer() { return paint_artifact_compositor_->RootLayer(); }

  size_t ContentLayerCount() {
    return paint_artifact_compositor_->GetExtraDataForTesting()
        ->content_layers.size();
  }

  cc::Layer* ContentLayerAt(unsigned index) {
    return paint_artifact_compositor_->GetExtraDataForTesting()
        ->content_layers[index]
        .get();
  }

  CompositorElementId ScrollElementId(unsigned id) {
    return CompositorElementIdFromLayoutObjectId(
        id, CompositorElementIdNamespace::kScroll);
  }

  void AddSimpleRectChunk(TestPaintArtifact& artifact) {
    artifact
        .Chunk(TransformPaintPropertyNode::Root(),
               ClipPaintPropertyNode::Root(), EffectPaintPropertyNode::Root())
        .RectDrawing(FloatRect(100, 100, 200, 100), Color::kBlack);
  }

  void CreateSimpleArtifactWithOpacity(TestPaintArtifact& artifact,
                                       float opacity,
                                       bool include_preceding_chunk,
                                       bool include_subsequent_chunk) {
    if (include_preceding_chunk)
      AddSimpleRectChunk(artifact);
    RefPtr<EffectPaintPropertyNode> effect =
        CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), opacity);
    artifact
        .Chunk(TransformPaintPropertyNode::Root(),
               ClipPaintPropertyNode::Root(), effect)
        .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
    if (include_subsequent_chunk)
      AddSimpleRectChunk(artifact);
    Update(artifact.Build());
  }

  using PendingLayer = PaintArtifactCompositor::PendingLayer;

  bool MightOverlap(const PendingLayer& a, const PendingLayer& b) {
    return PaintArtifactCompositor::MightOverlap(a, b);
  }

 private:
  std::unique_ptr<PaintArtifactCompositor> paint_artifact_compositor_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  std::unique_ptr<WebLayerTreeViewWithLayerTreeFrameSink> web_layer_tree_view_;
};

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, EmptyPaintArtifact) {
  PaintArtifact empty_artifact;
  Update(empty_artifact);
  EXPECT_TRUE(RootLayer()->children().empty());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, OneChunkWithAnOffset) {
  TestPaintArtifact artifact;
  artifact.Chunk(DefaultPaintChunkProperties())
      .RectDrawing(FloatRect(50, -50, 100, 100), Color::kWhite);
  Update(artifact.Build());

  ASSERT_EQ(1u, ContentLayerCount());
  const cc::Layer* child = ContentLayerAt(0);
  EXPECT_THAT(
      child->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 100, 100), Color::kWhite)));
  EXPECT_EQ(Translation(50, -50), child->ScreenSpaceTransform());
  EXPECT_EQ(gfx::Size(100, 100), child->bounds());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, OneTransform) {
  // A 90 degree clockwise rotation about (100, 100).
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(
          TransformPaintPropertyNode::Root(), TransformationMatrix().Rotate(90),
          FloatPoint3D(100, 100, 0), false, 0, kCompositingReason3DTransform);

  TestPaintArtifact artifact;
  artifact
      .Chunk(transform, ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kGray);
  artifact
      .Chunk(transform, ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(100, 100, 200, 100), Color::kBlack);
  Update(artifact.Build());

  ASSERT_EQ(2u, ContentLayerCount());
  {
    const cc::Layer* layer = ContentLayerAt(0);

    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::kWhite));
    rects_with_color.push_back(
        RectWithColor(FloatRect(100, 100, 200, 100), Color::kBlack));

    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
    gfx::RectF mapped_rect(0, 0, 100, 100);
    layer->ScreenSpaceTransform().TransformRect(&mapped_rect);
    EXPECT_EQ(gfx::RectF(100, 0, 100, 100), mapped_rect);
  }
  {
    const cc::Layer* layer = ContentLayerAt(1);
    EXPECT_THAT(
        layer->GetPicture(),
        Pointee(DrawsRectangle(FloatRect(0, 0, 100, 100), Color::kGray)));
    EXPECT_EQ(gfx::Transform(), layer->ScreenSpaceTransform());
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, TransformCombining) {
  // A translation by (5, 5) within a 2x scale about (10, 10).
  RefPtr<TransformPaintPropertyNode> transform1 =
      TransformPaintPropertyNode::Create(
          TransformPaintPropertyNode::Root(), TransformationMatrix().Scale(2),
          FloatPoint3D(10, 10, 0), false, 0, kCompositingReason3DTransform);
  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::Create(
          transform1, TransformationMatrix().Translate(5, 5), FloatPoint3D(),
          false, 0, kCompositingReason3DTransform);

  TestPaintArtifact artifact;
  artifact
      .Chunk(transform1, ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 300, 200), Color::kWhite);
  artifact
      .Chunk(transform2, ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 300, 200), Color::kBlack);
  Update(artifact.Build());

  ASSERT_EQ(2u, ContentLayerCount());
  {
    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(
        layer->GetPicture(),
        Pointee(DrawsRectangle(FloatRect(0, 0, 300, 200), Color::kWhite)));
    gfx::RectF mapped_rect(0, 0, 300, 200);
    layer->ScreenSpaceTransform().TransformRect(&mapped_rect);
    EXPECT_EQ(gfx::RectF(-10, -10, 600, 400), mapped_rect);
  }
  {
    const cc::Layer* layer = ContentLayerAt(1);
    EXPECT_THAT(
        layer->GetPicture(),
        Pointee(DrawsRectangle(FloatRect(0, 0, 300, 200), Color::kBlack)));
    gfx::RectF mapped_rect(0, 0, 300, 200);
    layer->ScreenSpaceTransform().TransformRect(&mapped_rect);
    EXPECT_EQ(gfx::RectF(0, 0, 600, 400), mapped_rect);
  }
  EXPECT_NE(ContentLayerAt(0)->transform_tree_index(),
            ContentLayerAt(1)->transform_tree_index());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees,
       FlattensInheritedTransform) {
  for (bool transform_is_flattened : {true, false}) {
    SCOPED_TRACE(transform_is_flattened);

    // The flattens_inherited_transform bit corresponds to whether the _parent_
    // transform node flattens the transform. This is because Blink's notion of
    // flattening determines whether content within the node's local transform
    // is flattened, while cc's notion applies in the parent's coordinate space.
    RefPtr<TransformPaintPropertyNode> transform1 =
        TransformPaintPropertyNode::Create(TransformPaintPropertyNode::Root(),
                                           TransformationMatrix(),
                                           FloatPoint3D());
    RefPtr<TransformPaintPropertyNode> transform2 =
        TransformPaintPropertyNode::Create(
            transform1, TransformationMatrix().Rotate3d(0, 45, 0),
            FloatPoint3D());
    RefPtr<TransformPaintPropertyNode> transform3 =
        TransformPaintPropertyNode::Create(
            transform2, TransformationMatrix().Rotate3d(0, 45, 0),
            FloatPoint3D(), transform_is_flattened);

    TestPaintArtifact artifact;
    artifact
        .Chunk(transform3, ClipPaintPropertyNode::Root(),
               EffectPaintPropertyNode::Root())
        .RectDrawing(FloatRect(0, 0, 300, 200), Color::kWhite);
    Update(artifact.Build());

    ASSERT_EQ(1u, ContentLayerCount());
    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(
        layer->GetPicture(),
        Pointee(DrawsRectangle(FloatRect(0, 0, 300, 200), Color::kWhite)));

    // The leaf transform node should flatten its inherited transform node
    // if and only if the intermediate rotation transform in the Blink tree
    // flattens.
    const cc::TransformNode* transform_node3 =
        GetPropertyTrees().transform_tree.Node(layer->transform_tree_index());
    EXPECT_EQ(transform_is_flattened,
              transform_node3->flattens_inherited_transform);

    // Given this, we should expect the correct screen space transform for
    // each case. If the transform was flattened, we should see it getting
    // an effective horizontal scale of 1/sqrt(2) each time, thus it gets
    // half as wide. If the transform was not flattened, we should see an
    // empty rectangle (as the total 90 degree rotation makes it
    // perpendicular to the viewport).
    gfx::RectF rect(0, 0, 100, 100);
    layer->ScreenSpaceTransform().TransformRect(&rect);
    if (transform_is_flattened)
      EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 50, 100), rect);
    else
      EXPECT_TRUE(rect.IsEmpty());
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, SortingContextID) {
  // Has no 3D rendering context.
  RefPtr<TransformPaintPropertyNode> transform1 =
      TransformPaintPropertyNode::Create(TransformPaintPropertyNode::Root(),
                                         TransformationMatrix(),
                                         FloatPoint3D());
  // Establishes a 3D rendering context.
  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::Create(transform1, TransformationMatrix(),
                                         FloatPoint3D(), false, 1,
                                         kCompositingReason3DTransform);
  // Extends the 3D rendering context of transform2.
  RefPtr<TransformPaintPropertyNode> transform3 =
      TransformPaintPropertyNode::Create(transform2, TransformationMatrix(),
                                         FloatPoint3D(), false, 1,
                                         kCompositingReason3DTransform);
  // Establishes a 3D rendering context distinct from transform2.
  RefPtr<TransformPaintPropertyNode> transform4 =
      TransformPaintPropertyNode::Create(transform2, TransformationMatrix(),
                                         FloatPoint3D(), false, 2,
                                         kCompositingReason3DTransform);

  TestPaintArtifact artifact;
  artifact
      .Chunk(transform1, ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 300, 200), Color::kWhite);
  artifact
      .Chunk(transform2, ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 300, 200), Color::kLightGray);
  artifact
      .Chunk(transform3, ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 300, 200), Color::kDarkGray);
  artifact
      .Chunk(transform4, ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 300, 200), Color::kBlack);
  Update(artifact.Build());

  ASSERT_EQ(4u, ContentLayerCount());

  // The white layer is not 3D sorted.
  const cc::Layer* white_layer = ContentLayerAt(0);
  EXPECT_THAT(
      white_layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 300, 200), Color::kWhite)));
  int white_sorting_context_id =
      GetTransformNode(white_layer).sorting_context_id;
  EXPECT_EQ(white_layer->sorting_context_id(), white_sorting_context_id);
  EXPECT_EQ(0, white_sorting_context_id);

  // The light gray layer is 3D sorted.
  const cc::Layer* light_gray_layer = ContentLayerAt(1);
  EXPECT_THAT(
      light_gray_layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 300, 200), Color::kLightGray)));
  int light_gray_sorting_context_id =
      GetTransformNode(light_gray_layer).sorting_context_id;
  EXPECT_NE(0, light_gray_sorting_context_id);

  // The dark gray layer is 3D sorted with the light gray layer, but has a
  // separate transform node.
  const cc::Layer* dark_gray_layer = ContentLayerAt(2);
  EXPECT_THAT(
      dark_gray_layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 300, 200), Color::kDarkGray)));
  int dark_gray_sorting_context_id =
      GetTransformNode(dark_gray_layer).sorting_context_id;
  EXPECT_EQ(light_gray_sorting_context_id, dark_gray_sorting_context_id);
  EXPECT_NE(light_gray_layer->transform_tree_index(),
            dark_gray_layer->transform_tree_index());

  // The black layer is 3D sorted, but in a separate context from the previous
  // layers.
  const cc::Layer* black_layer = ContentLayerAt(3);
  EXPECT_THAT(
      black_layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 300, 200), Color::kBlack)));
  int black_sorting_context_id =
      GetTransformNode(black_layer).sorting_context_id;
  EXPECT_NE(0, black_sorting_context_id);
  EXPECT_NE(light_gray_sorting_context_id, black_sorting_context_id);
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, OneClip) {
  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      FloatRoundedRect(100, 100, 300, 200));

  TestPaintArtifact artifact;
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), clip,
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(220, 80, 300, 200), Color::kBlack);
  Update(artifact.Build());

  ASSERT_EQ(1u, ContentLayerCount());
  const cc::Layer* layer = ContentLayerAt(0);
  EXPECT_THAT(
      layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 300, 200), Color::kBlack)));
  EXPECT_EQ(Translation(220, 80), layer->ScreenSpaceTransform());

  const cc::ClipNode* clip_node =
      GetPropertyTrees().clip_tree.Node(layer->clip_tree_index());
  EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP, clip_node->clip_type);
  EXPECT_EQ(gfx::RectF(100, 100, 300, 200), clip_node->clip);
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, NestedClips) {
  RefPtr<ClipPaintPropertyNode> clip1 = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      FloatRoundedRect(100, 100, 700, 700),
      kCompositingReasonOverflowScrollingTouch);
  RefPtr<ClipPaintPropertyNode> clip2 =
      ClipPaintPropertyNode::Create(clip1, TransformPaintPropertyNode::Root(),
                                    FloatRoundedRect(200, 200, 700, 700),
                                    kCompositingReasonOverflowScrollingTouch);

  TestPaintArtifact artifact;
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), clip1,
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(300, 350, 100, 100), Color::kWhite);
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), clip2,
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(300, 350, 100, 100), Color::kLightGray);
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), clip1,
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(300, 350, 100, 100), Color::kDarkGray);
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), clip2,
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(300, 350, 100, 100), Color::kBlack);
  Update(artifact.Build());

  ASSERT_EQ(4u, ContentLayerCount());

  const cc::Layer* white_layer = ContentLayerAt(0);
  EXPECT_THAT(
      white_layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 100, 100), Color::kWhite)));
  EXPECT_EQ(Translation(300, 350), white_layer->ScreenSpaceTransform());

  const cc::Layer* light_gray_layer = ContentLayerAt(1);
  EXPECT_THAT(
      light_gray_layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 100, 100), Color::kLightGray)));
  EXPECT_EQ(Translation(300, 350), light_gray_layer->ScreenSpaceTransform());

  const cc::Layer* dark_gray_layer = ContentLayerAt(2);
  EXPECT_THAT(
      dark_gray_layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 100, 100), Color::kDarkGray)));
  EXPECT_EQ(Translation(300, 350), dark_gray_layer->ScreenSpaceTransform());

  const cc::Layer* black_layer = ContentLayerAt(3);
  EXPECT_THAT(
      black_layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 100, 100), Color::kBlack)));
  EXPECT_EQ(Translation(300, 350), black_layer->ScreenSpaceTransform());

  EXPECT_EQ(white_layer->clip_tree_index(), dark_gray_layer->clip_tree_index());
  const cc::ClipNode* outer_clip =
      GetPropertyTrees().clip_tree.Node(white_layer->clip_tree_index());
  EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP, outer_clip->clip_type);
  EXPECT_EQ(gfx::RectF(100, 100, 700, 700), outer_clip->clip);

  EXPECT_EQ(light_gray_layer->clip_tree_index(),
            black_layer->clip_tree_index());
  const cc::ClipNode* inner_clip =
      GetPropertyTrees().clip_tree.Node(black_layer->clip_tree_index());
  EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP, inner_clip->clip_type);
  EXPECT_EQ(gfx::RectF(200, 200, 700, 700), inner_clip->clip);
  EXPECT_EQ(outer_clip->id, inner_clip->parent_id);
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, DeeplyNestedClips) {
  Vector<RefPtr<ClipPaintPropertyNode>> clips;
  for (unsigned i = 1; i <= 10; i++) {
    clips.push_back(ClipPaintPropertyNode::Create(
        clips.IsEmpty() ? ClipPaintPropertyNode::Root() : clips.back(),
        TransformPaintPropertyNode::Root(),
        FloatRoundedRect(5 * i, 0, 100, 200 - 10 * i)));
  }

  TestPaintArtifact artifact;
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), clips.back(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 200, 200), Color::kWhite);
  Update(artifact.Build());

  // Check the drawing layer.
  ASSERT_EQ(1u, ContentLayerCount());
  const cc::Layer* drawing_layer = ContentLayerAt(0);
  EXPECT_THAT(
      drawing_layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 200, 200), Color::kWhite)));
  EXPECT_EQ(gfx::Transform(), drawing_layer->ScreenSpaceTransform());

  // Check the clip nodes.
  const cc::ClipNode* clip_node =
      GetPropertyTrees().clip_tree.Node(drawing_layer->clip_tree_index());
  for (auto it = clips.rbegin(); it != clips.rend(); ++it) {
    const ClipPaintPropertyNode* paint_clip_node = it->Get();
    EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP, clip_node->clip_type);
    EXPECT_EQ(paint_clip_node->ClipRect().Rect(), clip_node->clip);
    clip_node = GetPropertyTrees().clip_tree.Node(clip_node->parent_id);
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, SiblingClips) {
  RefPtr<ClipPaintPropertyNode> common_clip = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      FloatRoundedRect(0, 0, 800, 600));
  RefPtr<ClipPaintPropertyNode> clip1 = ClipPaintPropertyNode::Create(
      common_clip, TransformPaintPropertyNode::Root(),
      FloatRoundedRect(0, 0, 400, 600));
  RefPtr<ClipPaintPropertyNode> clip2 = ClipPaintPropertyNode::Create(
      common_clip, TransformPaintPropertyNode::Root(),
      FloatRoundedRect(400, 0, 400, 600));

  TestPaintArtifact artifact;
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), clip1,
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 640, 480), Color::kWhite);
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), clip2,
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 640, 480), Color::kBlack);
  Update(artifact.Build());
  ASSERT_EQ(2u, ContentLayerCount());

  const cc::Layer* white_layer = ContentLayerAt(0);
  EXPECT_THAT(
      white_layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 640, 480), Color::kWhite)));
  EXPECT_EQ(gfx::Transform(), white_layer->ScreenSpaceTransform());
  const cc::ClipNode* white_clip =
      GetPropertyTrees().clip_tree.Node(white_layer->clip_tree_index());
  EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP, white_clip->clip_type);
  ASSERT_EQ(gfx::RectF(0, 0, 400, 600), white_clip->clip);

  const cc::Layer* black_layer = ContentLayerAt(1);
  EXPECT_THAT(
      black_layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 640, 480), Color::kBlack)));
  EXPECT_EQ(gfx::Transform(), black_layer->ScreenSpaceTransform());
  const cc::ClipNode* black_clip =
      GetPropertyTrees().clip_tree.Node(black_layer->clip_tree_index());
  EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP, black_clip->clip_type);
  ASSERT_EQ(gfx::RectF(400, 0, 400, 600), black_clip->clip);

  EXPECT_EQ(white_clip->parent_id, black_clip->parent_id);
  const cc::ClipNode* common_clip_node =
      GetPropertyTrees().clip_tree.Node(white_clip->parent_id);
  EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP,
            common_clip_node->clip_type);
  ASSERT_EQ(gfx::RectF(0, 0, 800, 600), common_clip_node->clip);
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees,
       ForeignLayerPassesThrough) {
  scoped_refptr<cc::Layer> layer = cc::Layer::Create();

  TestPaintArtifact test_artifact;
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk(DefaultPaintChunkProperties())
      .ForeignLayer(FloatPoint(50, 60), IntSize(400, 300), layer);
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kGray);

  const PaintArtifact& artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact.PaintChunks().size());
  Update(artifact);

  ASSERT_EQ(3u, ContentLayerCount());
  EXPECT_EQ(layer, ContentLayerAt(1));
  EXPECT_EQ(gfx::Size(400, 300), layer->bounds());
  EXPECT_EQ(Translation(50, 60), layer->ScreenSpaceTransform());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, EffectTreeConversion) {
  RefPtr<EffectPaintPropertyNode> effect1 = EffectPaintPropertyNode::Create(
      EffectPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      ClipPaintPropertyNode::Root(), kColorFilterNone,
      CompositorFilterOperations(), 0.5, SkBlendMode::kSrcOver,
      kCompositingReasonAll, CompositorElementId(2));
  RefPtr<EffectPaintPropertyNode> effect2 = EffectPaintPropertyNode::Create(
      effect1, TransformPaintPropertyNode::Root(),
      ClipPaintPropertyNode::Root(), kColorFilterNone,
      CompositorFilterOperations(), 0.3, SkBlendMode::kSrcOver,
      kCompositingReasonAll);
  RefPtr<EffectPaintPropertyNode> effect3 = EffectPaintPropertyNode::Create(
      EffectPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      ClipPaintPropertyNode::Root(), kColorFilterNone,
      CompositorFilterOperations(), 0.2, SkBlendMode::kSrcOver,
      kCompositingReasonAll);

  TestPaintArtifact artifact;
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             effect2.Get())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             effect1.Get())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             effect3.Get())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  Update(artifact.Build());

  ASSERT_EQ(3u, ContentLayerCount());

  const cc::EffectTree& effect_tree = GetPropertyTrees().effect_tree;
  // Node #0 reserved for null; #1 for root render surface; #2 for
  // EffectPaintPropertyNode::root(), plus 3 nodes for those created by
  // this test.
  ASSERT_EQ(5u, effect_tree.size());

  const cc::EffectNode& converted_root_effect = *effect_tree.Node(1);
  EXPECT_EQ(-1, converted_root_effect.parent_id);
  EXPECT_EQ(CompositorElementIdFromRootEffectId(1).ToInternalValue(),
            converted_root_effect.stable_id);

  const cc::EffectNode& converted_effect1 = *effect_tree.Node(2);
  EXPECT_EQ(converted_root_effect.id, converted_effect1.parent_id);
  EXPECT_FLOAT_EQ(0.5, converted_effect1.opacity);
  EXPECT_EQ(2u, converted_effect1.stable_id);

  const cc::EffectNode& converted_effect2 = *effect_tree.Node(3);
  EXPECT_EQ(converted_effect1.id, converted_effect2.parent_id);
  EXPECT_FLOAT_EQ(0.3, converted_effect2.opacity);

  const cc::EffectNode& converted_effect3 = *effect_tree.Node(4);
  EXPECT_EQ(converted_root_effect.id, converted_effect3.parent_id);
  EXPECT_FLOAT_EQ(0.2, converted_effect3.opacity);

  EXPECT_EQ(converted_effect2.id, ContentLayerAt(0)->effect_tree_index());
  EXPECT_EQ(converted_effect1.id, ContentLayerAt(1)->effect_tree_index());
  EXPECT_EQ(converted_effect3.id, ContentLayerAt(2)->effect_tree_index());
}

class FakeScrollClient : public WebLayerScrollClient {
 public:
  FakeScrollClient() : did_scroll_count(0) {}

  void DidScroll(const gfx::ScrollOffset& scroll_offset) final {
    did_scroll_count++;
    last_scroll_offset = scroll_offset;
  };

  gfx::ScrollOffset last_scroll_offset;
  unsigned did_scroll_count;
};

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, OneScrollNode) {
  FakeScrollClient scroll_client;

  CompositorElementId expected_compositor_element_id = ScrollElementId(2);
  RefPtr<ScrollPaintPropertyNode> scroll = ScrollPaintPropertyNode::Create(
      ScrollPaintPropertyNode::Root(), IntPoint(), IntSize(11, 13),
      IntSize(27, 31), true, false, 0 /* mainThreadScrollingReasons */,
      expected_compositor_element_id, &scroll_client);
  RefPtr<TransformPaintPropertyNode> scroll_translation =
      TransformPaintPropertyNode::Create(
          TransformPaintPropertyNode::Root(),
          TransformationMatrix().Translate(7, 9), FloatPoint3D(), false, 0,
          kCompositingReasonNone, CompositorElementId(), scroll);

  TestPaintArtifact artifact;
  artifact
      .Chunk(scroll_translation, ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(11, 13, 17, 19), Color::kWhite);
  Update(artifact.Build());

  const cc::ScrollTree& scroll_tree = GetPropertyTrees().scroll_tree;
  // Node #0 reserved for null; #1 for root render surface.
  ASSERT_EQ(3u, scroll_tree.size());
  const cc::ScrollNode& scroll_node = *scroll_tree.Node(2);
  EXPECT_EQ(gfx::Size(11, 13), scroll_node.container_bounds);
  EXPECT_EQ(gfx::Size(27, 31), scroll_node.bounds);
  EXPECT_TRUE(scroll_node.user_scrollable_horizontal);
  EXPECT_FALSE(scroll_node.user_scrollable_vertical);
  EXPECT_EQ(1, scroll_node.parent_id);
  EXPECT_EQ(expected_compositor_element_id, scroll_node.element_id);
  EXPECT_EQ(scroll_node.id,
            ElementIdToScrollNodeIndex(expected_compositor_element_id));
  EXPECT_EQ(expected_compositor_element_id, ContentLayerAt(0)->element_id());

  const cc::TransformTree& transform_tree = GetPropertyTrees().transform_tree;
  const cc::TransformNode& transform_node =
      *transform_tree.Node(scroll_node.transform_id);
  EXPECT_TRUE(transform_node.local.IsIdentity());
  EXPECT_EQ(gfx::ScrollOffset(-7, -9), transform_node.scroll_offset);

  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            scroll_node.main_thread_scrolling_reasons);

  auto* layer = ContentLayerAt(0);
  auto scroll_node_index = layer->scroll_tree_index();
  EXPECT_EQ(scroll_node_index, scroll_node.id);

  // Only one content layer, and the first child layer is the dummy layer for
  // the transform node.
  const cc::Layer* transform_node_layer = RootLayer()->children()[0].get();
  auto transform_node_index = transform_node_layer->transform_tree_index();
  EXPECT_EQ(transform_node_index, transform_node.id);

  EXPECT_EQ(0u, scroll_client.did_scroll_count);
  // TODO(pdr): The PaintArtifactCompositor should set the scrolling content
  // bounds so the Layer is scrollable. This call should be removed.
  layer->SetScrollable(gfx::Size(1, 1));
  layer->SetScrollOffsetFromImplSide(gfx::ScrollOffset(1, 2));
  EXPECT_EQ(1u, scroll_client.did_scroll_count);
  EXPECT_EQ(gfx::ScrollOffset(1, 2), scroll_client.last_scroll_offset);
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, TransformUnderScrollNode) {
  RefPtr<ScrollPaintPropertyNode> scroll = ScrollPaintPropertyNode::Create(
      ScrollPaintPropertyNode::Root(), IntPoint(), IntSize(11, 13),
      IntSize(27, 31), true, false, 0 /* mainThreadScrollingReasons */,
      CompositorElementId(), nullptr);
  RefPtr<TransformPaintPropertyNode> scroll_translation =
      TransformPaintPropertyNode::Create(
          TransformPaintPropertyNode::Root(),
          TransformationMatrix().Translate(7, 9), FloatPoint3D(), false, 0,
          kCompositingReasonNone, CompositorElementId(), scroll);

  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(
          scroll_translation, TransformationMatrix(), FloatPoint3D(), false, 0,
          kCompositingReason3DTransform);

  TestPaintArtifact artifact;
  artifact
      .Chunk(scroll_translation, ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(2, 4, 6, 8), Color::kBlack)
      .Chunk(transform, ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(1, 3, 5, 7), Color::kWhite);
  Update(artifact.Build());

  const cc::ScrollTree& scroll_tree = GetPropertyTrees().scroll_tree;
  // Node #0 reserved for null; #1 for root render surface.
  ASSERT_EQ(3u, scroll_tree.size());
  const cc::ScrollNode& scroll_node = *scroll_tree.Node(2);

  // Both layers should refer to the same scroll tree node.
  EXPECT_EQ(scroll_node.id, ContentLayerAt(0)->scroll_tree_index());
  EXPECT_EQ(scroll_node.id, ContentLayerAt(1)->scroll_tree_index());

  const cc::TransformTree& transform_tree = GetPropertyTrees().transform_tree;
  const cc::TransformNode& scroll_transform_node =
      *transform_tree.Node(scroll_node.transform_id);
  // The layers have different transform nodes.
  EXPECT_EQ(scroll_transform_node.id,
            ContentLayerAt(0)->transform_tree_index());
  EXPECT_NE(scroll_transform_node.id,
            ContentLayerAt(1)->transform_tree_index());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, NestedScrollNodes) {
  RefPtr<EffectPaintPropertyNode> effect =
      CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), 0.5);

  CompositorElementId expected_compositor_element_id_a = ScrollElementId(2);
  RefPtr<ScrollPaintPropertyNode> scroll_a = ScrollPaintPropertyNode::Create(
      ScrollPaintPropertyNode::Root(), IntPoint(), IntSize(2, 3), IntSize(5, 7),
      false, true,
      MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects,
      expected_compositor_element_id_a, nullptr);
  RefPtr<TransformPaintPropertyNode> scroll_translation_a =
      TransformPaintPropertyNode::Create(
          TransformPaintPropertyNode::Root(),
          TransformationMatrix().Translate(11, 13), FloatPoint3D(), false, 0,
          kCompositingReasonLayerForScrollingContents, CompositorElementId(),
          scroll_a);

  CompositorElementId expected_compositor_element_id_b = ScrollElementId(3);
  RefPtr<ScrollPaintPropertyNode> scroll_b = ScrollPaintPropertyNode::Create(
      scroll_translation_a->ScrollNode(), IntPoint(), IntSize(19, 23),
      IntSize(29, 31), true, false, 0 /* mainThreadScrollingReasons */,
      expected_compositor_element_id_b, nullptr);
  RefPtr<TransformPaintPropertyNode> scroll_translation_b =
      TransformPaintPropertyNode::Create(
          scroll_translation_a, TransformationMatrix().Translate(37, 41),
          FloatPoint3D(), false, 0, kCompositingReasonNone,
          CompositorElementId(), scroll_b);
  TestPaintArtifact artifact;
  artifact.Chunk(scroll_translation_a, ClipPaintPropertyNode::Root(), effect)
      .RectDrawing(FloatRect(7, 11, 13, 17), Color::kWhite);
  artifact.Chunk(scroll_translation_b, ClipPaintPropertyNode::Root(), effect)
      .RectDrawing(FloatRect(1, 2, 3, 5), Color::kWhite);
  Update(artifact.Build());

  const cc::ScrollTree& scroll_tree = GetPropertyTrees().scroll_tree;
  // Node #0 reserved for null; #1 for root render surface.
  ASSERT_EQ(3u, scroll_tree.size());
  const cc::ScrollNode& scroll_node = *scroll_tree.Node(2);
  EXPECT_EQ(gfx::Size(2, 3), scroll_node.container_bounds);
  EXPECT_EQ(gfx::Size(5, 7), scroll_node.bounds);
  EXPECT_FALSE(scroll_node.user_scrollable_horizontal);
  EXPECT_TRUE(scroll_node.user_scrollable_vertical);
  EXPECT_EQ(1, scroll_node.parent_id);
  EXPECT_EQ(expected_compositor_element_id_a, scroll_node.element_id);
  EXPECT_EQ(scroll_node.id,
            ElementIdToScrollNodeIndex(expected_compositor_element_id_a));

  EXPECT_EQ(expected_compositor_element_id_a, ContentLayerAt(0)->element_id());

  const cc::TransformTree& transform_tree = GetPropertyTrees().transform_tree;
  const cc::TransformNode& transform_node_a =
      *transform_tree.Node(scroll_node.transform_id);
  EXPECT_TRUE(transform_node_a.local.IsIdentity());
  EXPECT_EQ(gfx::ScrollOffset(-11, -13), transform_node_a.scroll_offset);

  EXPECT_TRUE(scroll_node.main_thread_scrolling_reasons &
              MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, MergeSimpleChunks) {
  TestPaintArtifact test_artifact;
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 200, 300), Color::kGray);

  const PaintArtifact& artifact = test_artifact.Build();
  ASSERT_EQ(2u, artifact.PaintChunks().size());
  Update(artifact);

  ASSERT_EQ(1u, ContentLayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::kWhite));
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, MergeClip) {
  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      FloatRoundedRect(10, 20, 50, 60));

  TestPaintArtifact test_artifact;
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), clip.Get(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 200, 300), Color::kBlack);
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 300, 400), Color::kGray);

  const PaintArtifact& artifact = test_artifact.Build();

  ASSERT_EQ(3u, artifact.PaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, ContentLayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::kWhite));
    // Clip is applied to this PaintChunk.
    rects_with_color.push_back(
        RectWithColor(FloatRect(10, 20, 50, 60), Color::kBlack));
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 300, 400), Color::kGray));

    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, Merge2DTransform) {
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(
          TransformPaintPropertyNode::Root(),
          TransformationMatrix().Translate(50, 50), FloatPoint3D(100, 100, 0),
          false, 0);

  TestPaintArtifact test_artifact;
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact
      .Chunk(transform.Get(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 200, 300), Color::kGray);

  const PaintArtifact& artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact.PaintChunks().size());
  Update(artifact);

  ASSERT_EQ(1u, ContentLayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::kWhite));
    // Transform is applied to this PaintChunk.
    rects_with_color.push_back(
        RectWithColor(FloatRect(50, 50, 100, 100), Color::kBlack));
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees,
       Merge2DTransformDirectAncestor) {
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(
          TransformPaintPropertyNode::Root(), TransformationMatrix(),
          FloatPoint3D(), false, 0, kCompositingReason3DTransform);

  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::Create(
          transform.Get(), TransformationMatrix().Translate(50, 50),
          FloatPoint3D(100, 100, 0), false, 0);

  TestPaintArtifact test_artifact;
  test_artifact
      .Chunk(transform.Get(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  // The second chunk can merge into the first because it has a descendant
  // state of the first's transform and no direct compositing reason.
  test_artifact
      .Chunk(transform2.Get(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);

  const PaintArtifact& artifact = test_artifact.Build();
  ASSERT_EQ(2u, artifact.PaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, ContentLayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::kWhite));
    // Transform is applied to this PaintChunk.
    rects_with_color.push_back(
        RectWithColor(FloatRect(50, 50, 100, 100), Color::kBlack));

    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, MergeTransformOrigin) {
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(TransformPaintPropertyNode::Root(),
                                         TransformationMatrix().Rotate(45),
                                         FloatPoint3D(100, 100, 0), false, 0);

  TestPaintArtifact test_artifact;
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact
      .Chunk(transform.Get(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 200, 300), Color::kGray);

  const PaintArtifact& artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact.PaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, ContentLayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 42, 100, 100), Color::kWhite));
    // Transform is applied to this PaintChunk.
    rects_with_color.push_back(RectWithColor(
        FloatRect(29.2893, 0.578644, 141.421, 141.421), Color::kBlack));
    rects_with_color.push_back(
        RectWithColor(FloatRect(00, 42, 200, 300), Color::kGray));

    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, MergeOpacity) {
  float opacity = 2.0 / 255.0;
  RefPtr<EffectPaintPropertyNode> effect =
      CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), opacity);

  TestPaintArtifact test_artifact;
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             effect.Get())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 200, 300), Color::kGray);

  const PaintArtifact& artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact.PaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, ContentLayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::kWhite));
    // Transform is applied to this PaintChunk.
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100),
                      Color(Color::kBlack).CombineWithAlpha(opacity).Rgb()));
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, MergeNested) {
  // Tests merging of an opacity effect, inside of a clip, inside of a
  // transform.

  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(
          TransformPaintPropertyNode::Root(),
          TransformationMatrix().Translate(50, 50), FloatPoint3D(100, 100, 0),
          false, 0);

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), transform.Get(),
      FloatRoundedRect(10, 20, 50, 60));

  float opacity = 2.0 / 255.0;
  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::Create(
      EffectPaintPropertyNode::Root(), transform.Get(), clip.Get(),
      kColorFilterNone, CompositorFilterOperations(), opacity,
      SkBlendMode::kSrcOver);

  TestPaintArtifact test_artifact;
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk(transform.Get(), clip.Get(), effect.Get())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 200, 300), Color::kGray);

  const PaintArtifact& artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact.PaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, ContentLayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::kWhite));
    // Transform is applied to this PaintChunk.
    rects_with_color.push_back(
        RectWithColor(FloatRect(60, 70, 50, 60),
                      Color(Color::kBlack).CombineWithAlpha(opacity).Rgb()));
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, ClipPushedUp) {
  // Tests merging of an element which has a clipapplied to it,
  // but has an ancestor transform of them. This can happen for fixed-
  // or absolute-position elements which escape scroll transforms.

  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(
          TransformPaintPropertyNode::Root(),
          TransformationMatrix().Translate(20, 25), FloatPoint3D(100, 100, 0),
          false, 0);

  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::Create(
          transform.Get(), TransformationMatrix().Translate(20, 25),
          FloatPoint3D(100, 100, 0), false, 0);

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), transform2.Get(),
      FloatRoundedRect(10, 20, 50, 60));

  TestPaintArtifact test_artifact;
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), clip.Get(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 300, 400), Color::kBlack);
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 200, 300), Color::kGray);

  const PaintArtifact& artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact.PaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, ContentLayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::kWhite));
    // The two transforms (combined translation of (40, 50)) are applied here,
    // before clipping.
    rects_with_color.push_back(
        RectWithColor(FloatRect(50, 70, 50, 60), Color(Color::kBlack)));
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

// TODO(crbug.com/696842): The effect refuses to "decomposite" because it's in
// a deeper transform space than its chunk. We should allow decomposite if
// the two transform nodes share the same direct compositing ancestor.
TEST_F(PaintArtifactCompositorTestWithPropertyTrees, EffectPushedUp_DISABLED) {
  // Tests merging of an element which has an effect applied to it,
  // but has an ancestor transform of them. This can happen for fixed-
  // or absolute-position elements which escape scroll transforms.

  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(
          TransformPaintPropertyNode::Root(),
          TransformationMatrix().Translate(20, 25), FloatPoint3D(100, 100, 0),
          false, 0);

  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::Create(
          transform.Get(), TransformationMatrix().Translate(20, 25),
          FloatPoint3D(100, 100, 0), false, 0);

  float opacity = 2.0 / 255.0;
  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::Create(
      EffectPaintPropertyNode::Root(), transform2.Get(),
      ClipPaintPropertyNode::Root(), kColorFilterNone,
      CompositorFilterOperations(), opacity, SkBlendMode::kSrcOver);

  TestPaintArtifact test_artifact;
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             effect.Get())
      .RectDrawing(FloatRect(0, 0, 300, 400), Color::kBlack);
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 200, 300), Color::kGray);

  const PaintArtifact& artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact.PaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, ContentLayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::kWhite));
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 300, 400),
                      Color(Color::kBlack).CombineWithAlpha(opacity).Rgb()));
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

// TODO(crbug.com/696842): The effect refuses to "decomposite" because it's in
// a deeper transform space than its chunk. We should allow decomposite if
// the two transform nodes share the same direct compositing ancestor.
TEST_F(PaintArtifactCompositorTestWithPropertyTrees,
       EffectAndClipPushedUp_DISABLED) {
  // Tests merging of an element which has an effect applied to it,
  // but has an ancestor transform of them. This can happen for fixed-
  // or absolute-position elements which escape scroll transforms.

  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(
          TransformPaintPropertyNode::Root(),
          TransformationMatrix().Translate(20, 25), FloatPoint3D(100, 100, 0),
          false, 0);

  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::Create(
          transform.Get(), TransformationMatrix().Translate(20, 25),
          FloatPoint3D(100, 100, 0), false, 0);

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), transform.Get(),
      FloatRoundedRect(10, 20, 50, 60));

  float opacity = 2.0 / 255.0;
  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::Create(
      EffectPaintPropertyNode::Root(), transform2.Get(), clip.Get(),
      kColorFilterNone, CompositorFilterOperations(), opacity,
      SkBlendMode::kSrcOver);

  TestPaintArtifact test_artifact;
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), clip.Get(), effect.Get())
      .RectDrawing(FloatRect(0, 0, 300, 400), Color::kBlack);
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 200, 300), Color::kGray);

  const PaintArtifact& artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact.PaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, ContentLayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::kWhite));
    // The clip is under |transform| but not |transform2|, so only an adjustment
    // of (20, 25) occurs.
    rects_with_color.push_back(
        RectWithColor(FloatRect(30, 45, 50, 60),
                      Color(Color::kBlack).CombineWithAlpha(opacity).Rgb()));
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, ClipAndEffectNoTransform) {
  // Tests merging of an element which has a clip and effect in the root
  // transform space.

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      FloatRoundedRect(10, 20, 50, 60));

  float opacity = 2.0 / 255.0;
  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::Create(
      EffectPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      clip.Get(), kColorFilterNone, CompositorFilterOperations(), opacity,
      SkBlendMode::kSrcOver);

  TestPaintArtifact test_artifact;
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), clip.Get(), effect.Get())
      .RectDrawing(FloatRect(0, 0, 300, 400), Color::kBlack);
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 200, 300), Color::kGray);

  const PaintArtifact& artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact.PaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, ContentLayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::kWhite));
    rects_with_color.push_back(
        RectWithColor(FloatRect(10, 20, 50, 60),
                      Color(Color::kBlack).CombineWithAlpha(opacity).Rgb()));
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, TwoClips) {
  // Tests merging of an element which has two clips in the root
  // transform space.

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      FloatRoundedRect(20, 30, 10, 20));

  RefPtr<ClipPaintPropertyNode> clip2 = ClipPaintPropertyNode::Create(
      clip.Get(), TransformPaintPropertyNode::Root(),
      FloatRoundedRect(10, 20, 50, 60));

  TestPaintArtifact test_artifact;
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), clip2.Get(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 300, 400), Color::kBlack);
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 200, 300), Color::kGray);

  const PaintArtifact& artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact.PaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, ContentLayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::kWhite));
    // The interesction of the two clips is (20, 30, 10, 20).
    rects_with_color.push_back(
        RectWithColor(FloatRect(20, 30, 10, 20), Color(Color::kBlack)));
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, TwoTransformsClipBetween) {
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(
          TransformPaintPropertyNode::Root(),
          TransformationMatrix().Translate(20, 25), FloatPoint3D(100, 100, 0),
          false, 0);
  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      FloatRoundedRect(0, 0, 50, 60));
  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::Create(
          transform.Get(), TransformationMatrix().Translate(20, 25),
          FloatPoint3D(100, 100, 0), false, 0);
  TestPaintArtifact test_artifact;
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact
      .Chunk(transform2.Get(), clip.Get(), EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 300, 400), Color::kBlack);
  test_artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 200, 300), Color::kGray);

  const PaintArtifact& artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact.PaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, ContentLayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::kWhite));
    rects_with_color.push_back(
        RectWithColor(FloatRect(40, 50, 10, 10), Color(Color::kBlack)));
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::kGray));
    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, OverlapTransform) {
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(
          TransformPaintPropertyNode::Root(),
          TransformationMatrix().Translate(50, 50), FloatPoint3D(100, 100, 0),
          false, 0, kCompositingReason3DTransform);

  TestPaintArtifact test_artifact;
  test_artifact.Chunk(DefaultPaintChunkProperties())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact
      .Chunk(transform.Get(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  test_artifact.Chunk(DefaultPaintChunkProperties())
      .RectDrawing(FloatRect(0, 0, 200, 300), Color::kGray);

  const PaintArtifact& artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact.PaintChunks().size());
  Update(artifact);
  // The third paint chunk overlaps the second but can't merge due to
  // incompatible transform. The second paint chunk can't merge into the first
  // due to a direct compositing reason.
  ASSERT_EQ(3u, ContentLayerCount());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, MightOverlap) {
  PaintChunk paint_chunk = DefaultChunk();
  paint_chunk.bounds = FloatRect(0, 0, 100, 100);
  PendingLayer pending_layer(paint_chunk, false);

  PaintChunk paint_chunk2 = DefaultChunk();
  paint_chunk2.bounds = FloatRect(0, 0, 100, 100);

  {
    PendingLayer pending_layer2(paint_chunk2, false);
    EXPECT_TRUE(MightOverlap(pending_layer, pending_layer2));
  }

  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(
          TransformPaintPropertyNode::Root(),
          TransformationMatrix().Translate(99, 0), FloatPoint3D(100, 100, 0),
          false);
  {
    paint_chunk2.properties.property_tree_state.SetTransform(transform.Get());
    PendingLayer pending_layer2(paint_chunk2, false);
    EXPECT_TRUE(MightOverlap(pending_layer, pending_layer2));
  }

  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::Create(
          TransformPaintPropertyNode::Root(),
          TransformationMatrix().Translate(100, 0), FloatPoint3D(100, 100, 0),
          false);
  {
    paint_chunk2.properties.property_tree_state.SetTransform(transform2.Get());
    PendingLayer pending_layer2(paint_chunk2, false);
    EXPECT_FALSE(MightOverlap(pending_layer, pending_layer2));
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, PendingLayer) {
  PaintChunk chunk1 = DefaultChunk();
  chunk1.properties.property_tree_state = PropertyTreeState(
      TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
      EffectPaintPropertyNode::Root());
  chunk1.properties.backface_hidden = true;
  chunk1.known_to_be_opaque = true;
  chunk1.bounds = FloatRect(0, 0, 30, 40);

  PendingLayer pending_layer(chunk1, false);

  EXPECT_TRUE(pending_layer.backface_hidden);
  EXPECT_TRUE(pending_layer.known_to_be_opaque);
  EXPECT_EQ(FloatRect(0, 0, 30, 40), pending_layer.bounds);

  PaintChunk chunk2 = DefaultChunk();
  chunk2.properties.property_tree_state = chunk1.properties.property_tree_state;
  chunk2.properties.backface_hidden = true;
  chunk2.known_to_be_opaque = true;
  chunk2.bounds = FloatRect(10, 20, 30, 40);
  pending_layer.Merge(PendingLayer(chunk2, false));

  EXPECT_TRUE(pending_layer.backface_hidden);
  // Bounds not equal to one PaintChunk.
  EXPECT_FALSE(pending_layer.known_to_be_opaque);
  EXPECT_EQ(FloatRect(0, 0, 40, 60), pending_layer.bounds);

  PaintChunk chunk3 = DefaultChunk();
  chunk3.properties.property_tree_state = chunk1.properties.property_tree_state;
  chunk3.properties.backface_hidden = true;
  chunk3.known_to_be_opaque = true;
  chunk3.bounds = FloatRect(-5, -25, 20, 20);
  pending_layer.Merge(PendingLayer(chunk3, false));

  EXPECT_TRUE(pending_layer.backface_hidden);
  EXPECT_FALSE(pending_layer.known_to_be_opaque);
  EXPECT_EQ(FloatRect(-5, -25, 45, 85), pending_layer.bounds);
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, PendingLayerWithGeometry) {
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(
          TransformPaintPropertyNode::Root(),
          TransformationMatrix().Translate(20, 25), FloatPoint3D(100, 100, 0),
          false, 0);

  PaintChunk chunk1 = DefaultChunk();
  chunk1.properties.property_tree_state = PropertyTreeState(
      TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
      EffectPaintPropertyNode::Root());
  chunk1.bounds = FloatRect(0, 0, 30, 40);

  PendingLayer pending_layer(chunk1, false);

  EXPECT_EQ(FloatRect(0, 0, 30, 40), pending_layer.bounds);

  PaintChunk chunk2 = DefaultChunk();
  chunk2.properties.property_tree_state = chunk1.properties.property_tree_state;
  chunk2.properties.property_tree_state.SetTransform(transform);
  chunk2.bounds = FloatRect(0, 0, 50, 60);
  pending_layer.Merge(PendingLayer(chunk2, false));

  EXPECT_EQ(FloatRect(0, 0, 70, 85), pending_layer.bounds);
}

// TODO(crbug.com/701991):
// The test is disabled because opaque rect mapping is not implemented yet.
TEST_F(PaintArtifactCompositorTestWithPropertyTrees,
       PendingLayerKnownOpaque_DISABLED) {
  PaintChunk chunk1 = DefaultChunk();
  chunk1.properties.property_tree_state = PropertyTreeState(
      TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
      EffectPaintPropertyNode::Root());
  chunk1.bounds = FloatRect(0, 0, 30, 40);
  chunk1.known_to_be_opaque = false;
  PendingLayer pending_layer(chunk1, false);

  EXPECT_FALSE(pending_layer.known_to_be_opaque);

  PaintChunk chunk2 = DefaultChunk();
  chunk2.properties.property_tree_state = chunk1.properties.property_tree_state;
  chunk2.bounds = FloatRect(0, 0, 25, 35);
  chunk2.known_to_be_opaque = true;
  pending_layer.Merge(PendingLayer(chunk2, false));

  // Chunk 2 doesn't cover the entire layer, so not opaque.
  EXPECT_FALSE(pending_layer.known_to_be_opaque);

  PaintChunk chunk3 = DefaultChunk();
  chunk3.properties.property_tree_state = chunk1.properties.property_tree_state;
  chunk3.bounds = FloatRect(0, 0, 50, 60);
  chunk3.known_to_be_opaque = true;
  pending_layer.Merge(PendingLayer(chunk3, false));

  // Chunk 3 covers the entire layer, so now it's opaque.
  EXPECT_TRUE(pending_layer.known_to_be_opaque);
}

PassRefPtr<EffectPaintPropertyNode> CreateSampleEffectNodeWithElementId() {
  CompositorElementId expected_compositor_element_id(2);
  float opacity = 2.0 / 255.0;
  return EffectPaintPropertyNode::Create(
      EffectPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      ClipPaintPropertyNode::Root(), kColorFilterNone,
      CompositorFilterOperations(), opacity, SkBlendMode::kSrcOver,
      kCompositingReasonActiveAnimation, expected_compositor_element_id);
}

PassRefPtr<TransformPaintPropertyNode>
CreateSampleTransformNodeWithElementId() {
  CompositorElementId expected_compositor_element_id(3);
  return TransformPaintPropertyNode::Create(
      TransformPaintPropertyNode::Root(), TransformationMatrix().Rotate(90),
      FloatPoint3D(100, 100, 0), false, 0, kCompositingReason3DTransform,
      expected_compositor_element_id);
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, TransformWithElementId) {
  RefPtr<TransformPaintPropertyNode> transform =
      CreateSampleTransformNodeWithElementId();
  TestPaintArtifact artifact;
  artifact
      .Chunk(transform, ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(100, 100, 200, 100), Color::kBlack);
  Update(artifact.Build());

  EXPECT_EQ(2,
            ElementIdToTransformNodeIndex(transform->GetCompositorElementId()));
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees,
       TransformNodeHasOwningLayerId) {
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(
          TransformPaintPropertyNode::Root(), TransformationMatrix().Rotate(90),
          FloatPoint3D(100, 100, 0), false, 0, kCompositingReason3DTransform);

  TestPaintArtifact artifact;
  artifact
      .Chunk(transform, ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(100, 100, 200, 100), Color::kBlack);
  Update(artifact.Build());

  // Only one content layer, and the first child layer is the dummy layer for
  // the transform node.
  ASSERT_EQ(1u, ContentLayerCount());
  const cc::Layer* transform_node_layer = RootLayer()->children()[0].get();
  const cc::TransformNode* cc_transform_node =
      GetPropertyTrees().transform_tree.Node(
          transform_node_layer->transform_tree_index());
  auto transform_node_index = transform_node_layer->transform_tree_index();
  EXPECT_EQ(transform_node_index, cc_transform_node->id);
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, EffectWithElementId) {
  RefPtr<EffectPaintPropertyNode> effect =
      CreateSampleEffectNodeWithElementId();
  TestPaintArtifact artifact;
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             effect.Get())
      .RectDrawing(FloatRect(100, 100, 200, 100), Color::kBlack);
  Update(artifact.Build());

  EXPECT_EQ(2, ElementIdToEffectNodeIndex(effect->GetCompositorElementId()));
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, CompositedLuminanceMask) {
  RefPtr<EffectPaintPropertyNode> masked = EffectPaintPropertyNode::Create(
      EffectPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      ClipPaintPropertyNode::Root(), kColorFilterNone,
      CompositorFilterOperations(), 1.0, SkBlendMode::kSrcOver,
      kCompositingReasonIsolateCompositedDescendants);
  RefPtr<EffectPaintPropertyNode> masking = EffectPaintPropertyNode::Create(
      masked, TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
      kColorFilterLuminanceToAlpha, CompositorFilterOperations(), 1.0,
      SkBlendMode::kDstIn, kCompositingReasonSquashingDisallowed);

  TestPaintArtifact artifact;
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             masked.Get())
      .RectDrawing(FloatRect(100, 100, 200, 200), Color::kGray);
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             masking.Get())
      .RectDrawing(FloatRect(150, 150, 100, 100), Color::kWhite);
  Update(artifact.Build());
  ASSERT_EQ(2u, ContentLayerCount());

  const cc::Layer* masked_layer = ContentLayerAt(0);
  EXPECT_THAT(masked_layer->GetPicture(),
              Pointee(DrawsRectangle(FloatRect(0, 0, 200, 200), Color::kGray)));
  EXPECT_EQ(Translation(100, 100), masked_layer->ScreenSpaceTransform());
  EXPECT_EQ(gfx::Size(200, 200), masked_layer->bounds());
  const cc::EffectNode* masked_group =
      GetPropertyTrees().effect_tree.Node(masked_layer->effect_tree_index());
  EXPECT_TRUE(masked_group->has_render_surface);

  const cc::Layer* masking_layer = ContentLayerAt(1);
  EXPECT_THAT(
      masking_layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 100, 100), Color::kWhite)));
  EXPECT_EQ(Translation(150, 150), masking_layer->ScreenSpaceTransform());
  EXPECT_EQ(gfx::Size(100, 100), masking_layer->bounds());
  const cc::EffectNode* masking_group =
      GetPropertyTrees().effect_tree.Node(masking_layer->effect_tree_index());
  EXPECT_TRUE(masking_group->has_render_surface);
  EXPECT_EQ(masked_group->id, masking_group->parent_id);
  ASSERT_EQ(1u, masking_group->filters.size());
  EXPECT_EQ(cc::FilterOperation::REFERENCE,
            masking_group->filters.at(0).type());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees,
       UpdateProducesNewSequenceNumber) {
  // A 90 degree clockwise rotation about (100, 100).
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(
          TransformPaintPropertyNode::Root(), TransformationMatrix().Rotate(90),
          FloatPoint3D(100, 100, 0), false, 0, kCompositingReason3DTransform);

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      FloatRoundedRect(100, 100, 300, 200));

  RefPtr<EffectPaintPropertyNode> effect =
      CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), 0.5);

  TestPaintArtifact artifact;
  artifact.Chunk(transform, clip, effect)
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kGray);
  Update(artifact.Build());

  // Two content layers for the differentiated rect drawings and three dummy
  // layers for each of the transform, clip and effect nodes.
  EXPECT_EQ(5u, RootLayer()->children().size());
  int sequence_number = GetPropertyTrees().sequence_number;
  EXPECT_GT(sequence_number, 0);
  for (auto layer : RootLayer()->children()) {
    EXPECT_EQ(sequence_number, layer->property_tree_sequence_number());
  }

  Update(artifact.Build());

  EXPECT_EQ(5u, RootLayer()->children().size());
  sequence_number++;
  EXPECT_EQ(sequence_number, GetPropertyTrees().sequence_number);
  for (auto layer : RootLayer()->children()) {
    EXPECT_EQ(sequence_number, layer->property_tree_sequence_number());
  }

  Update(artifact.Build());

  EXPECT_EQ(5u, RootLayer()->children().size());
  sequence_number++;
  EXPECT_EQ(sequence_number, GetPropertyTrees().sequence_number);
  for (auto layer : RootLayer()->children()) {
    EXPECT_EQ(sequence_number, layer->property_tree_sequence_number());
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, DecompositeClip) {
  // A clipped paint chunk that gets merged into a previous layer should
  // only contribute clipped bounds to the layer bound.

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      FloatRoundedRect(75, 75, 100, 100));

  TestPaintArtifact artifact;
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(50, 50, 100, 100), Color::kGray);
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), clip.Get(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(100, 100, 100, 100), Color::kGray);
  Update(artifact.Build());
  ASSERT_EQ(1u, ContentLayerCount());

  const cc::Layer* layer = ContentLayerAt(0);
  EXPECT_EQ(gfx::Vector2dF(50.f, 50.f), layer->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(125, 125), layer->bounds());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, DecompositeEffect) {
  // An effect node without direct compositing reason and does not need to
  // group compositing descendants should not be composited and can merge
  // with other chunks.

  RefPtr<EffectPaintPropertyNode> effect =
      CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), 0.5);

  TestPaintArtifact artifact;
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(50, 25, 100, 100), Color::kGray);
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             effect.Get())
      .RectDrawing(FloatRect(25, 75, 100, 100), Color::kGray);
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(75, 75, 100, 100), Color::kGray);
  Update(artifact.Build());
  ASSERT_EQ(1u, ContentLayerCount());

  const cc::Layer* layer = ContentLayerAt(0);
  EXPECT_EQ(gfx::Vector2dF(25.f, 25.f), layer->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(150, 150), layer->bounds());
  EXPECT_EQ(1, layer->effect_tree_index());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, DirectlyCompositedEffect) {
  // An effect node with direct compositing shall be composited.
  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::Create(
      EffectPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      ClipPaintPropertyNode::Root(), kColorFilterNone,
      CompositorFilterOperations(), 0.5f, SkBlendMode::kSrcOver,
      kCompositingReasonAll);

  TestPaintArtifact artifact;
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(50, 25, 100, 100), Color::kGray);
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             effect.Get())
      .RectDrawing(FloatRect(25, 75, 100, 100), Color::kGray);
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(75, 75, 100, 100), Color::kGray);
  Update(artifact.Build());
  ASSERT_EQ(3u, ContentLayerCount());

  const cc::Layer* layer1 = ContentLayerAt(0);
  EXPECT_EQ(gfx::Vector2dF(50.f, 25.f), layer1->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(100, 100), layer1->bounds());
  EXPECT_EQ(1, layer1->effect_tree_index());

  const cc::Layer* layer2 = ContentLayerAt(1);
  EXPECT_EQ(gfx::Vector2dF(25.f, 75.f), layer2->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(100, 100), layer2->bounds());
  const cc::EffectNode* effect_node =
      GetPropertyTrees().effect_tree.Node(layer2->effect_tree_index());
  EXPECT_EQ(1, effect_node->parent_id);
  EXPECT_EQ(0.5f, effect_node->opacity);

  const cc::Layer* layer3 = ContentLayerAt(2);
  EXPECT_EQ(gfx::Vector2dF(75.f, 75.f), layer3->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(100, 100), layer3->bounds());
  EXPECT_EQ(1, layer3->effect_tree_index());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, DecompositeDeepEffect) {
  // A paint chunk may enter multiple level effects with or without compositing
  // reasons. This test verifies we still decomposite effects without a direct
  // reason, but stop at a directly composited effect.
  RefPtr<EffectPaintPropertyNode> effect1 =
      CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), 0.1f);
  RefPtr<EffectPaintPropertyNode> effect2 = EffectPaintPropertyNode::Create(
      effect1, TransformPaintPropertyNode::Root(),
      ClipPaintPropertyNode::Root(), kColorFilterNone,
      CompositorFilterOperations(), 0.2f, SkBlendMode::kSrcOver,
      kCompositingReasonAll);
  RefPtr<EffectPaintPropertyNode> effect3 =
      CreateOpacityOnlyEffect(effect2, 0.3f);

  TestPaintArtifact artifact;
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(50, 25, 100, 100), Color::kGray);
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             effect3.Get())
      .RectDrawing(FloatRect(25, 75, 100, 100), Color::kGray);
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(75, 75, 100, 100), Color::kGray);
  Update(artifact.Build());
  ASSERT_EQ(3u, ContentLayerCount());

  const cc::Layer* layer1 = ContentLayerAt(0);
  EXPECT_EQ(gfx::Vector2dF(50.f, 25.f), layer1->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(100, 100), layer1->bounds());
  EXPECT_EQ(1, layer1->effect_tree_index());

  const cc::Layer* layer2 = ContentLayerAt(1);
  EXPECT_EQ(gfx::Vector2dF(25.f, 75.f), layer2->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(100, 100), layer2->bounds());
  const cc::EffectNode* effect_node2 =
      GetPropertyTrees().effect_tree.Node(layer2->effect_tree_index());
  EXPECT_EQ(0.2f, effect_node2->opacity);
  const cc::EffectNode* effect_node1 =
      GetPropertyTrees().effect_tree.Node(effect_node2->parent_id);
  EXPECT_EQ(1, effect_node1->parent_id);
  EXPECT_EQ(0.1f, effect_node1->opacity);

  const cc::Layer* layer3 = ContentLayerAt(2);
  EXPECT_EQ(gfx::Vector2dF(75.f, 75.f), layer3->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(100, 100), layer3->bounds());
  EXPECT_EQ(1, layer3->effect_tree_index());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees,
       IndirectlyCompositedEffect) {
  // An effect node without direct compositing still needs to be composited
  // for grouping, if some chunks need to be composited.
  RefPtr<EffectPaintPropertyNode> effect =
      CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), 0.5f);
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(
          TransformPaintPropertyNode::Root(), TransformationMatrix(),
          FloatPoint3D(), false, 0, kCompositingReason3DTransform);

  TestPaintArtifact artifact;
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(50, 25, 100, 100), Color::kGray);
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             effect.Get())
      .RectDrawing(FloatRect(25, 75, 100, 100), Color::kGray);
  artifact.Chunk(transform.Get(), ClipPaintPropertyNode::Root(), effect.Get())
      .RectDrawing(FloatRect(75, 75, 100, 100), Color::kGray);
  Update(artifact.Build());
  ASSERT_EQ(3u, ContentLayerCount());

  const cc::Layer* layer1 = ContentLayerAt(0);
  EXPECT_EQ(gfx::Vector2dF(50.f, 25.f), layer1->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(100, 100), layer1->bounds());
  EXPECT_EQ(1, layer1->effect_tree_index());

  const cc::Layer* layer2 = ContentLayerAt(1);
  EXPECT_EQ(gfx::Vector2dF(25.f, 75.f), layer2->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(100, 100), layer2->bounds());
  const cc::EffectNode* effect_node =
      GetPropertyTrees().effect_tree.Node(layer2->effect_tree_index());
  EXPECT_EQ(1, effect_node->parent_id);
  EXPECT_EQ(0.5f, effect_node->opacity);

  const cc::Layer* layer3 = ContentLayerAt(2);
  EXPECT_EQ(gfx::Vector2dF(75.f, 75.f), layer3->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(100, 100), layer3->bounds());
  EXPECT_EQ(effect_node->id, layer3->effect_tree_index());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees,
       DecompositedEffectNotMergingDueToOverlap) {
  // This tests an effect that doesn't need to be composited, but needs
  // separate backing due to overlap with a previous composited effect.
  RefPtr<EffectPaintPropertyNode> effect1 =
      CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), 0.1f);
  RefPtr<EffectPaintPropertyNode> effect2 =
      CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), 0.2f);
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(
          TransformPaintPropertyNode::Root(), TransformationMatrix(),
          FloatPoint3D(), false, 0, kCompositingReason3DTransform);
  TestPaintArtifact artifact;
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 50, 50), Color::kGray);
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             effect1.Get())
      .RectDrawing(FloatRect(100, 0, 50, 50), Color::kGray);
  // This chunk has a transform that must be composited, thus causing effect1
  // to be composited too.
  artifact.Chunk(transform.Get(), ClipPaintPropertyNode::Root(), effect1.Get())
      .RectDrawing(FloatRect(200, 0, 50, 50), Color::kGray);
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             effect2.Get())
      .RectDrawing(FloatRect(200, 100, 50, 50), Color::kGray);
  // This chunk overlaps with the 2nd chunk, but is seemingly safe to merge.
  // However because effect1 gets composited due to a composited transform,
  // we can't merge with effect1 nor skip it to merge with the first chunk.
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             effect2.Get())
      .RectDrawing(FloatRect(100, 0, 50, 50), Color::kGray);

  Update(artifact.Build());
  ASSERT_EQ(4u, ContentLayerCount());

  const cc::Layer* layer1 = ContentLayerAt(0);
  EXPECT_EQ(gfx::Vector2dF(0.f, 0.f), layer1->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(50, 50), layer1->bounds());
  EXPECT_EQ(1, layer1->effect_tree_index());

  const cc::Layer* layer2 = ContentLayerAt(1);
  EXPECT_EQ(gfx::Vector2dF(100.f, 0.f), layer2->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(50, 50), layer2->bounds());
  const cc::EffectNode* effect_node =
      GetPropertyTrees().effect_tree.Node(layer2->effect_tree_index());
  EXPECT_EQ(1, effect_node->parent_id);
  EXPECT_EQ(0.1f, effect_node->opacity);

  const cc::Layer* layer3 = ContentLayerAt(2);
  EXPECT_EQ(gfx::Vector2dF(200.f, 0.f), layer3->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(50, 50), layer3->bounds());
  EXPECT_EQ(effect_node->id, layer3->effect_tree_index());

  const cc::Layer* layer4 = ContentLayerAt(3);
  EXPECT_EQ(gfx::Vector2dF(100.f, 0.f), layer4->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(150, 150), layer4->bounds());
  EXPECT_EQ(1, layer4->effect_tree_index());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees,
       UpdatePopulatesCompositedElementIds) {
  RefPtr<TransformPaintPropertyNode> transform =
      CreateSampleTransformNodeWithElementId();
  RefPtr<EffectPaintPropertyNode> effect =
      CreateSampleEffectNodeWithElementId();
  TestPaintArtifact artifact;
  artifact
      .Chunk(transform, ClipPaintPropertyNode::Root(),
             EffectPaintPropertyNode::Root())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack)
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             effect.Get())
      .RectDrawing(FloatRect(100, 100, 200, 100), Color::kBlack);

  CompositorElementIdSet composited_element_ids;
  Update(artifact.Build(), composited_element_ids);

  EXPECT_EQ(2u, composited_element_ids.size());
  EXPECT_TRUE(
      composited_element_ids.Contains(transform->GetCompositorElementId()));
  EXPECT_TRUE(
      composited_element_ids.Contains(effect->GetCompositorElementId()));
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, SkipChunkWithOpacityZero) {
  {
    TestPaintArtifact artifact;
    CreateSimpleArtifactWithOpacity(artifact, 0, false, false);
    ASSERT_EQ(0u, ContentLayerCount());
  }
  {
    TestPaintArtifact artifact;
    CreateSimpleArtifactWithOpacity(artifact, 0, true, false);
    ASSERT_EQ(1u, ContentLayerCount());
  }
  {
    TestPaintArtifact artifact;
    CreateSimpleArtifactWithOpacity(artifact, 0, true, true);
    ASSERT_EQ(1u, ContentLayerCount());
  }
  {
    TestPaintArtifact artifact;
    CreateSimpleArtifactWithOpacity(artifact, 0, false, true);
    ASSERT_EQ(1u, ContentLayerCount());
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees, SkipChunkWithTinyOpacity) {
  {
    TestPaintArtifact artifact;
    CreateSimpleArtifactWithOpacity(artifact, 0.0003f, false, false);
    ASSERT_EQ(0u, ContentLayerCount());
  }
  {
    TestPaintArtifact artifact;
    CreateSimpleArtifactWithOpacity(artifact, 0.0003f, true, false);
    ASSERT_EQ(1u, ContentLayerCount());
  }
  {
    TestPaintArtifact artifact;
    CreateSimpleArtifactWithOpacity(artifact, 0.0003f, true, true);
    ASSERT_EQ(1u, ContentLayerCount());
  }
  {
    TestPaintArtifact artifact;
    CreateSimpleArtifactWithOpacity(artifact, 0.0003f, false, true);
    ASSERT_EQ(1u, ContentLayerCount());
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees,
       DontSkipChunkWithMinimumOpacity) {
  {
    TestPaintArtifact artifact;
    CreateSimpleArtifactWithOpacity(artifact, 0.0004f, false, false);
    ASSERT_EQ(1u, ContentLayerCount());
  }
  {
    TestPaintArtifact artifact;
    CreateSimpleArtifactWithOpacity(artifact, 0.0004f, true, false);
    ASSERT_EQ(1u, ContentLayerCount());
  }
  {
    TestPaintArtifact artifact;
    CreateSimpleArtifactWithOpacity(artifact, 0.0004f, true, true);
    ASSERT_EQ(1u, ContentLayerCount());
  }
  {
    TestPaintArtifact artifact;
    CreateSimpleArtifactWithOpacity(artifact, 0.0004f, false, true);
    ASSERT_EQ(1u, ContentLayerCount());
  }
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees,
       DontSkipChunkWithAboveMinimumOpacity) {
  {
    TestPaintArtifact artifact;
    CreateSimpleArtifactWithOpacity(artifact, 0.3f, false, false);
    ASSERT_EQ(1u, ContentLayerCount());
  }
  {
    TestPaintArtifact artifact;
    CreateSimpleArtifactWithOpacity(artifact, 0.3f, true, false);
    ASSERT_EQ(1u, ContentLayerCount());
  }
  {
    TestPaintArtifact artifact;
    CreateSimpleArtifactWithOpacity(artifact, 0.3f, true, true);
    ASSERT_EQ(1u, ContentLayerCount());
  }
  {
    TestPaintArtifact artifact;
    CreateSimpleArtifactWithOpacity(artifact, 0.3f, false, true);
    ASSERT_EQ(1u, ContentLayerCount());
  }
}

PassRefPtr<EffectPaintPropertyNode> CreateEffectWithOpacityAndReason(
    float opacity,
    CompositingReasons reason,
    RefPtr<EffectPaintPropertyNode> parent = nullptr) {
  return EffectPaintPropertyNode::Create(
      parent ? parent : EffectPaintPropertyNode::Root(),
      TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
      kColorFilterNone, CompositorFilterOperations(), opacity,
      SkBlendMode::kSrcOver, reason);
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees,
       DontSkipChunkWithTinyOpacityAndDirectCompositingReason) {
  RefPtr<EffectPaintPropertyNode> effect =
      CreateEffectWithOpacityAndReason(0.0001f, kCompositingReasonCanvas);
  TestPaintArtifact artifact;
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             effect)
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());
  ASSERT_EQ(1u, ContentLayerCount());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees,
       SkipChunkWithTinyOpacityAndVisibleChildEffectNode) {
  RefPtr<EffectPaintPropertyNode> tinyEffect =
      CreateEffectWithOpacityAndReason(0.0001f, kCompositingReasonNone);
  RefPtr<EffectPaintPropertyNode> visibleEffect =
      CreateEffectWithOpacityAndReason(0.5f, kCompositingReasonNone,
                                       tinyEffect);
  TestPaintArtifact artifact;
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             visibleEffect)
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());
  ASSERT_EQ(0u, ContentLayerCount());
}

TEST_F(
    PaintArtifactCompositorTestWithPropertyTrees,
    DontSkipChunkWithTinyOpacityAndVisibleChildEffectNodeWithCompositingParent) {
  RefPtr<EffectPaintPropertyNode> tinyEffect =
      CreateEffectWithOpacityAndReason(0.0001f, kCompositingReasonCanvas);
  RefPtr<EffectPaintPropertyNode> visibleEffect =
      CreateEffectWithOpacityAndReason(0.5f, kCompositingReasonNone,
                                       tinyEffect);
  TestPaintArtifact artifact;
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             visibleEffect)
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());
  ASSERT_EQ(1u, ContentLayerCount());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees,
       SkipChunkWithTinyOpacityAndVisibleChildEffectNodeWithCompositingChild) {
  RefPtr<EffectPaintPropertyNode> tinyEffect =
      CreateEffectWithOpacityAndReason(0.0001f, kCompositingReasonNone);
  RefPtr<EffectPaintPropertyNode> visibleEffect =
      CreateEffectWithOpacityAndReason(0.5f, kCompositingReasonCanvas,
                                       tinyEffect);
  TestPaintArtifact artifact;
  artifact
      .Chunk(TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
             visibleEffect)
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());
  ASSERT_EQ(0u, ContentLayerCount());
}

TEST_F(PaintArtifactCompositorTestWithPropertyTrees,
       UpdateManagesLayerElementIds) {
  RefPtr<TransformPaintPropertyNode> transform =
      CreateSampleTransformNodeWithElementId();
  CompositorElementId element_id = transform->GetCompositorElementId();

  {
    TestPaintArtifact artifact;
    artifact
        .Chunk(transform, ClipPaintPropertyNode::Root(),
               EffectPaintPropertyNode::Root())
        .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);

    Update(artifact.Build());
    ASSERT_EQ(1u, ContentLayerCount());
    ASSERT_TRUE(GetLayerTreeHost().LayerByElementId(element_id));
  }

  {
    TestPaintArtifact artifact;
    ASSERT_TRUE(GetLayerTreeHost().LayerByElementId(element_id));
    Update(artifact.Build());
    ASSERT_EQ(0u, ContentLayerCount());
    ASSERT_FALSE(GetLayerTreeHost().LayerByElementId(element_id));
  }
}

}  // namespace blink
