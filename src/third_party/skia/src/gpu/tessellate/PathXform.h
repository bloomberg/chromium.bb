/*
 * Copyright 2021 Google LLC.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef tessellate_PathXform_DEFINED
#define tessellate_PathXform_DEFINED

#include "include/core/SkMatrix.h"
#include "src/gpu/BufferWriter.h"
#include "src/gpu/tessellate/Tessellation.h"

namespace skgpu {

// Applies an affine 2d transformation to points and path components. Converts path components to
// tessellation patches. Uses SIMD, but takes care to map points identically, regardless of which
// method is called.
//
// This class stores redundant data, so it is best used only as a stack-allocated object at the
// point of use.
class PathXform {
public:
    PathXform() = default;
    PathXform(const SkMatrix& m) { *this = m; }

    PathXform& operator=(const SkMatrix& m) {
        SkASSERT(!m.hasPerspective());
        // Duplicate the matrix in float4.lo and float4.hi so we can map two points at once.
        fScale = {m.getScaleX(), m.getScaleY(), m.getScaleX(), m.getScaleY()};
        fSkew = {m.getSkewX(), m.getSkewY(), m.getSkewX(), m.getSkewY()};
        fTrans = {m.getTranslateX(), m.getTranslateY(), m.getTranslateX(), m.getTranslateY()};
        return *this;
    }

    SK_ALWAYS_INLINE float2 mapPoint(float2 p) const {
        return fScale.lo * p + (fSkew.lo * skvx::shuffle<1,0>(p) + fTrans.lo);
    }

    SK_ALWAYS_INLINE float4 map2Points(float4 p0p1) const {
        return fScale * p0p1 + (fSkew * p0p1.yxwz() + fTrans);
    }

    SK_ALWAYS_INLINE float2 map1Point(const SkPoint pt[1]) const {
        return this->mapPoint(float2::Load(pt));
    }

    SK_ALWAYS_INLINE float4 map2Points(const SkPoint pts[2]) const {
        return this->map2Points(float4::Load(pts));
    }

    SK_ALWAYS_INLINE SkPoint mapPoint(SkPoint p) const {
        return skvx::bit_pun<SkPoint>(this->mapPoint(skvx::bit_pun<float2>(p)));
    }

    SK_ALWAYS_INLINE float4 map2Points(SkPoint p0, SkPoint p1) const {
        return this->map2Points(float4(skvx::bit_pun<float2>(p0), skvx::bit_pun<float2>(p1)));
    }

    SK_ALWAYS_INLINE void map3Points(VertexWriter* writer, const SkPoint pts[3]) const {
        *writer << this->map2Points(pts);
        *writer << this->map1Point(pts + 2);
    }

    SK_ALWAYS_INLINE void map4Points(VertexWriter* writer, const SkPoint pts[4]) const {
        *writer << this->map2Points(pts);
        *writer << this->map2Points(pts + 2);
    }

    // Emits a degenerate, 4-point transformed cubic bezier equal to a line.
    SK_ALWAYS_INLINE void mapLineToCubic(VertexWriter* writer,
                                         SkPoint startPt,
                                         SkPoint endPt) const {
        float2 p0 = this->mapPoint(skvx::bit_pun<float2>(startPt));
        float2 p1 = this->mapPoint(skvx::bit_pun<float2>(endPt));
        float2 v = (p1 - p0) * (1/3.f);
        *writer << p0 << (p0 + v) << (p1 - v) << p1;
    }

    // Emits a degenerate, 4-point transformed bezier equal to a quadratic.
    SK_ALWAYS_INLINE void mapQuadToCubic(VertexWriter* writer, const SkPoint pts[3]) const {
        float2 p0 = this->mapPoint(skvx::bit_pun<float2>(pts[0]));
        float2 p1 = this->mapPoint(skvx::bit_pun<float2>(pts[1]));
        float2 p2 = this->mapPoint(skvx::bit_pun<float2>(pts[2]));
        float2 c = p1 * (2/3.f);
        *writer << p0 << (p0 * 1/3.f + c) << (p2 * 1/3.f + c) << p2;
    }

    // Writes out the 3 conic points transformed, plus a 4th point with the conic weight in x and
    // infinity in y. Infinite y flags the 4-point patch as a conic.
    SK_ALWAYS_INLINE void mapConicToPatch(VertexWriter* writer,
                                          const SkPoint pts[3],
                                          float w) const {
        this->map3Points(writer, pts);
        *writer << w << VertexWriter::kIEEE_32_infinity;
    }

private:
    float4 fScale;
    float4 fSkew;
    float4 fTrans;
};

}  // namespace skgpu

#endif  // tessellate_PathXform_DEFINED
