/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrQuad.h"

#include "GrTypesPriv.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// Functions for identifying the quad type from its coordinates, which are kept debug-only since
// production code should rely on the matrix to derive the quad type more efficiently. These are
// useful in asserts that the quad type is as expected.
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef SK_DEBUG
// Allow some tolerance from floating point matrix transformations, but SkScalarNearlyEqual doesn't
// support comparing infinity, and coords_form_rect should return true for infinite edges
#define NEARLY_EQUAL(f1, f2) (f1 == f2 || SkScalarNearlyEqual(f1, f2, 1e-5f))
// Similarly, support infinite rectangles by looking at the sign of infinities
static bool dot_nearly_zero(const SkVector& e1, const SkVector& e2) {
    static constexpr auto dot = SkPoint::DotProduct;
    static constexpr auto sign = SkScalarSignAsScalar;

    SkScalar dotValue = dot(e1, e2);
    if (SkScalarIsNaN(dotValue)) {
        // Form vectors from the signs of infinities, and check their dot product
        dotValue = dot({sign(e1.fX), sign(e1.fY)}, {sign(e2.fX), sign(e2.fY)});
    }

    return SkScalarNearlyZero(dotValue, 1e-3f);
}

// This is not the most performance critical function; code using GrQuad should rely on the faster
// quad type from matrix path, so this will only be called as part of SkASSERT.
static bool coords_form_rect(const float xs[4], const float ys[4]) {
    return (NEARLY_EQUAL(xs[0], xs[1]) && NEARLY_EQUAL(xs[2], xs[3]) &&
            NEARLY_EQUAL(ys[0], ys[2]) && NEARLY_EQUAL(ys[1], ys[3])) ||
           (NEARLY_EQUAL(xs[0], xs[2]) && NEARLY_EQUAL(xs[1], xs[3]) &&
            NEARLY_EQUAL(ys[0], ys[1]) && NEARLY_EQUAL(ys[2], ys[3]));
}

static bool coords_rectilinear(const float xs[4], const float ys[4]) {
    SkVector e0{xs[1] - xs[0], ys[1] - ys[0]}; // connects to e1 and e2(repeat)
    SkVector e1{xs[3] - xs[1], ys[3] - ys[1]}; // connects to e0(repeat) and e3
    SkVector e2{xs[0] - xs[2], ys[0] - ys[2]}; // connects to e0 and e3(repeat)
    SkVector e3{xs[2] - xs[3], ys[2] - ys[3]}; // connects to e1(repeat) and e2

    e0.normalize();
    e1.normalize();
    e2.normalize();
    e3.normalize();

    return dot_nearly_zero(e0, e1) && dot_nearly_zero(e1, e3) &&
           dot_nearly_zero(e2, e0) && dot_nearly_zero(e3, e2);
}

GrQuadType GrQuad::quadType() const {
    // Since GrQuad applies any perspective information at construction time, there's only two
    // types to choose from.
    if (coords_form_rect(fX, fY)) {
        return GrQuadType::kRect;
    } else if (coords_rectilinear(fX, fY)) {
        return GrQuadType::kRectilinear;
    } else {
        return GrQuadType::kStandard;
    }
}

