// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSLengthListInterpolationType_h
#define CSSLengthListInterpolationType_h

#include "core/animation/CSSInterpolationType.h"
#include "core/animation/CSSLengthInterpolationType.h"
#include "platform/Length.h"
#include "wtf/RefVector.h"

namespace blink {

class CSSLengthListInterpolationType : public CSSInterpolationType {
public:
    CSSLengthListInterpolationType(CSSPropertyID);

    InterpolationValue maybeConvertUnderlyingValue(const InterpolationEnvironment&) const final;
    void composite(UnderlyingValueOwner&, double underlyingFraction, const InterpolationValue&) const final;
    void apply(const InterpolableValue&, const NonInterpolableValue*, InterpolationEnvironment&) const final;

private:
    InterpolationValue maybeConvertLengthList(const RefVector<Length>*, float zoom) const;

    InterpolationValue maybeConvertNeutral(const InterpolationValue& underlying, ConversionCheckers&) const final;
    InterpolationValue maybeConvertInitial() const final;
    InterpolationValue maybeConvertInherit(const StyleResolverState&, ConversionCheckers&) const final;
    InterpolationValue maybeConvertValue(const CSSValue&, const StyleResolverState&, ConversionCheckers&) const final;

    PairwiseInterpolationValue mergeSingleConversions(InterpolationValue& start, InterpolationValue& end) const final;

    const ValueRange m_valueRange;
};

} // namespace blink

#endif // CSSLengthListInterpolationType_h
