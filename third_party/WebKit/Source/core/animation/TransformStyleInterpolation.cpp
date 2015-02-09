// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/TransformStyleInterpolation.h"

#include "core/animation/DoubleStyleInterpolation.h"
#include "core/animation/LengthBoxStyleInterpolation.h"
#include "core/animation/LengthStyleInterpolation.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSTransformValue.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/TransformBuilder.h"

namespace blink {

bool TransformStyleInterpolation::canCreateFrom(const CSSValue& value)
{
    if (value.isPrimitiveValue() && toCSSPrimitiveValue(value).isValueID())
        return toCSSPrimitiveValue(value).getValueID() == CSSValueNone;
    if (!value.isValueList())
        return false;
    const CSSValueList* listValue = toCSSValueList(&value);
    for (size_t i = 0; i < listValue->length(); i++) {
        if (!listValue->item(i)->isTransformValue()) {
            return false;
        }
    }
    return true;
}

bool TransformStyleInterpolation::fallBackToLegacy(CSSValue& start, CSSValue& end)
{
    if (!canCreateFrom(start) || !canCreateFrom(end))
        return false;
    if (start.isPrimitiveValue() && end.isPrimitiveValue())
        return false;
    if (start.isPrimitiveValue() || end.isPrimitiveValue()) {
        ASSERT(!start.isPrimitiveValue() || toCSSPrimitiveValue(start).getValueID() == CSSValueNone);
        ASSERT(!end.isPrimitiveValue() || toCSSPrimitiveValue(end).getValueID() == CSSValueNone);
        return true;
    }
    const CSSValueList* startList = toCSSValueList(&start);
    const CSSValueList* endList = toCSSValueList(&end);
    if (startList->length() != endList->length())
        return true;
    for (size_t i = 0; i < startList->length(); i++) {
        if (toCSSTransformValue(startList->item(i))->operationType() == CSSTransformValue::MatrixTransformOperation
            || toCSSTransformValue(startList->item(i))->operationType() == CSSTransformValue::Matrix3DTransformOperation
            || toCSSTransformValue(startList->item(i))->operationType() != toCSSTransformValue(endList->item(i))->operationType())
            return true;
    }

    return false;
}

PassOwnPtrWillBeRawPtr<InterpolableValue> TransformStyleInterpolation::transformToInterpolableValue(CSSValue& value)
{
    if (value.isPrimitiveValue())
        return InterpolableList::create(0);
    CSSValueList* listValue = toCSSValueList(&value);
    OwnPtrWillBeRawPtr<InterpolableList> result = InterpolableList::create(listValue->length());

    for (size_t j = 0; j < listValue->length(); j++) {
        CSSTransformValue* transform = toCSSTransformValue(listValue->item(j));
        const size_t length = transform->length();
        OwnPtrWillBeRawPtr<InterpolableList> resultItem = InterpolableList::create(length);
        switch (transform->operationType()) {
        case CSSTransformValue::SkewXTransformOperation:
        case CSSTransformValue::SkewYTransformOperation:
        case CSSTransformValue::ScaleXTransformOperation:
        case CSSTransformValue::ScaleYTransformOperation:
        case CSSTransformValue::ScaleZTransformOperation:
        case CSSTransformValue::RotateTransformOperation:
        case CSSTransformValue::RotateXTransformOperation:
        case CSSTransformValue::RotateYTransformOperation:
        case CSSTransformValue::RotateZTransformOperation:
        case CSSTransformValue::ScaleTransformOperation:
        case CSSTransformValue::SkewTransformOperation:
        case CSSTransformValue::Scale3DTransformOperation:
        case CSSTransformValue::Rotate3DTransformOperation:
            for (size_t i = 0; i < length; i++) {
                ASSERT(DoubleStyleInterpolation::canCreateFrom(*transform->item(i)));
                resultItem->set(i, DoubleStyleInterpolation::doubleToInterpolableValue(*transform->item(i)));
            }
            break;
        case CSSTransformValue::PerspectiveTransformOperation:
        case CSSTransformValue::TranslateXTransformOperation:
        case CSSTransformValue::TranslateYTransformOperation:
        case CSSTransformValue::TranslateZTransformOperation:
        case CSSTransformValue::TranslateTransformOperation:
        case CSSTransformValue::Translate3DTransformOperation:
            for (size_t i = 0; i < length; i++) {
                ASSERT(LengthStyleInterpolation::canCreateFrom(*transform->item(i)));
                resultItem->set(i, LengthStyleInterpolation::toInterpolableValue(*transform->item(i)));
            }
            break;
        case CSSTransformValue::UnknownTransformOperation:
            break;
        default:
            ASSERT_NOT_REACHED();
            return nullptr;
        }
        result->set(j, resultItem.release());
    }

    return result.release();
}


PassRefPtrWillBeRawPtr<CSSValue> TransformStyleInterpolation::interpolableValueToTransform(InterpolableValue* value, Vector<CSSTransformValue::TransformOperationType> types)
{
    InterpolableList* list = toInterpolableList(value);
    if (list->length() == 0)
        return CSSPrimitiveValue::createIdentifier(CSSValueNone);
    RefPtrWillBeRawPtr<CSSValueList> result = CSSValueList::createSpaceSeparated();

    for (size_t j = 0; j < list->length(); j++) {
        const InterpolableList* itemList = toInterpolableList(list->get(j));
        const size_t itemLength = itemList->length();
        RefPtrWillBeRawPtr<CSSTransformValue> transform = CSSTransformValue::create(types[j]);

        switch (types.at(j)) {
        case CSSTransformValue::ScaleXTransformOperation:
        case CSSTransformValue::ScaleYTransformOperation:
        case CSSTransformValue::ScaleZTransformOperation:
        case CSSTransformValue::ScaleTransformOperation:
        case CSSTransformValue::Scale3DTransformOperation:
            for (size_t i = 0; i < itemLength; i++) {
                ASSERT(itemList->get(i) > 0);
                transform->append(DoubleStyleInterpolation::interpolableValueToDouble(*itemList->get(i), true, RangeAll));
            }
            break;
        case CSSTransformValue::SkewXTransformOperation:
        case CSSTransformValue::SkewYTransformOperation:
        case CSSTransformValue::RotateTransformOperation:
        case CSSTransformValue::RotateXTransformOperation:
        case CSSTransformValue::RotateYTransformOperation:
        case CSSTransformValue::RotateZTransformOperation:
        case CSSTransformValue::Rotate3DTransformOperation:
        case CSSTransformValue::SkewTransformOperation:
            for (size_t i = 0; i < itemLength; i++)
                transform->append(DoubleStyleInterpolation::interpolableValueToDouble(*itemList->get(i), false, RangeAll));
            break;
        case CSSTransformValue::PerspectiveTransformOperation:
        case CSSTransformValue::TranslateXTransformOperation:
        case CSSTransformValue::TranslateYTransformOperation:
        case CSSTransformValue::TranslateZTransformOperation:
        case CSSTransformValue::TranslateTransformOperation:
        case CSSTransformValue::Translate3DTransformOperation:
            for (size_t i = 0; i < itemLength; i++)
                transform->append(LengthStyleInterpolation::fromInterpolableValue(*itemList->get(i), RangeAll));
            break;
        case CSSTransformValue::UnknownTransformOperation:
            break;
        default:
            ASSERT_NOT_REACHED();
            return nullptr;
        }
        result->append(transform);
    }
    return result.release();
}

void TransformStyleInterpolation::apply(StyleResolverState& state) const
{
    StyleBuilder::applyProperty(m_id, state, interpolableValueToTransform(m_cachedValue.get(), m_types).get());
}

void TransformStyleInterpolation::trace(Visitor* visitor)
{
    StyleInterpolation::trace(visitor);
}

}

