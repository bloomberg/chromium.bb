// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_AND_RASTER_INVALIDATION_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_AND_RASTER_INVALIDATION_TEST_H_

#include "cc/animation/animation_host.h"
#include "cc/layers/picture_layer.h"
#include "cc/test/stub_layer_tree_host_client.h"
#include "cc/test/stub_layer_tree_host_single_thread_client.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/platform/graphics/compositing/content_layer_client_impl.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"

namespace blink {

class PaintAndRasterInvalidationTest : public PaintControllerPaintTest {
 public:
  PaintAndRasterInvalidationTest()
      : PaintControllerPaintTest(SingleChildLocalFrameClient::Create()) {}

 protected:
  cc::Layer* GetCcLayer(size_t index = 0) const {
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      return GetDocument()
          .View()
          ->GetPaintArtifactCompositorForTesting()
          ->RootLayer()
          ->children()[index]
          .get();
    }
    return GetLayoutView().Layer()->GraphicsLayerBacking()->ContentLayer();
  }

  cc::LayerClient* GetCcLayerClient(size_t index = 0) const {
    return GetCcLayer(index)->GetLayerClientForTesting();
  }

  const RasterInvalidationTracking* GetRasterInvalidationTracking(
      size_t index = 0) const {
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      return static_cast<ContentLayerClientImpl*>(GetCcLayerClient(index))
          ->GetRasterInvalidationTrackingForTesting();
    }
    return GetLayoutView()
        .Layer()
        ->GraphicsLayerBacking()
        ->GetRasterInvalidationTracking();
  }

  void SetUp() override {
    PaintControllerPaintTest::SetUp();

    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      cc::LayerTreeSettings settings;
      settings.layer_transforms_should_scale_layer_contents = true;

      animation_host_ = cc::AnimationHost::CreateMainInstance();
      cc::LayerTreeHost::InitParams params;
      params.client = &layer_tree_host_client_;
      params.settings = &settings;
      params.main_task_runner = base::ThreadTaskRunnerHandle::Get();
      params.task_graph_runner = &task_graph_runner_;
      params.mutator_host = animation_host_.get();

      layer_tree_host_ = cc::LayerTreeHost::CreateSingleThreaded(
          &layer_tree_host_single_thread_client_, &params);
      layer_tree_host_->SetRootLayer(
          GetDocument()
              .View()
              ->GetPaintArtifactCompositorForTesting()
              ->RootLayer());
    }
  }

  const DisplayItemClient* ViewScrollingContentsDisplayItemClient() const {
    return GetLayoutView().Layer()->GraphicsLayerBacking();
  }

 private:
  cc::StubLayerTreeHostSingleThreadClient layer_tree_host_single_thread_client_;
  cc::StubLayerTreeHostClient layer_tree_host_client_;
  cc::TestTaskGraphRunner task_graph_runner_;
  std::unique_ptr<cc::AnimationHost> animation_host_;
  std::unique_ptr<cc::LayerTreeHost> layer_tree_host_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_AND_RASTER_INVALIDATION_TEST_H_
