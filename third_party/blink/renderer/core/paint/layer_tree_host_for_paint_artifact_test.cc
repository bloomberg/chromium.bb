// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/layer_tree_host_for_paint_artifact_test.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_thread.h"

namespace blink {

LayerTreeHostForPaintArtifactTest::LayerTreeHostForPaintArtifactTest() {
  cc::LayerTreeSettings settings;
  settings.layer_transforms_should_scale_layer_contents = true;

  animation_host_ = cc::AnimationHost::CreateMainInstance();
  cc::LayerTreeHost::InitParams params;
  params.client = &layer_tree_host_client_;
  params.settings = &settings;
  params.main_task_runner =
      Platform::Current()->CurrentThread()->GetTaskRunner();
  params.task_graph_runner = &task_graph_runner_;
  params.mutator_host = animation_host_.get();

  layer_tree_host_ = cc::LayerTreeHost::CreateSingleThreaded(
      &layer_tree_host_single_thread_client_, &params);
}

}  // namespace blink
