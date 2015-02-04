// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/LengthBoxStyleInterpolation.h"

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
        result->set(i, LengthStyleInterpolation::toInterpolableValue(*side[i]));
    }
    return result.release();
}

PassRefPtrWillBeRawPtr<CSSValue> LengthBoxStyleInterpolation::interpolableValueToLengthBox(InterpolableValue* value, InterpolationRange range)
{
    InterpolableList* lengthBox = toInterpolableList(value);
    RefPtrWillBeRawPtr<Rect> result = Rect::create();

    result->setLeft(LengthStyleInterpolation::fromInterpolableValue(*lengthBox->get(0), RangeNonNegative));
    result->setRight(LengthStyleInterpolation::fromInterpolableValue(*lengthBox->get(1), RangeNonNegative));
    result->setTop(LengthStyleInterpolation::fromInterpolableValue(*lengthBox->get(2), RangeNonNegative));
    result->setBottom(LengthStyleInterpolation::fromInterpolableValue(*lengthBox->get(3), RangeNonNegative));

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
        result->set(i, LengthStyleInterpolation::toInterpolableValue(*side[i]));
    }
    return result.release();
}

static inline PassRefPtrWillBeRawPtr<CSSPrimitiveValue> toPrimitiveValue(PassRefPtrWillBeRawPtr<CSSValue> value)
{
    return adoptRefWillBeNoop(toCSSPrimitiveValue(value.leakRef()));
}

PassRefPtrWillBeRawPtr<CSSValue> LengthBoxStyleInterpolation::interpolableValueToBorderImageSlice(InterpolableValue* value, bool fill)
{
    InterpolableList* lengthBox = toInterpolableList(value);
    RefPtrWillBeRawPtr<Quad> quad = Quad::create();

    quad->setLeft(toPrimitiveValue(LengthStyleInterpolation::fromInterpolableValue(*lengthBox->get(0), RangeNonNegative)));
    quad->setRight(toPrimitiveValue(LengthStyleInterpolation::fromInterpolableValue(*lengthBox->get(1), RangeNonNegative)));
    quad->setTop(toPrimitiveValue(LengthStyleInterpolation::fromInterpolableValue(*lengthBox->get(2), RangeNonNegative)));
    quad->setBottom(toPrimitiveValue(LengthStyleInterpolation::fromInterpolableValue(*lengthBox->get(3), RangeNonNegative)));

    return CSSBorderImageSliceValue::create(CSSPrimitiveValue::create(quad.release()), fill);
}

}
