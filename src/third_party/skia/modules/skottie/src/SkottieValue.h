/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkottieValue_DEFINED
#define SkottieValue_DEFINED

#include "SkColor.h"
#include "SkPaint.h"
#include "SkPath.h"
#include "SkScalar.h"
#include "SkString.h"
#include "SkTextUtils.h"
#include "SkTypeface.h"

#include <vector>

namespace skjson { class Value; }

namespace skottie {
namespace internal {
class AnimationBuilder;
} // namespace internal

template <typename T>
struct ValueTraits {
    static bool FromJSON(const skjson::Value&, const internal::AnimationBuilder*, T*);

    template <typename U>
    static U As(const T&);

    static bool CanLerp(const T&, const T&);
    static void Lerp(const T&, const T&, float, T*);
};

using ScalarValue = SkScalar;
using VectorValue = std::vector<ScalarValue>;

struct BezierVertex {
    SkPoint fInPoint,  // "in" control point, relative to the vertex
            fOutPoint, // "out" control point, relative to the vertex
            fVertex;

    bool operator==(const BezierVertex& other) const {
        return fInPoint  == other.fInPoint
            && fOutPoint == other.fOutPoint
            && fVertex   == other.fVertex;
    }

    bool operator!=(const BezierVertex& other) const { return !(*this == other); }
};

struct ShapeValue {
    std::vector<BezierVertex> fVertices;
    bool                      fClosed   : 1,
                              fVolatile : 1;

    ShapeValue() : fClosed(false), fVolatile(false) {}
    ShapeValue(const ShapeValue&)            = default;
    ShapeValue(ShapeValue&&)                 = default;
    ShapeValue& operator=(const ShapeValue&) = default;

    bool operator==(const ShapeValue& other) const {
        return fVertices == other.fVertices && fClosed == other.fClosed;
    }

    bool operator!=(const ShapeValue& other) const { return !(*this == other); }
};

struct TextValue {
    sk_sp<SkTypeface> fTypeface;
    SkString          fText;
    float             fTextSize    = 0,
                      fStrokeWidth = 0;
    SkTextUtils::Align fAlign       = SkTextUtils::kLeft_Align;
    SkColor           fFillColor   = SK_ColorTRANSPARENT,
                      fStrokeColor = SK_ColorTRANSPARENT;
    bool              fHasFill   : 1,
                      fHasStroke : 1;

    bool operator==(const TextValue& other) const {
        return fTypeface == other.fTypeface
            && fText == other.fText
            && fTextSize == other.fTextSize
            && fStrokeWidth == other.fStrokeWidth
            && fAlign == other.fAlign
            && fFillColor == other.fFillColor
            && fStrokeColor == other.fStrokeColor
            && fHasFill == other.fHasFill
            && fHasStroke == other.fHasStroke;
    }

    bool operator!=(const TextValue& other) const { return !(*this == other); }
};

} // namespace skottie

#endif // SkottieValue_DEFINED
