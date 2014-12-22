// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/LengthPoint3DStyleInterpolation.h"

#include "core/animation/LengthStyleInterpolation.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/resolver/StyleBuilder.h"

namespace blink {

bool LengthPoint3DStyleInterpolation::canCreateFrom(const CSSValue& value)
{
    if (!value.isValueList())
        return false;
    const CSSValueList& valueList = toCSSValueList(value);
    return valueList.length() == 3
        && toCSSPrimitiveValue(valueList.item(0))->isLength()
        && toCSSPrimitiveValue(valueList.item(1))->isLength()
        && toCSSPrimitiveValue(valueList.item(2))->isLength();
}

PassOwnPtrWillBeRawPtr<InterpolableValue> LengthPoint3DStyleInterpolation::lengthPoint3DtoInterpolableValue(const CSSValue& value)
{
    const int sizeOfList = 3;

    OwnPtrWillBeRawPtr<InterpolableList> result = InterpolableList::create(sizeOfList);
    ASSERT(value.isValueList());
    const CSSValueList& valueList = toCSSValueList(value);

    const CSSPrimitiveValue* length[sizeOfList] =
        { toCSSPrimitiveValue(valueList.item(0))
        , toCSSPrimitiveValue(valueList.item(1))
        , toCSSPrimitiveValue(valueList.item(2)) };

    for (size_t i = 0; i < sizeOfList; i ++) {
        result->set(i, LengthStyleInterpolation::lengthToInterpolableValue(*length[i]));
    }
    return result.release();
}

static inline PassRefPtrWillBeRawPtr<CSSPrimitiveValue> toPrimitiveValue(PassRefPtrWillBeRawPtr<CSSValue> value)
{
    return adoptRef(toCSSPrimitiveValue(value.leakRef()));
}

PassRefPtrWillBeRawPtr<CSSValue> LengthPoint3DStyleInterpolation::interpolableValueToLengthPoint3D(InterpolableValue* value)
{
    InterpolableList* lengthPoint3D = toInterpolableList(value);
    RefPtr<CSSValueList> result = CSSValueList::createCommaSeparated();
    const int sizeOfList = 3;

    for (size_t i = 0; i < sizeOfList; i++)
        result->append(toPrimitiveValue(LengthStyleInterpolation::interpolableValueToLength(lengthPoint3D->get(i), ValueRangeNonNegative)));

    return result.release();
}

void LengthPoint3DStyleInterpolation::apply(StyleResolverState& state) const
{
    StyleBuilder::applyProperty(m_id, state, interpolableValueToLengthPoint3D(m_cachedValue.get()).get());
}

void LengthPoint3DStyleInterpolation::trace(Visitor* visitor)
{
    StyleInterpolation::trace(visitor);
}

}


