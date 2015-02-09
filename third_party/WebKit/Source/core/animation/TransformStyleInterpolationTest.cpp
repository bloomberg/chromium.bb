// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/TransformStyleInterpolation.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSTransformValue.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/TransformBuilder.h"

#include <gtest/gtest.h>

namespace blink {

class AnimationTransformStyleInterpolationTest : public ::testing::Test {
protected:
    static PassOwnPtrWillBeRawPtr<InterpolableValue> transformToInterpolableValue(CSSValue& value)
    {
        return TransformStyleInterpolation::transformToInterpolableValue(value);
    }

    static PassRefPtrWillBeRawPtr<CSSValue> interpolableValueToTransform(InterpolableValue* value, WillBeHeapVector<CSSTransformValue::TransformOperationType> types)
    {
        return TransformStyleInterpolation::interpolableValueToTransform(value, types);
    }

    static PassRefPtrWillBeRawPtr<CSSValue> roundTrip(CSSValue& value, WillBeHeapVector<CSSTransformValue::TransformOperationType> types)
    {
        return interpolableValueToTransform(transformToInterpolableValue(value).get(), types);
    }

    static void testPrimitiveValue(RefPtrWillBeRawPtr<CSSValue> value, double expectedValue, CSSPrimitiveValue::UnitType units[])
    {
        EXPECT_TRUE(value.get()->isValueList());
        CSSValueList* listValue = toCSSValueList(value.get());

        for (size_t j = 0; j < listValue->length(); j++) {
            EXPECT_TRUE(listValue->item(j)->isTransformValue());
            CSSTransformValue* transform = toCSSTransformValue(listValue->item(j));
            const size_t length = transform->length();
            for (size_t i = 0; i < length; i++) {
                EXPECT_TRUE(transform->item(i)->isPrimitiveValue());
                EXPECT_EQ(toCSSPrimitiveValue(transform->item(i))->getDoubleValue(), expectedValue);
                EXPECT_EQ(toCSSPrimitiveValue(transform->item(i))->primitiveType(), units[j]);
            }
        }
    }
};

TEST_F(AnimationTransformStyleInterpolationTest, ZeroTransform)
{
    RefPtrWillBeRawPtr<CSSValueList> result = CSSValueList::createSpaceSeparated();

    RefPtrWillBeRawPtr<CSSTransformValue> transform = CSSTransformValue::create(CSSTransformValue::PerspectiveTransformOperation);
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PX));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::TranslateTransformOperation);
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PX));
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PX));
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PX));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::TranslateXTransformOperation);
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PX));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::TranslateYTransformOperation);
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PX));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::ScaleTransformOperation);
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_NUMBER));
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_NUMBER));
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_NUMBER));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::ScaleXTransformOperation);
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_NUMBER));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::ScaleYTransformOperation);
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_NUMBER));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::RotateTransformOperation);
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_DEG));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::SkewTransformOperation);
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_DEG));
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_DEG));
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_DEG));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::SkewXTransformOperation);
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_DEG));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::SkewYTransformOperation);
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_DEG));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::RotateXTransformOperation);
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_DEG));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::RotateYTransformOperation);
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_DEG));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::TranslateZTransformOperation);
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PX));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::Translate3DTransformOperation);
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PX));
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PX));
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PX));
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PX));
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PX));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::ScaleZTransformOperation);
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_NUMBER));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::Scale3DTransformOperation);
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_NUMBER));
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_NUMBER));
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_NUMBER));
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_NUMBER));
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_NUMBER));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::RotateZTransformOperation);
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_DEG));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::Rotate3DTransformOperation);
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_NUMBER));
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_NUMBER));
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_NUMBER));
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_NUMBER));
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_NUMBER));
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_NUMBER));
    transform->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_NUMBER));
    result->append(transform);

    WillBeHeapVector<CSSTransformValue::TransformOperationType> types = WillBeHeapVector<CSSTransformValue::TransformOperationType>(19);
    types.append(CSSTransformValue::PerspectiveTransformOperation);
    types.append(CSSTransformValue::TranslateTransformOperation);
    types.append(CSSTransformValue::TranslateXTransformOperation);
    types.append(CSSTransformValue::TranslateYTransformOperation);
    types.append(CSSTransformValue::ScaleTransformOperation);
    types.append(CSSTransformValue::ScaleXTransformOperation);
    types.append(CSSTransformValue::ScaleYTransformOperation);
    types.append(CSSTransformValue::RotateTransformOperation);
    types.append(CSSTransformValue::SkewTransformOperation);
    types.append(CSSTransformValue::SkewXTransformOperation);
    types.append(CSSTransformValue::SkewYTransformOperation);
    types.append(CSSTransformValue::RotateXTransformOperation);
    types.append(CSSTransformValue::RotateYTransformOperation);
    types.append(CSSTransformValue::TranslateZTransformOperation);
    types.append(CSSTransformValue::Translate3DTransformOperation);
    types.append(CSSTransformValue::ScaleZTransformOperation);
    types.append(CSSTransformValue::Scale3DTransformOperation);
    types.append(CSSTransformValue::RotateZTransformOperation);
    types.append(CSSTransformValue::Rotate3DTransformOperation);

    CSSPrimitiveValue::UnitType units[19];
    units[0] = CSSPrimitiveValue::CSS_NUMBER;
    units[1] = CSSPrimitiveValue::CSS_PX;
    units[2] = CSSPrimitiveValue::CSS_PX;
    units[3] = CSSPrimitiveValue::CSS_PX;
    units[4] = CSSPrimitiveValue::CSS_NUMBER;
    units[5] = CSSPrimitiveValue::CSS_NUMBER;
    units[6] = CSSPrimitiveValue::CSS_NUMBER;
    units[7] = CSSPrimitiveValue::CSS_DEG;
    units[8] = CSSPrimitiveValue::CSS_DEG;
    units[9] = CSSPrimitiveValue::CSS_DEG;
    units[10] = CSSPrimitiveValue::CSS_DEG;
    units[11] = CSSPrimitiveValue::CSS_DEG;
    units[12] = CSSPrimitiveValue::CSS_DEG;
    units[13] = CSSPrimitiveValue::CSS_PX;
    units[14] = CSSPrimitiveValue::CSS_PX;
    units[15] = CSSPrimitiveValue::CSS_NUMBER;
    units[16] = CSSPrimitiveValue::CSS_NUMBER;
    units[17] = CSSPrimitiveValue::CSS_DEG;
    units[18] = CSSPrimitiveValue::CSS_NUMBER;

    RefPtrWillBeRawPtr<CSSValue> value = roundTrip(*result, types);
    testPrimitiveValue(value, 0, units);
}

