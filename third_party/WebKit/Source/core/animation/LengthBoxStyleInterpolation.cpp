// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/LengthBoxStyleInterpolation.h"

#include "core/css/resolver/StyleBuilder.h"

namespace blink {

namespace {

bool onlyLengthToAuto(Rect* startRect, Rect* endRect)
{
    return (startRect->left()->isLength() == endRect->left()->isValueID() || endRect->left()->isLength() == startRect->left()->isValueID())
        && (startRect->right()->isLength() == endRect->right()->isValueID() || endRect->right()->isLength() == startRect->right()->isValueID())
        && (startRect->top()->isLength() == endRect->top()->isValueID() || endRect->top()->isLength() == startRect->top()->isValueID())
        && (startRect->bottom()->isLength() == endRect->bottom()->isValueID() || endRect->bottom()->isLength() == startRect->bottom()->isValueID());
}

} // namespace

PassRefPtrWillBeRawPtr<LengthBoxStyleInterpolation> LengthBoxStyleInterpolation::maybeCreateFrom(CSSValue* start, CSSValue* end, CSSPropertyID id)
{
    bool startRect = start->isPrimitiveValue() && toCSSPrimitiveValue(start)->isRect();
    bool endRect = end->isPrimitiveValue() && toCSSPrimitiveValue(end)->isRect();
    bool startAuto = start->isPrimitiveValue() && toCSSPrimitiveValue(start)->isValueID() && toCSSPrimitiveValue(start)->getValueID() == CSSValueAuto;
    bool endAuto = end->isPrimitiveValue() && toCSSPrimitiveValue(end)->isValueID() && toCSSPrimitiveValue(end)->getValueID() == CSSValueAuto;

    if (startAuto || endAuto) {
        return adoptRefWillBeNoop(new LengthBoxStyleInterpolation(InterpolableBool::create(false), InterpolableBool::create(true), id, false, start, end));
    }
    if (startRect && endRect) {
        Rect* startRectValue = toCSSPrimitiveValue(start)->getRectValue();
        Rect* endRectValue = toCSSPrimitiveValue(end)->getRectValue();

        if (onlyLengthToAuto(startRectValue, endRectValue))
            return adoptRefWillBeNoop(new LengthBoxStyleInterpolation(InterpolableBool::create(false), InterpolableBool::create(true), id, false, start, end));
        return adoptRefWillBeNoop(new LengthBoxStyleInterpolation(lengthBoxtoInterpolableValue(*start, *end, false), lengthBoxtoInterpolableValue(*end, *start, true), id, false, start, end));
    }
    if (start->isBorderImageSliceValue() && toCSSBorderImageSliceValue(start)->slices()
        && end->isBorderImageSliceValue() && toCSSBorderImageSliceValue(end)->slices()
        && toCSSBorderImageSliceValue(start)->m_fill == toCSSBorderImageSliceValue(end)->m_fill)
        return adoptRefWillBeNoop(new LengthBoxStyleInterpolation(borderImageSlicetoInterpolableValue(*start), borderImageSlicetoInterpolableValue(*end), id, toCSSBorderImageSliceValue(start)->m_fill, start, end));
    return nullptr;
}

PassOwnPtrWillBeRawPtr<InterpolableValue> LengthBoxStyleInterpolation::lengthBoxtoInterpolableValue(const CSSValue& lengthBox, const CSSValue& matchingValue, bool isEndInterpolation)
{
    const int numberOfSides = 4;
    OwnPtrWillBeRawPtr<InterpolableList> result = InterpolableList::create(numberOfSides);
    Rect* rect = toCSSPrimitiveValue(lengthBox).getRectValue();
    Rect* matchingRect = toCSSPrimitiveValue(matchingValue).getRectValue();
    CSSPrimitiveValue* side[numberOfSides] = { rect->left(), rect->right(), rect->top(), rect->bottom() };
    CSSPrimitiveValue* matchingSide[numberOfSides] = { matchingRect->left(), matchingRect->right(), matchingRect->top(), matchingRect->bottom() };

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

namespace {

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> findValue(InterpolableList* lengthBox, size_t i, CSSPrimitiveValue* start[], CSSPrimitiveValue* end[])
{
    if (lengthBox->get(i)->isBool()) {
        if (toInterpolableBool(lengthBox->get(i))->value())
            return end[i];
        return start[i];
    }
    return LengthStyleInterpolation::fromInterpolableValue(*lengthBox->get(i), RangeAll);
}

}

PassRefPtrWillBeRawPtr<CSSValue> LengthBoxStyleInterpolation::interpolableValueToLengthBox(InterpolableValue* value, const CSSValue& originalStart, const CSSValue& originalEnd)
{
    InterpolableList* lengthBox = toInterpolableList(value);
    Rect* startRect = toCSSPrimitiveValue(originalStart).getRectValue();
    Rect* endRect = toCSSPrimitiveValue(originalEnd).getRectValue();
    CSSPrimitiveValue* startSides[4] = { startRect->left(), startRect->right(), startRect->top(), startRect->bottom() };
    CSSPrimitiveValue* endSides[4] = { endRect->left(), endRect->right(), endRect->top(), endRect->bottom() };
    RefPtrWillBeRawPtr<Rect> result = Rect::create();

    result->setLeft(findValue(lengthBox, 0, startSides, endSides));
    result->setRight(findValue(lengthBox, 1, startSides, endSides));
    result->setTop(findValue(lengthBox, 2, startSides, endSides));
    result->setBottom(findValue(lengthBox, 3, startSides, endSides));

    return CSSPrimitiveValue::create(result.release());
}

PassOwnPtrWillBeRawPtr<InterpolableValue> LengthBoxStyleInterpolation::borderImageSlicetoInterpolableValue(const CSSValue& value)
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

PassRefPtrWillBeRawPtr<CSSValue> LengthBoxStyleInterpolation::interpolableValueToBorderImageSlice(InterpolableValue* value, bool fill)
{
    InterpolableList* lengthBox = toInterpolableList(value);
    RefPtrWillBeRawPtr<Quad> quad = Quad::create();

    quad->setLeft(LengthStyleInterpolation::fromInterpolableValue(*lengthBox->get(0), RangeNonNegative));
    quad->setRight(LengthStyleInterpolation::fromInterpolableValue(*lengthBox->get(1), RangeNonNegative));
    quad->setTop(LengthStyleInterpolation::fromInterpolableValue(*lengthBox->get(2), RangeNonNegative));
    quad->setBottom(LengthStyleInterpolation::fromInterpolableValue(*lengthBox->get(3), RangeNonNegative));

    return CSSBorderImageSliceValue::create(CSSPrimitiveValue::create(quad.release()), fill);
}

void LengthBoxStyleInterpolation::apply(StyleResolverState& state) const
{
    if (m_id == CSSPropertyWebkitMaskBoxImageSlice || m_id == CSSPropertyBorderImageSlice) {
        StyleBuilder::applyProperty(m_id, state, interpolableValueToBorderImageSlice(m_cachedValue.get(), m_fill).get());
    } else if (m_cachedValue.get()->isBool()) {
        StyleBuilder::applyProperty(m_id, state, toInterpolableBool(m_cachedValue.get())->value() ? m_endCSSValue.get() : m_startCSSValue.get());
    } else {
        StyleBuilder::applyProperty(m_id, state, interpolableValueToLengthBox(m_cachedValue.get(), *m_startCSSValue, *m_endCSSValue).get());
    }
}

void LengthBoxStyleInterpolation::trace(Visitor* visitor)
{
    StyleInterpolation::trace(visitor);
    visitor->trace(m_startCSSValue);
    visitor->trace(m_endCSSValue);
}

}
