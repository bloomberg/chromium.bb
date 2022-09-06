/*
 * Copyright 2022 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skgpu_graphite_render_TessellateStrokesRenderStep_DEFINED
#define skgpu_graphite_render_TessellateStrokesRenderStep_DEFINED

#include "src/gpu/graphite/Renderer.h"

namespace skgpu::graphite {

class TessellateStrokesRenderStep final : public RenderStep {
public:
    // TODO: If this takes DepthStencilSettings directly and a way to adjust the flags to specify
    // that it performs shading, this RenderStep definition could be used to handle inverse-filled
    // stroke draws.
    TessellateStrokesRenderStep();

    ~TessellateStrokesRenderStep() override;

    const char* vertexSkSL() const override;
    void writeVertices(DrawWriter*, const DrawGeometry&) const override;
    void writeUniforms(const DrawGeometry&, SkPipelineDataGatherer*) const override;
};

}  // namespace skgpu::graphite

#endif // skgpu_graphite_render_TessellateStrokesRenderStep_DEFINED
