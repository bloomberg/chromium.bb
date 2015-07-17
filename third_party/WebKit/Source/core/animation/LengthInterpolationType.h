// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LengthInterpolationType_h
#define LengthInterpolationType_h

#include "core/animation/InterpolationType.h"
#include "core/animation/LengthPropertyFunctions.h"

namespace blink {

class LengthInterpolationType : public InterpolationType {
public:
    LengthInterpolationType(CSSPropertyID);

    PassOwnPtrWillBeRawPtr<InterpolationValue> maybeConvertUnderlyingValue(const StyleResolverState&) const final;
    void apply(const InterpolableValue&, const NonInterpolableValue*, StyleResolverState&) const final;

private:
    PassOwnPtrWillBeRawPtr<InterpolationValue> maybeConvertLength(const Length&, float zoom) const;
    PassOwnPtrWillBeRawPtr<InterpolationValue> maybeConvertNeutral() const final;
    PassOwnPtrWillBeRawPtr<InterpolationValue> maybeConvertInitial() const final;
    PassOwnPtrWillBeRawPtr<InterpolationValue> maybeConvertInherit(const StyleResolverState*, ConversionCheckers&) const final;
    PassOwnPtrWillBeRawPtr<InterpolationValue> maybeConvertValue(const CSSValue&, const StyleResolverState*, ConversionCheckers&) const final;

    const ValueRange m_valueRange;
};

} // namespace blink

#endif // LengthInterpolationType_h
