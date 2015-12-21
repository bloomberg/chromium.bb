// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/LengthPairStyleInterpolation.h"

#include "core/animation/LengthStyleInterpolation.h"
#include "core/css/CSSValuePair.h"
#include "core/css/resolver/StyleBuilder.h"

namespace blink {

bool LengthPairStyleInterpolation::canCreateFrom(const CSSValue& value)
{
    return value.isValuePair();
}

PassOwnPtr<InterpolableValue> LengthPairStyleInterpolation::lengthPairToInterpolableValue(const CSSValue& value)
{
    OwnPtr<InterpolableList> result = InterpolableList::create(2);
    const CSSValuePair& pair = toCSSValuePair(value);

    result->set(0, LengthStyleInterpolation::toInterpolableValue(pair.first()));
    result->set(1, LengthStyleInterpolation::toInterpolableValue(pair.second()));
    return result.release();
}

PassRefPtrWillBeRawPtr<CSSValue> LengthPairStyleInterpolation::interpolableValueToLengthPair(InterpolableValue* value, InterpolationRange range)
{
    InterpolableList* lengthPair = toInterpolableList(value);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> first = LengthStyleInterpolation::fromInterpolableValue(*lengthPair->get(0), range);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> second = LengthStyleInterpolation::fromInterpolableValue(*lengthPair->get(1), range);
    return CSSValuePair::create(first.release(), second.release(), CSSValuePair::KeepIdenticalValues);
}

void LengthPairStyleInterpolation::apply(StyleResolverState& state) const
{
    StyleBuilder::applyProperty(m_id, state, interpolableValueToLengthPair(m_cachedValue.get(), m_range).get());
}

} // namespace blink
