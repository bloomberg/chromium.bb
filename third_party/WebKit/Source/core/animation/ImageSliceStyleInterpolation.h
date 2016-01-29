// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageSliceStyleInterpolation_h
#define ImageSliceStyleInterpolation_h

#include "core/animation/StyleInterpolation.h"

namespace blink {

class CSSBorderImageSliceValue;

class ImageSliceStyleInterpolation : public StyleInterpolation {
public:
    static bool usesDefaultInterpolation(const CSSValue&, const CSSValue&);
    static PassRefPtr<ImageSliceStyleInterpolation> maybeCreate(const CSSValue&, const CSSValue&, CSSPropertyID);

    void apply(StyleResolverState&) const override;

    // Image slices can have either percentages or numbers: http://dev.w3.org/csswg/css-backgrounds-3/#the-border-image-slice
    struct Metadata {
        bool isPercentage;
        bool fill;
        bool operator==(const Metadata& o) const { return isPercentage == o.isPercentage && fill == o.fill; }
    };

private:
    ImageSliceStyleInterpolation(PassOwnPtr<InterpolableValue> start, PassOwnPtr<InterpolableValue> end, CSSPropertyID property, Metadata metadata)
        : StyleInterpolation(std::move(start), std::move(end), property)
        , m_metadata(metadata)
    { }

    Metadata m_metadata;
};

} // namespace blink

#endif // ImageSliceStyleInterpolation_h
