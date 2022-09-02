/*
 * Copyright 2022 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skgpu_graphite_render_CoverBoundsRenderStep_DEFINED
#define skgpu_graphite_render_CoverBoundsRenderStep_DEFINED

#include "src/gpu/graphite/Renderer.h"

namespace skgpu::graphite {

class CoverBoundsRenderStep final : public RenderStep {
public:
    CoverBoundsRenderStep(bool inverseFill);

    ~CoverBoundsRenderStep() override;

    const char* vertexSkSL() const override;
    void writeVertices(DrawWriter*, const DrawGeometry&) const override;
    void writeUniforms(const DrawGeometry&, SkPipelineDataGatherer*) const override;

private:
    const bool fInverseFill;
};

}  // namespace skgpu::graphite

#endif // skgpu_render_CoverBoundsRenderStep_DEFINED