TEST_F(AnimationTransformStyleInterpolationTest, SingleUnitTransform)
{
    RefPtrWillBeRawPtr<CSSValueList> result = CSSValueList::createSpaceSeparated();

    RefPtrWillBeRawPtr<CSSTransformValue> transform = CSSTransformValue::create(CSSTransformValue::PerspectiveTransformOperation);
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_PX));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::TranslateTransformOperation);
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_PX));
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_PX));
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_PX));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::TranslateXTransformOperation);
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_PX));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::TranslateYTransformOperation);
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_PX));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::ScaleTransformOperation);
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_NUMBER));
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_NUMBER));
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_NUMBER));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::ScaleXTransformOperation);
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_NUMBER));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::ScaleYTransformOperation);
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_NUMBER));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::RotateTransformOperation);
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_NUMBER));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::SkewTransformOperation);
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_NUMBER));
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_NUMBER));
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_NUMBER));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::SkewXTransformOperation);
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_NUMBER));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::SkewYTransformOperation);
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_NUMBER));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::RotateXTransformOperation);
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_NUMBER));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::RotateYTransformOperation);
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_NUMBER));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::TranslateZTransformOperation);
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_PX));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::Translate3DTransformOperation);
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_PX));
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_PX));
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_PX));
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_PX));
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_PX));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::ScaleZTransformOperation);
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_NUMBER));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::Scale3DTransformOperation);
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_NUMBER));
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_NUMBER));
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_NUMBER));
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_NUMBER));
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_NUMBER));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::RotateZTransformOperation);
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_NUMBER));
    result->append(transform);
    transform = CSSTransformValue::create(CSSTransformValue::Rotate3DTransformOperation);
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_NUMBER));
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_NUMBER));
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_NUMBER));
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_NUMBER));
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_NUMBER));
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_NUMBER));
    transform->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_NUMBER));
    result->append(transform);

    WillBeHeapVector<CSSTransformValue::TransformOperationType> types = WillBeHeapVector<CSSTransformValue::TransformOperationType>(19);
    types.append(CSSTransformValue::PerspectiveTransformOperation);
    types.append(CSSTransformValue::TranslateTransformOperation);
    types.append(CSSTransformValue::TranslateXTransformOperation);
    types.append(CSSTransformValue::TranslateYTransformOperation);
    types.append(CSSTransformValue::ScaleTransformOperation);
    types.append(CSSTransformValue::ScaleXTransformOperation);
    types.append(CSSTransformValue::ScaleYTransformOperation);
    types.append(CSSTransformValue::RotateTransformOperation);
    types.append(CSSTransformValue::SkewTransformOperation);
    types.append(CSSTransformValue::SkewXTransformOperation);
    types.append(CSSTransformValue::SkewYTransformOperation);
    types.append(CSSTransformValue::RotateXTransformOperation);
    types.append(CSSTransformValue::RotateYTransformOperation);
    types.append(CSSTransformValue::TranslateZTransformOperation);
    types.append(CSSTransformValue::Translate3DTransformOperation);
    types.append(CSSTransformValue::ScaleZTransformOperation);
    types.append(CSSTransformValue::Scale3DTransformOperation);
    types.append(CSSTransformValue::RotateZTransformOperation);
    types.append(CSSTransformValue::Rotate3DTransformOperation);

    CSSPrimitiveValue::UnitType units[19];
    units[0] = CSSPrimitiveValue::CSS_NUMBER;
    units[1] = CSSPrimitiveValue::CSS_PX;
    units[2] = CSSPrimitiveValue::CSS_PX;
    units[3] = CSSPrimitiveValue::CSS_PX;
    units[4] = CSSPrimitiveValue::CSS_NUMBER;
    units[5] = CSSPrimitiveValue::CSS_NUMBER;
    units[6] = CSSPrimitiveValue::CSS_NUMBER;
    units[7] = CSSPrimitiveValue::CSS_DEG;
    units[8] = CSSPrimitiveValue::CSS_DEG;
    units[9] = CSSPrimitiveValue::CSS_DEG;
    units[10] = CSSPrimitiveValue::CSS_DEG;
    units[11] = CSSPrimitiveValue::CSS_DEG;
    units[12] = CSSPrimitiveValue::CSS_DEG;
    units[13] = CSSPrimitiveValue::CSS_PX;
    units[14] = CSSPrimitiveValue::CSS_PX;
    units[15] = CSSPrimitiveValue::CSS_NUMBER;
    units[16] = CSSPrimitiveValue::CSS_NUMBER;
    units[17] = CSSPrimitiveValue::CSS_DEG;
    units[18] = CSSPrimitiveValue::CSS_NUMBER;

    RefPtrWillBeRawPtr<CSSValue> value = roundTrip(*result, types);
    testPrimitiveValue(value, 10, units);
}

}
