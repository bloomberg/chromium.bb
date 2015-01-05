// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/LengthBoxStyleInterpolation.h"

#include "core/animation/LengthStyleInterpolation.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/Rect.h"
#include "core/css/resolver/StyleBuilder.h"

namespace blink {

bool LengthBoxStyleInterpolation::canCreateFrom(const CSSValue& value)
{
    return value.isPrimitiveValue() && toCSSPrimitiveValue(value).isRect();
}

PassOwnPtrWillBeRawPtr<InterpolableValue> LengthBoxStyleInterpolation::lengthBoxtoInterpolableValue(const CSSValue& lengthBox)
{
    const int numberOfSides = 4;
    OwnPtrWillBeRawPtr<InterpolableList> result = InterpolableList::create(numberOfSides);
    Rect* rect = toCSSPrimitiveValue(lengthBox).getRectValue();
    CSSPrimitiveValue* side[numberOfSides] = { rect->left(), rect->right(), rect->top(), rect->bottom() };

    for (size_t i = 0; i < numberOfSides; i++) {
        result->set(i, LengthStyleInterpolation::lengthToInterpolableValue(*side[i]));
    }
    return result.release();
}

PassRefPtrWillBeRawPtr<CSSValue> LengthBoxStyleInterpolation::interpolableValueToLengthBox(InterpolableValue* value)
{
    InterpolableList* lengthBox = toInterpolableList(value);
    RefPtrWillBeRawPtr<Rect> result = Rect::create();

    result->setLeft(LengthStyleInterpolation::interpolableValueToLength(lengthBox->get(0), ValueRangeNonNegative));
    result->setRight(LengthStyleInterpolation::interpolableValueToLength(lengthBox->get(1), ValueRangeNonNegative));
    result->setTop(LengthStyleInterpolation::interpolableValueToLength(lengthBox->get(2), ValueRangeNonNegative));
    result->setBottom(LengthStyleInterpolation::interpolableValueToLength(lengthBox->get(3), ValueRangeNonNegative));

    return CSSPrimitiveValue::create(result.release());
}

void LengthBoxStyleInterpolation::apply(StyleResolverState& state) const
{
    if (m_id == CSSPropertyWebkitMaskBoxImageSlice)
        StyleBuilder::applyProperty(m_id, state, interpolableValueToBorderImageSlice(m_cachedValue.get(), m_fill).get());
    else
        StyleBuilder::applyProperty(m_id, state, interpolableValueToLengthBox(m_cachedValue.get()).get());
}

void LengthBoxStyleInterpolation::trace(Visitor* visitor)
{
    StyleInterpolation::trace(visitor);
}

bool LengthBoxStyleInterpolation::matchingFill(CSSValue& start, CSSValue& end)
{
    return toCSSBorderImageSliceValue(start).m_fill == toCSSBorderImageSliceValue(end).m_fill;
}

bool LengthBoxStyleInterpolation::canCreateFromBorderImageSlice(CSSValue& value)
{
    return value.isBorderImageSliceValue() && toCSSBorderImageSliceValue(value).slices();
}

PassOwnPtrWillBeRawPtr<InterpolableValue> LengthBoxStyleInterpolation::borderImageSlicetoInterpolableValue(CSSValue& value)
{
    const int numberOfSides = 4;
    OwnPtrWillBeRawPtr<InterpolableList> result = InterpolableList::create(numberOfSides);
    Quad* quad = toCSSBorderImageSliceValue(value).slices();
    CSSPrimitiveValue* side[numberOfSides] = { quad->left(), quad->right(), quad->top(), quad->bottom() };

    for (size_t i = 0; i < numberOfSides; i++) {
        result->set(i, LengthStyleInterpolation::lengthToInterpolableValue(*side[i]));
    }
    return result.release();
}

static inline PassRefPtrWillBeRawPtr<CSSPrimitiveValue> toPrimitiveValue(PassRefPtrWillBeRawPtr<CSSValue> value)
{
    return adoptRef(toCSSPrimitiveValue(value.leakRef()));
}

PassRefPtrWillBeRawPtr<CSSValue> LengthBoxStyleInterpolation::interpolableValueToBorderImageSlice(InterpolableValue* value, bool fill)
{
    InterpolableList* lengthBox = toInterpolableList(value);
    RefPtrWillBeRawPtr<Quad> quad = Quad::create();

    quad->setLeft(toPrimitiveValue(LengthStyleInterpolation::interpolableValueToLength(lengthBox->get(0), ValueRangeNonNegative)));
    quad->setRight(toPrimitiveValue(LengthStyleInterpolation::interpolableValueToLength(lengthBox->get(1), ValueRangeNonNegative)));
    quad->setTop(toPrimitiveValue(LengthStyleInterpolation::interpolableValueToLength(lengthBox->get(2), ValueRangeNonNegative)));
    quad->setBottom(toPrimitiveValue(LengthStyleInterpolation::interpolableValueToLength(lengthBox->get(3), ValueRangeNonNegative)));

    return CSSBorderImageSliceValue::create(CSSPrimitiveValue::create(quad.release()), fill);
}

}
