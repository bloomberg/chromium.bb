// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/SVGStrokeDasharrayStyleInterpolation.h"

#include "core/animation/SVGLengthStyleInterpolation.h"
#include "core/css/CSSValueList.h"
#include "core/css/resolver/StyleBuilder.h"

#include "wtf/MathExtras.h"

namespace blink {

namespace {

bool isNone(const CSSValue& value)
{
    if (!value.isPrimitiveValue())
        return false;
    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    return primitiveValue.isValueID() && primitiveValue.getValueID() == CSSValueNone;
}

} // namespace

PassRefPtrWillBeRawPtr<CSSValueList> SVGStrokeDasharrayStyleInterpolation::interpolableValueToStrokeDasharray(const InterpolableValue& interpolableValue, const Vector<CSSPrimitiveValue::UnitType>& types)
{
    const InterpolableList& interpolableList = toInterpolableList(interpolableValue);
    ASSERT(types.size() == interpolableList.length());

    RefPtrWillBeRawPtr<CSSValueList> ret = CSSValueList::createCommaSeparated();
    for (size_t index = 0; index < interpolableList.length(); ++index) {
        ret->append(SVGLengthStyleInterpolation::interpolableValueToLength(*interpolableList.get(index), types.at(index), RangeNonNegative));
    }
    return ret.release();
}

bool SVGStrokeDasharrayStyleInterpolation::canCreateFrom(const CSSValue& value)
{
    if (!value.isValueList())
        return isNone(value);
    const CSSValueList& valueList = toCSSValueList(value);

    for (size_t index = 0; index < valueList.length(); ++index) {
        if (!SVGLengthStyleInterpolation::canCreateFrom(*valueList.item(index)))
            return false;
    }
    return true;
}

PassRefPtrWillBeRawPtr<SVGStrokeDasharrayStyleInterpolation> SVGStrokeDasharrayStyleInterpolation::maybeCreate(const CSSValue& start, const CSSValue& end, CSSPropertyID id)
{
    if (!canCreateFrom(start) || !canCreateFrom(end))
        return nullptr;

    RefPtrWillBeRawPtr<CSSValueList> singleZero = CSSValueList::createCommaSeparated();
    singleZero->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PX));

    const CSSValueList& valueListStart = start.isValueList() ? toCSSValueList(start) : *singleZero;
    const CSSValueList& valueListEnd = end.isValueList() ? toCSSValueList(end) : *singleZero;
    size_t size = lowestCommonMultiple(valueListStart.length(), valueListEnd.length());
    ASSERT(size > 0);

    Vector<CSSPrimitiveValue::UnitType> types(size);
    OwnPtrWillBeRawPtr<InterpolableList> interpolableStart = InterpolableList::create(size);
    OwnPtrWillBeRawPtr<InterpolableList> interpolableEnd = InterpolableList::create(size);

    for (size_t i = 0; i < size; ++i) {
        const CSSPrimitiveValue& from = *toCSSPrimitiveValue(valueListStart.item(i % valueListStart.length()));
        const CSSPrimitiveValue& to = *toCSSPrimitiveValue(valueListEnd.item(i % valueListEnd.length()));

        // Spec: If a pair of values cannot be interpolated, then the lists are not interpolable.
        types[i] = SVGLengthStyleInterpolation::commonUnitType(from, to);
        if (types[i] == CSSPrimitiveValue::CSS_UNKNOWN)
            return nullptr;
        interpolableStart->set(i, SVGLengthStyleInterpolation::lengthToInterpolableValue(from));
        interpolableEnd->set(i, SVGLengthStyleInterpolation::lengthToInterpolableValue(to));
    }
    return adoptRefWillBeNoop(new SVGStrokeDasharrayStyleInterpolation(interpolableStart.release(), interpolableEnd.release(), id, types));
}

void SVGStrokeDasharrayStyleInterpolation::apply(StyleResolverState& state) const
{
    StyleBuilder::applyProperty(m_id, state, interpolableValueToStrokeDasharray(*m_cachedValue, m_types).get());
}

}
