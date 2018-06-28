// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_LAYER_TREE_HOST_FOR_PAINT_ARTIFACT_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_LAYER_TREE_HOST_FOR_PAINT_ARTIFACT_TEST_H_

#include "cc/animation/animation_host.h"
#include "cc/test/stub_layer_tree_host_client.h"
#include "cc/test/stub_layer_tree_host_single_thread_client.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"

namespace blink {

class LayerTreeHostForPaintArtifactTest {
 public:
  LayerTreeHostForPaintArtifactTest();

  cc::LayerTreeHost* layer_tree_host() { return layer_tree_host_.get(); }

 private:
  cc::StubLayerTreeHostSingleThreadClient layer_tree_host_single_thread_client_;
  cc::StubLayerTreeHostClient layer_tree_host_client_;
  cc::TestTaskGraphRunner task_graph_runner_;
  std::unique_ptr<cc::AnimationHost> animation_host_;
  std::unique_ptr<cc::LayerTreeHost> layer_tree_host_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_LAYER_TREE_HOST_FOR_PAINT_ARTIFACT_TEST_H_
