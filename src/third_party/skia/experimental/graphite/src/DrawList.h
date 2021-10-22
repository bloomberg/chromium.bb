/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skgpu_DrawList_DEFINED
#define skgpu_DrawList_DEFINED

#include "include/core/SkColor.h"
#include "include/core/SkPaint.h"
#include "include/core/SkShader.h"

#include "experimental/graphite/include/GraphiteTypes.h"

#include <cstdint>

class SkM44;
class SkPath;
struct SkIRect;

namespace skgpu {

// Forward declarations that capture the intermediate state lying between public Skia types and
// the direct GPU representation.
struct PaintParams;
struct StrokeParams;

/**
 * A DrawList represents a collection of drawing commands (and related clip/shading state) in
 * a form that closely mirrors what can be rendered efficiently and directly by the GPU backend
 * (while balancing how much pre-processing to do for draws that might get eliminated later due to
 * occlusion culling).
 *
 * A draw command combines:
 *   - a shape
 *   - a transform
 *   - a primitive clip (not affected by the transform)
 *   - optional shading description (shader, color filter, blend mode, etc)
 *   - a sorting z
 *   - a write/test z
 *
 * Commands are accumulated in an arbitrary order and then sorted by increasing sort z when the list
 * is prepared into an actual command buffer. The result of a draw command is the rasterization of
 * the transformed shape, restricted by its primitive clip (e.g. a scissor rect) and a depth test
 * of "GREATER" vs. its write/test z. (A test of GREATER, as opposed to GEQUAL, avoids double hits
 * for draws that may have overlapping geometry, e.g. stroking.) If the command has a shading
 * description, the color buffer will be modified; if not, it will be a depth-only draw.
 *
 * In addition to sorting the collected commands, the command list can be optimized during
 * preparation. Commands that are fully occluded by later operations can be skipped entirely without
 * affecting the final results. Adjacent commands (post sort) that would use equivalent GPU
 * pipelines are merged to produce fewer (but larger) operations on the GPU.
 *
 * Other than flush-time optimizations (sort, cull, and merge), the command list does what you tell
 * it to. Draw-specific simplification, style application, and advanced clipping should be handled
 * at a higher layer.
 */
class DrawList {
public:
    // TBD: Do we always need the inverse deviceToLocal matrix? If not the entire matrix, do we need
    // some other matrix-dependent value (e.g. scale factor) frequently? Since the localToDevice
    // transform from the Device changes at the same or slower rate as draw commands, it may make
    // sense for it to compute these dependent values and provide them here. Storing the scale
    // factor per draw command is low overhead, but unsure about storing 2 matrices per command.

    void stencilAndFillPath(const SkM44& localToDevice,
                            const SkPath& path,
                            const SkIRect& scissor, // TBD: expand this to one xformed rrect clip?
                            CompressedPaintersOrder colorDepthOrder,
                            CompressedPaintersOrder stencilOrder,
                            uint16_t depth,
                            const PaintParams* paint) {}

    void fillConvexPath(const SkM44& localToDevice,
                        const SkPath& path,
                        const SkIRect& scissor,
                        CompressedPaintersOrder colorDepthOrder,
                        uint16_t depth,
                        const PaintParams* paint) {}

    void strokePath(const SkM44& localToDevice,
                    const SkPath& path,
                    const StrokeParams& stroke,
                    const SkIRect& scissor,
                    CompressedPaintersOrder colorDepthOrder,
                    uint16_t depth,
                    const PaintParams* paint) {}

    // TODO: fill[R]Rect, stroke[R]Rect (will need to support per-edge aa and arbitrary quads)
    //       fillImage (per-edge aa and arbitrary quad, only if this fast path is required)
    //       dashPath(feasible for general paths?)
    //       dash[R]Rect(only if general dashPath isn't viable)
    //       dashLine(only if general or rrect version aren't viable)

    int count() const { return 0; }

    // TBD: Figure out preparation/flush APIs and/or how to iterate the draw commands. These will
    // be responsible for sorting by sort z and shading state, doing occlusion culling, and possibly
    // merging compatible, consecutive remaining commands. It can also easily track if there are
    // remaining depth-only draws or complex path draws that would trigger DMSAA. I[ml] can see this
    // all being internal to DrawCommandList, or being supported by accessors and iterators with the
    // rest of the logic stored in SDC. It is also unknown at this time how much conversion the
    // PaintParams will need to go through (vs. just building a key) in order to do state sorting.

    // TBD: Any value in de-duplicating paint params/programs during accumulation or being able
    // to query the set of required programs for a given command list? Any time query or flush time?

private:
    // TODO: actually implement this, probably stl for now but will likely need something that
    // is allocation efficient. Should also explore having 1 vector of commands, vs. parallel
    // vectors of various fields.
};

// TBD: If occlusion culling is eliminated as a phase, we can easily move the paint conversion
// back to Device when the command is recorded (similar to SkPaint -> GrPaint), and then
// PaintParams is not required as an intermediate representation.
// NOTE: Only represents the shading state of an SkPaint. Style and complex effects (mask filters,
// image filters, path effects) must be handled higher up. AA is not tracked since everything is
// assumed to be anti-aliased.
struct PaintParams {
    SkColor4f       fColor;
    SkBlendMode     fBlendMode;
    sk_sp<SkShader> fShader; // For now only use SkShader::asAGradient() when converting to GPU
    // TODO: Will also store ColorFilter, custom Blender, dither, and any extra shader from an
    // active clipShader().
};

// NOTE: Only represents the stroke style; stroke-and-fill and hairline must be handled higher up.
struct StrokeParams {
    float         fWidth; // > 0 and relative to shape's transform
    float         fMiterLimit;
    SkPaint::Join fJoin;
    SkPaint::Cap  fCap;
};

// TBD: Separate DashParams extracted from an SkDashPathEffect? Or folded into StrokeParams?

} // namespace skgpu

#endif // skgpu_DrawList_DEFINED
