// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/LengthBoxStyleInterpolation.h"

#include "core/css/CSSQuadValue.h"
#include "core/css/resolver/StyleBuilder.h"

namespace blink {

namespace {

bool onlyInterpolateBetweenLengthAndCSSValueAuto(const CSSQuadValue& startRect, const CSSQuadValue& endRect)
{
    return startRect.left()->isLength() != endRect.left()->isLength()
        && startRect.right()->isLength() != endRect.right()->isLength()
        && startRect.top()->isLength() != endRect.top()->isLength()
        && startRect.bottom()->isLength() != endRect.bottom()->isLength();
}

} // namespace

PassRefPtr<LengthBoxStyleInterpolation> LengthBoxStyleInterpolation::maybeCreateFrom(CSSValue& start, CSSValue& end, CSSPropertyID id)
{
    bool startRect = start.isQuadValue() && toCSSQuadValue(start).serializationType() == CSSQuadValue::SerializationType::SerializeAsRect;
    bool endRect = end.isQuadValue() && toCSSQuadValue(end).serializationType() == CSSQuadValue::SerializationType::SerializeAsRect;

    if (startRect && endRect)
        return adoptRef(new LengthBoxStyleInterpolation(lengthBoxtoInterpolableValue(start, end, false), lengthBoxtoInterpolableValue(end, start, true), id, &start, &end));
    return nullptr;
}

PassOwnPtr<InterpolableValue> LengthBoxStyleInterpolation::lengthBoxtoInterpolableValue(const CSSValue& lengthBox, const CSSValue& matchingValue, bool isEndInterpolation)
{
    const int numberOfSides = 4;
    OwnPtr<InterpolableList> result = InterpolableList::create(numberOfSides);
    const CSSQuadValue& rect = toCSSQuadValue(lengthBox);
    const CSSQuadValue& matchingRect = toCSSQuadValue(matchingValue);
    CSSPrimitiveValue* side[numberOfSides] = { rect.left(), rect.right(), rect.top(), rect.bottom() };
    CSSPrimitiveValue* matchingSide[numberOfSides] = { matchingRect.left(), matchingRect.right(), matchingRect.top(), matchingRect.bottom() };

    for (size_t i = 0; i < numberOfSides; i++) {
        if (side[i]->isValueID() || matchingSide[i]->isValueID()) {
            result->set(i, InterpolableBool::create(isEndInterpolation));
        } else {
            ASSERT(LengthStyleInterpolation::canCreateFrom(*side[i]));
            result->set(i, LengthStyleInterpolation::toInterpolableValue(*side[i]));
        }
    }
    return result.release();
}

bool LengthBoxStyleInterpolation::usesDefaultInterpolation(const CSSValue& start, const CSSValue& end)
{
    if (start.isPrimitiveValue() && end.isPrimitiveValue()) {
        const CSSPrimitiveValue& startValue = toCSSPrimitiveValue(start);
        const CSSPrimitiveValue& endValue = toCSSPrimitiveValue(end);
        return (startValue.isValueID() && startValue.getValueID() == CSSValueAuto)
            || (endValue.isValueID() && endValue.getValueID() == CSSValueAuto);
    }

    if (!start.isQuadValue() || !end.isQuadValue())
        return false;

    const CSSQuadValue& startValue = toCSSQuadValue(start);
    const CSSQuadValue& endValue = toCSSQuadValue(end);
    return onlyInterpolateBetweenLengthAndCSSValueAuto(startValue, endValue);
}

namespace {

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> indexedValueToLength(InterpolableList& lengthBox, size_t i, CSSPrimitiveValue* start[], CSSPrimitiveValue* end[])
{
    if (lengthBox.get(i)->isBool()) {
        if (toInterpolableBool(lengthBox.get(i))->value())
            return end[i];
        return start[i];
    }
    return LengthStyleInterpolation::fromInterpolableValue(*lengthBox.get(i), RangeAll);
}

} // namespace

PassRefPtrWillBeRawPtr<CSSValue> LengthBoxStyleInterpolation::interpolableValueToLengthBox(InterpolableValue* value, const CSSValue& originalStart, const CSSValue& originalEnd)
{
    InterpolableList* lengthBox = toInterpolableList(value);
    const CSSQuadValue& startRect = toCSSQuadValue(originalStart);
    const CSSQuadValue& endRect = toCSSQuadValue(originalEnd);
    CSSPrimitiveValue* startSides[4] = { startRect.left(), startRect.right(), startRect.top(), startRect.bottom() };
    CSSPrimitiveValue* endSides[4] = { endRect.left(), endRect.right(), endRect.top(), endRect.bottom() };

    RefPtrWillBeRawPtr<CSSPrimitiveValue> left = indexedValueToLength(*lengthBox, 0, startSides, endSides);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> right = indexedValueToLength(*lengthBox, 1, startSides, endSides);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> top = indexedValueToLength(*lengthBox, 2, startSides, endSides);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> bottom = indexedValueToLength(*lengthBox, 3, startSides, endSides);

    return CSSQuadValue::create(top.release(), right.release(), bottom.release(), left.release(), CSSQuadValue::SerializeAsRect);
}

void LengthBoxStyleInterpolation::apply(StyleResolverState& state) const
{
    if (m_cachedValue.get()->isBool())
        StyleBuilder::applyProperty(m_id, state, toInterpolableBool(m_cachedValue.get())->value() ? m_endCSSValue.get() : m_startCSSValue.get());
    else
        StyleBuilder::applyProperty(m_id, state, interpolableValueToLengthBox(m_cachedValue.get(), *m_startCSSValue, *m_endCSSValue).get());
}

} // namespace blink
