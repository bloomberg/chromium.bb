// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define the interface for a generic simulation, and a factory method for
// instantiating different models.

#ifndef GPU_TOOLS_COMPOSITOR_MODEL_BENCH_RENDER_MODELS_H_
#define GPU_TOOLS_COMPOSITOR_MODEL_BENCH_RENDER_MODELS_H_

#include <memory>

#include "base/macros.h"
#include "gpu/tools/compositor_model_bench/render_tree.h"

enum RenderModel {
  ForwardRenderModel
};

const char* ModelToString(RenderModel m);

class RenderModelSimulator {
 public:
  virtual ~RenderModelSimulator();
  virtual void Update() = 0;
  virtual void Resize(int width, int height) = 0;

 protected:
  explicit RenderModelSimulator(std::unique_ptr<RenderNode> root);

  std::unique_ptr<RenderNode> root_;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(RenderModelSimulator);
};

std::unique_ptr<RenderModelSimulator> ConstructSimulationModel(
    RenderModel model,
    std::unique_ptr<RenderNode> render_tree_root,
    int window_width,
    int window_height);

#endif  // GPU_TOOLS_COMPOSITOR_MODEL_BENCH_RENDER_MODELS_H_

