// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGTransformListInterpolationType_h
#define SVGTransformListInterpolationType_h

#include "core/animation/SVGInterpolationType.h"
#include "core/svg/SVGTransform.h"

namespace blink {

class SVGTransformListInterpolationType : public SVGInterpolationType {
public:
    SVGTransformListInterpolationType(const QualifiedName& attribute)
        : SVGInterpolationType(attribute)
    { }

private:
    InterpolationValue maybeConvertSVGValue(const SVGPropertyBase& svgValue) const final;
    InterpolationValue maybeConvertSingle(const PropertySpecificKeyframe&, const InterpolationEnvironment&, const InterpolationValue& underlying, ConversionCheckers&) const final;
    PassRefPtrWillBeRawPtr<SVGPropertyBase> appliedSVGValue(const InterpolableValue&, const NonInterpolableValue*) const final;

    PairwiseInterpolationValue mergeSingleConversions(InterpolationValue& start, InterpolationValue& end) const final;
    void composite(UnderlyingValueOwner&, double, const InterpolationValue&) const final;
};

} // namespace blink

#endif // SVGTransformListInterpolationType_h
