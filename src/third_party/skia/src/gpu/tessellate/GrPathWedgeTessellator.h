/*
 * Copyright 2021 Google LLC.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrPathWedgeTessellator_DEFINED
#define GrPathWedgeTessellator_DEFINED

#include "src/gpu/GrVertexChunkArray.h"
#include "src/gpu/tessellate/GrPathTessellator.h"

class GrCaps;
class GrGpuBuffer;
class GrPipeline;

// Prepares an array of "wedge" patches for GrWedgeTessellateShader. A wedge is an independent,
// 5-point closed contour consisting of 4 control points plus an anchor point fanning from the
// center of the curve's resident contour. A wedge can be either a cubic or a conic. Quadratics and
// lines are converted to cubics. Once stencilled, these wedges alone define the complete path.
class GrPathWedgeTessellator : public GrPathTessellator {
public:
    // Creates a wedge tessellator with the shader type best suited for the given path description.
    static GrPathTessellator* Make(SkArenaAlloc*, const SkMatrix& viewMatrix, const SkPMColor4f&,
                                   int numPathVerbs, const GrPipeline&, const GrCaps&);

    void prepare(GrMeshDrawTarget*,
                 const SkRect& cullBounds,
                 const PathDrawList&,
                 int totalCombinedPathVerbCnt) override;


#if SK_GPU_V1
    void draw(GrOpFlushState*) const override;
#endif

private:
    GrPathWedgeTessellator(GrPathTessellationShader* shader)
            : GrPathTessellator(shader) {}

    GrVertexChunkArray fVertexChunkArray;

    // If using fixed count, this is the number of vertices we need to emit per instance.
    int fFixedIndexCount;
    sk_sp<const GrGpuBuffer> fFixedCountVertexBuffer;
    sk_sp<const GrGpuBuffer> fFixedCountIndexBuffer;
};

#endif
