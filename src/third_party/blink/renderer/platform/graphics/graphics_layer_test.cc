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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"

#include <memory>
#include <utility>

#include "cc/layers/picture_layer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller_test.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/testing/fake_graphics_layer_client.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

using ::testing::ElementsAre;

namespace blink {

class GraphicsLayerTest : public PaintControllerTestBase {
 protected:
  const RasterInvalidator* GetInternalRasterInvalidator(
      const GraphicsLayer& layer) {
    return layer.raster_invalidator_.get();
  }

  RasterInvalidator& EnsureRasterInvalidator(GraphicsLayer& layer) {
    return layer.EnsureRasterInvalidator();
  }

  const PaintController* GetInternalPaintController(
      const GraphicsLayer& layer) {
    return layer.paint_controller_.get();
  }

 private:
  ScopedCompositeAfterPaintForTest cap_{false};
};

TEST_F(GraphicsLayerTest, PaintRecursively) {
  FakeGraphicsLayerClient* client =
      MakeGarbageCollected<FakeGraphicsLayerClient>();
  GraphicsLayer* root = MakeGarbageCollected<GraphicsLayer>(*client);
  root->SetPaintsHitTest(true);
  root->SetLayerState(PropertyTreeState::Root(), gfx::Vector2d());

  // Initially layer1 doesn't draw content.
  GraphicsLayer* layer1 = MakeGarbageCollected<GraphicsLayer>(*client);
  EXPECT_FALSE(layer1->DrawsContent());
  auto t1 = Create2DTranslation(t0(), 10, 20);
  PropertyTreeState layer1_state(*t1, c0(), e0());
  layer1->SetLayerState(layer1_state, gfx::Vector2d());
  root->AddChild(layer1);
  GraphicsLayer* layer2 = MakeGarbageCollected<GraphicsLayer>(*client);
  layer2->SetDrawsContent(true);
  auto t2 = Create2DTranslation(t0(), 10, 20);
  PropertyTreeState layer2_state(*t2, c0(), e0());
  layer2->SetLayerState(layer2_state, gfx::Vector2d());
  root->AddChild(layer2);

  client->SetPainter([&](const GraphicsLayer* layer, GraphicsContext& context,
                         GraphicsLayerPaintingPhase, const IntRect&) {
    if (layer == root) {
      context.GetPaintController().RecordHitTestData(
          *layer, gfx::Rect(1, 2, 3, 4), TouchAction::kNone, false);
    } else if (layer == layer1) {
      ScopedPaintChunkProperties properties(
          context.GetPaintController(), layer1_state, *layer, kBackgroundType);
      PaintControllerTestBase::DrawRect(context, *layer, kBackgroundType,
                                        gfx::Rect(2, 3, 4, 5));
    } else if (layer == layer2) {
      ScopedPaintChunkProperties properties(
          context.GetPaintController(), layer2_state, *layer, kBackgroundType);
      PaintControllerTestBase::DrawRect(context, *layer, kBackgroundType,
                                        gfx::Rect(3, 4, 5, 6));
    }
  });

  GraphicsContext context(GetPaintController());
  client->SetNeedsRepaint(true);
  HeapVector<PreCompositedLayerInfo> pre_composited_layers;
  {
    PaintController::CycleScope cycle_scope;
    EXPECT_TRUE(
        root->PaintRecursively(context, pre_composited_layers, cycle_scope));
  }
  EXPECT_TRUE(root->Repainted());
  EXPECT_FALSE(layer1->Repainted());
  EXPECT_TRUE(layer2->Repainted());

  HitTestData hit_test_data;
  hit_test_data.touch_action_rects = {{gfx::Rect(1, 2, 3, 4)}};
  ASSERT_EQ(2u, pre_composited_layers.size());
  EXPECT_EQ(root, pre_composited_layers[0].graphics_layer);
  EXPECT_THAT(
      pre_composited_layers[0].chunks,
      ElementsAre(IsPaintChunk(
          0, 0, PaintChunk::Id(root->Id(), DisplayItem::kHitTest),
          PropertyTreeState::Root(), &hit_test_data, gfx::Rect(1, 2, 3, 4))));
  EXPECT_THAT(pre_composited_layers[0].chunks.begin().DisplayItems(),
              ElementsAre());
  EXPECT_EQ(layer2, pre_composited_layers[1].graphics_layer);
  EXPECT_THAT(pre_composited_layers[1].chunks,
              ElementsAre(IsPaintChunk(
                  0, 1, PaintChunk::Id(layer2->Id(), kBackgroundType),
                  layer2_state, nullptr, gfx::Rect(3, 4, 5, 6))));
  EXPECT_THAT(pre_composited_layers[1].chunks.begin().DisplayItems(),
              ElementsAre(IsSameId(layer2->Id(), kBackgroundType)));

  // Paint again with nothing changed.
  client->SetNeedsRepaint(false);
  pre_composited_layers.clear();
  {
    PaintController::CycleScope cycle_scope;
    EXPECT_FALSE(
        root->PaintRecursively(context, pre_composited_layers, cycle_scope));
  }
  EXPECT_FALSE(root->Repainted());
  EXPECT_FALSE(layer1->Repainted());
  EXPECT_FALSE(layer2->Repainted());
  EXPECT_EQ(2u, pre_composited_layers.size());

  // Paint again with layer1 drawing content.
  layer1->SetDrawsContent(true);
  pre_composited_layers.clear();
  {
    PaintController::CycleScope cycle_scope;
    EXPECT_TRUE(
        root->PaintRecursively(context, pre_composited_layers, cycle_scope));
  }
  EXPECT_FALSE(root->Repainted());
  EXPECT_TRUE(layer1->Repainted());
  EXPECT_FALSE(layer2->Repainted());

  EXPECT_EQ(3u, pre_composited_layers.size());
  EXPECT_EQ(root, pre_composited_layers[0].graphics_layer);
  EXPECT_THAT(
      pre_composited_layers[0].chunks,
      ElementsAre(IsPaintChunk(
          0, 0, PaintChunk::Id(root->Id(), DisplayItem::kHitTest),
          PropertyTreeState::Root(), &hit_test_data, gfx::Rect(1, 2, 3, 4))));
  EXPECT_THAT(pre_composited_layers[0].chunks.begin().DisplayItems(),
              ElementsAre());
  EXPECT_EQ(layer1, pre_composited_layers[1].graphics_layer);
  EXPECT_THAT(pre_composited_layers[1].chunks,
              ElementsAre(IsPaintChunk(
                  0, 1, PaintChunk::Id(layer1->Id(), kBackgroundType),
                  layer1_state, nullptr, gfx::Rect(2, 3, 4, 5))));
  EXPECT_THAT(pre_composited_layers[1].chunks.begin().DisplayItems(),
              ElementsAre(IsSameId(layer1->Id(), kBackgroundType)));
  EXPECT_EQ(layer2, pre_composited_layers[2].graphics_layer);
  EXPECT_THAT(pre_composited_layers[2].chunks,
              ElementsAre(IsPaintChunk(
                  0, 1, PaintChunk::Id(layer2->Id(), kBackgroundType),
                  layer2_state, nullptr, gfx::Rect(3, 4, 5, 6))));
  EXPECT_THAT(pre_composited_layers[2].chunks.begin().DisplayItems(),
              ElementsAre(IsSameId(layer2->Id(), kBackgroundType)));

  root->Destroy();
  layer1->Destroy();
  layer2->Destroy();
}

TEST_F(GraphicsLayerTest, SetDrawsContentFalse) {
  FakeGraphicsLayerClient* client =
      MakeGarbageCollected<FakeGraphicsLayerClient>();
  GraphicsLayer* layer = MakeGarbageCollected<GraphicsLayer>(*client);
  layer->SetDrawsContent(true);

  layer->GetPaintController();
  EXPECT_NE(nullptr, GetInternalPaintController(*layer));
  EnsureRasterInvalidator(*layer);
  EXPECT_NE(nullptr, GetInternalRasterInvalidator(*layer));

  layer->SetDrawsContent(false);
  EXPECT_EQ(nullptr, GetInternalPaintController(*layer));
  EXPECT_EQ(nullptr, GetInternalRasterInvalidator(*layer));

  layer->Destroy();
}

TEST_F(GraphicsLayerTest, ContentsLayer) {
  FakeGraphicsLayerClient* client =
      MakeGarbageCollected<FakeGraphicsLayerClient>();
  GraphicsLayer* graphics_layer = MakeGarbageCollected<GraphicsLayer>(*client);
  auto contents_layer = cc::Layer::Create();
  graphics_layer->SetContentsToCcLayer(contents_layer);
  EXPECT_TRUE(graphics_layer->HasContentsLayer());
  EXPECT_EQ(contents_layer.get(), graphics_layer->ContentsLayer());
  graphics_layer->SetContentsToCcLayer(nullptr);
  EXPECT_FALSE(graphics_layer->HasContentsLayer());
  EXPECT_EQ(nullptr, graphics_layer->ContentsLayer());

  graphics_layer->Destroy();
}

}  // namespace blink