GrQuadType GrPerspQuad::quadType() const {
    if (this->hasPerspective()) {
        return GrQuadType::kPerspective;
    } else {
        // Rect or standard quad, can ignore w since they are all ones
        if (coords_form_rect(fX, fY)) {
            return GrQuadType::kRect;
        } else if (coords_rectilinear(fX, fY)) {
            return GrQuadType::kRectilinear;
        } else {
            return GrQuadType::kStandard;
        }
    }
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////

static bool aa_affects_rect(float ql, float qt, float qr, float qb) {
    return !SkScalarIsInt(ql) || !SkScalarIsInt(qr) || !SkScalarIsInt(qt) || !SkScalarIsInt(qb);
}

static void map_rect_translate_scale(const SkRect& rect, const SkMatrix& m,
                                     Sk4f* xs, Sk4f* ys) {
    SkMatrix::TypeMask tm = m.getType();
    SkASSERT(tm <= (SkMatrix::kScale_Mask | SkMatrix::kTranslate_Mask));

    Sk4f r = Sk4f::Load(&rect);
    if (tm > SkMatrix::kIdentity_Mask) {
        const Sk4f t(m.getTranslateX(), m.getTranslateY(), m.getTranslateX(), m.getTranslateY());
        if (tm <= SkMatrix::kTranslate_Mask) {
            r += t;
        } else {
            const Sk4f s(m.getScaleX(), m.getScaleY(), m.getScaleX(), m.getScaleY());
            r = r * s + t;
        }
    }
    *xs = SkNx_shuffle<0, 0, 2, 2>(r);
    *ys = SkNx_shuffle<1, 3, 1, 3>(r);
}

static void map_quad_general(const Sk4f& qx, const Sk4f& qy, const SkMatrix& m,
                             Sk4f* xs, Sk4f* ys, Sk4f* ws) {
    static constexpr auto fma = SkNx_fma<4, float>;
    *xs = fma(m.getScaleX(), qx, fma(m.getSkewX(), qy, m.getTranslateX()));
    *ys = fma(m.getSkewY(), qx, fma(m.getScaleY(), qy, m.getTranslateY()));
    if (m.hasPerspective()) {
        Sk4f w = fma(m.getPerspX(), qx, fma(m.getPerspY(), qy, m.get(SkMatrix::kMPersp2)));
        if (ws) {
            // Output the calculated w coordinates
            *ws = w;
        } else {
            // Apply perspective division immediately
            Sk4f iw = w.invert();
            *xs *= iw;
            *ys *= iw;
        }
    } else if (ws) {
        *ws = 1.f;
    }
}

static void map_rect_general(const SkRect& rect, const SkMatrix& matrix,
                             Sk4f* xs, Sk4f* ys, Sk4f* ws) {
    Sk4f rx(rect.fLeft, rect.fLeft, rect.fRight, rect.fRight);
    Sk4f ry(rect.fTop, rect.fBottom, rect.fTop, rect.fBottom);
    map_quad_general(rx, ry, matrix, xs, ys, ws);
}

// Rearranges (top-left, top-right, bottom-right, bottom-left) ordered skQuadPts into xs and ys
// ordered (top-left, bottom-left, top-right, bottom-right)
static void rearrange_sk_to_gr_points(const SkPoint skQuadPts[4], Sk4f* xs, Sk4f* ys) {
    *xs = Sk4f(skQuadPts[0].fX, skQuadPts[3].fX, skQuadPts[1].fX, skQuadPts[2].fX);
    *ys = Sk4f(skQuadPts[0].fY, skQuadPts[3].fY, skQuadPts[1].fY, skQuadPts[2].fY);
}

template <typename Q>
void GrResolveAATypeForQuad(GrAAType requestedAAType, GrQuadAAFlags requestedEdgeFlags,
                            const Q& quad, GrQuadType knownType,
                            GrAAType* outAAType, GrQuadAAFlags* outEdgeFlags) {
    // Most cases will keep the requested types unchanged
    *outAAType = requestedAAType;
    *outEdgeFlags = requestedEdgeFlags;

    switch (requestedAAType) {
        // When aa type is coverage, disable AA if the edge configuration doesn't actually need it
        case GrAAType::kCoverage:
            if (requestedEdgeFlags == GrQuadAAFlags::kNone) {
                // Turn off anti-aliasing
                *outAAType = GrAAType::kNone;
            } else {
                // For coverage AA, if the quad is a rect and it lines up with pixel boundaries
                // then overall aa and per-edge aa can be completely disabled
                if (knownType == GrQuadType::kRect && !quad.aaHasEffectOnRect()) {
                    *outAAType = GrAAType::kNone;
                    *outEdgeFlags = GrQuadAAFlags::kNone;
                }
            }
            break;
        // For no or msaa anti aliasing, override the edge flags since edge flags only make sense
        // when coverage aa is being used.
        case GrAAType::kNone:
            *outEdgeFlags = GrQuadAAFlags::kNone;
            break;
        case GrAAType::kMSAA:
            *outEdgeFlags = GrQuadAAFlags::kAll;
            break;
        case GrAAType::kMixedSamples:
            SK_ABORT("Should not use mixed sample AA with edge AA flags");
            break;
    }
};

// Instantiate GrResolve... for GrQuad and GrPerspQuad
template void GrResolveAATypeForQuad(GrAAType, GrQuadAAFlags, const GrQuad&, GrQuadType,
                                     GrAAType*, GrQuadAAFlags*);
template void GrResolveAATypeForQuad(GrAAType, GrQuadAAFlags, const GrPerspQuad&, GrQuadType,
                                     GrAAType*, GrQuadAAFlags*);

GrQuadType GrQuadTypeForTransformedRect(const SkMatrix& matrix) {
    if (matrix.rectStaysRect()) {
        return GrQuadType::kRect;
    } else if (matrix.preservesRightAngles()) {
        return GrQuadType::kRectilinear;
    } else if (matrix.hasPerspective()) {
        return GrQuadType::kPerspective;
    } else {
        return GrQuadType::kStandard;
    }
}

GrQuad GrQuad::MakeFromRect(const SkRect& rect, const SkMatrix& m) {
    Sk4f x, y;
    SkMatrix::TypeMask tm = m.getType();
    if (tm <= (SkMatrix::kScale_Mask | SkMatrix::kTranslate_Mask)) {
        map_rect_translate_scale(rect, m, &x, &y);
    } else {
        map_rect_general(rect, m, &x, &y, nullptr);
    }
    return GrQuad(x, y);
}

GrQuad GrQuad::MakeFromSkQuad(const SkPoint pts[4], const SkMatrix& matrix) {
    Sk4f xs, ys;
    rearrange_sk_to_gr_points(pts, &xs, &ys);
    if (matrix.isIdentity()) {
        return GrQuad(xs, ys);
    } else {
        Sk4f mx, my;
        map_quad_general(xs, ys, matrix, &mx, &my, nullptr);
        return GrQuad(mx, my);
    }
}

bool GrQuad::aaHasEffectOnRect() const {
    SkASSERT(this->quadType() == GrQuadType::kRect);
    return aa_affects_rect(fX[0], fY[0], fX[3], fY[3]);
}

// Private constructor used by GrQuadList to quickly fill in a quad's values from the channel arrays
GrPerspQuad::GrPerspQuad(const float* xs, const float* ys, const float* ws) {
    memcpy(fX, xs, 4 * sizeof(float));
    memcpy(fY, ys, 4 * sizeof(float));
    memcpy(fW, ws, 4 * sizeof(float));
}

GrPerspQuad GrPerspQuad::MakeFromRect(const SkRect& rect, const SkMatrix& m) {
    Sk4f x, y, w;
    SkMatrix::TypeMask tm = m.getType();
    if (tm <= (SkMatrix::kScale_Mask | SkMatrix::kTranslate_Mask)) {
        map_rect_translate_scale(rect, m, &x, &y);
        w = 1.f;
    } else {
        map_rect_general(rect, m, &x, &y, &w);
    }
    return GrPerspQuad(x, y, w);
}

GrPerspQuad GrPerspQuad::MakeFromSkQuad(const SkPoint pts[4], const SkMatrix& matrix) {
    Sk4f xs, ys;
    rearrange_sk_to_gr_points(pts, &xs, &ys);
    if (matrix.isIdentity()) {
        return GrPerspQuad(xs, ys, 1.f);
    } else {
        Sk4f mx, my, mw;
        map_quad_general(xs, ys, matrix, &mx, &my, &mw);
        return GrPerspQuad(mx, my, mw);
    }
}

bool GrPerspQuad::aaHasEffectOnRect() const {
    SkASSERT(this->quadType() == GrQuadType::kRect);
    // If rect, ws must all be 1s so no need to divide
    return aa_affects_rect(fX[0], fY[0], fX[3], fY[3]);
}
