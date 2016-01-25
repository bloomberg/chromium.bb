// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/LengthBoxStyleInterpolation.h"

#include "core/animation/LengthStyleInterpolation.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSQuadValue.h"
#include "core/css/StylePropertySet.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class AnimationLengthBoxStyleInterpolationTest : public ::testing::Test {
protected:
    static PassOwnPtr<InterpolableValue> lengthBoxToInterpolableValue(const CSSValue& value)
    {
        return LengthBoxStyleInterpolation::lengthBoxtoInterpolableValue(value, value, false);
    }

    static PassRefPtrWillBeRawPtr<CSSValue> interpolableValueToLengthBox(InterpolableValue* value, const CSSValue& start, const CSSValue& end)
    {
        return LengthBoxStyleInterpolation::interpolableValueToLengthBox(value, start, end);
    }

    static PassRefPtrWillBeRawPtr<CSSValue> roundTrip(PassRefPtrWillBeRawPtr<CSSValue> value)
    {
        return interpolableValueToLengthBox(lengthBoxToInterpolableValue(*value).get(), *value, *value);
    }

    static void testQuadValue(RefPtrWillBeRawPtr<CSSValue> value, double left, double right, double top, double bottom, CSSPrimitiveValue::UnitType unitType)
    {
        EXPECT_TRUE(value->isQuadValue());
        RefPtrWillBeRawPtr<CSSQuadValue> rect = toCSSQuadValue(value.get());

        EXPECT_EQ(rect->left()->getDoubleValue(), left);
        EXPECT_EQ(rect->right()->getDoubleValue(), right);
        EXPECT_EQ(rect->top()->getDoubleValue(), top);
        EXPECT_EQ(rect->bottom()->getDoubleValue(), bottom);

        EXPECT_EQ(unitType, rect->left()->typeWithCalcResolved());
        EXPECT_EQ(unitType, rect->right()->typeWithCalcResolved());
        EXPECT_EQ(unitType, rect->top()->typeWithCalcResolved());
        EXPECT_EQ(unitType, rect->bottom()->typeWithCalcResolved());
    }
};

TEST_F(AnimationLengthBoxStyleInterpolationTest, ZeroLengthBox)
{
    RefPtrWillBeRawPtr<CSSPrimitiveValue> left = CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Pixels);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> right = CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Pixels);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> top = CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Pixels);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> bottom = CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Pixels);
    RefPtrWillBeRawPtr<CSSValue> value = roundTrip(CSSQuadValue::create(top, right, bottom, left, CSSQuadValue::SerializeAsRect));
    testQuadValue(value, 0, 0, 0, 0, CSSPrimitiveValue::UnitType::Pixels);

    RefPtrWillBeRawPtr<CSSPrimitiveValue> leftEms = CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Ems);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> rightEms = CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Ems);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> topEms = CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Ems);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> bottomEms = CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Ems);

    value = roundTrip(CSSQuadValue::create(topEms.release(), rightEms.release(), bottomEms.release(), leftEms.release(), CSSQuadValue::SerializeAsRect));
    testQuadValue(value, 0, 0, 0, 0, CSSPrimitiveValue::UnitType::Ems);
}

TEST_F(AnimationLengthBoxStyleInterpolationTest, SingleUnitBox)
{
    RefPtrWillBeRawPtr<CSSPrimitiveValue> leftPx = CSSPrimitiveValue::create(10, CSSPrimitiveValue::UnitType::Pixels);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> rightPx = CSSPrimitiveValue::create(10, CSSPrimitiveValue::UnitType::Pixels);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> topPx = CSSPrimitiveValue::create(10, CSSPrimitiveValue::UnitType::Pixels);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> bottomPx = CSSPrimitiveValue::create(10, CSSPrimitiveValue::UnitType::Pixels);

    RefPtrWillBeRawPtr<CSSValue> value = roundTrip(CSSQuadValue::create(topPx.release(), rightPx.release(), bottomPx.release(), leftPx.release(), CSSQuadValue::SerializeAsRect));
    testQuadValue(value, 10, 10, 10, 10, CSSPrimitiveValue::UnitType::Pixels);

    RefPtrWillBeRawPtr<CSSPrimitiveValue> leftPer = CSSPrimitiveValue::create(30, CSSPrimitiveValue::UnitType::Percentage);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> rightPer = CSSPrimitiveValue::create(30, CSSPrimitiveValue::UnitType::Percentage);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> topPer = CSSPrimitiveValue::create(30, CSSPrimitiveValue::UnitType::Percentage);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> bottomPer = CSSPrimitiveValue::create(30, CSSPrimitiveValue::UnitType::Percentage);

    value = roundTrip(CSSQuadValue::create(topPer.release(), rightPer.release(), bottomPer.release(), leftPer.release(), CSSQuadValue::SerializeAsRect));
    testQuadValue(value, 30, 30, 30, 30, CSSPrimitiveValue::UnitType::Percentage);

    RefPtrWillBeRawPtr<CSSPrimitiveValue> leftEms = CSSPrimitiveValue::create(-10, CSSPrimitiveValue::UnitType::Ems);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> rightEms = CSSPrimitiveValue::create(-10, CSSPrimitiveValue::UnitType::Ems);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> topEms = CSSPrimitiveValue::create(-10, CSSPrimitiveValue::UnitType::Ems);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> bottomEms = CSSPrimitiveValue::create(-10, CSSPrimitiveValue::UnitType::Ems);

    value = roundTrip(CSSQuadValue::create(topEms.release(), rightEms.release(), bottomEms.release(), leftEms.release(), CSSQuadValue::SerializeAsRect));
    testQuadValue(value, -10, -10, -10, -10, CSSPrimitiveValue::UnitType::Ems);
}

TEST_F(AnimationLengthBoxStyleInterpolationTest, MultipleValues)
{
    RefPtrWillBeRawPtr<CSSPrimitiveValue> leftPx = CSSPrimitiveValue::create(10, CSSPrimitiveValue::UnitType::Pixels);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> rightPx = CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Pixels);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> topPx = CSSPrimitiveValue::create(20, CSSPrimitiveValue::UnitType::Pixels);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> bottomPx = CSSPrimitiveValue::create(40, CSSPrimitiveValue::UnitType::Pixels);

    RefPtrWillBeRawPtr<CSSValue> value = roundTrip(CSSQuadValue::create(topPx.release(), rightPx.release(), bottomPx.release(), leftPx.release(), CSSQuadValue::SerializeAsRect));
    testQuadValue(value, 10, 0, 20, 40, CSSPrimitiveValue::UnitType::Pixels);

    RefPtrWillBeRawPtr<CSSPrimitiveValue> leftPer = CSSPrimitiveValue::create(30, CSSPrimitiveValue::UnitType::Percentage);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> rightPer = CSSPrimitiveValue::create(-30, CSSPrimitiveValue::UnitType::Percentage);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> topPer = CSSPrimitiveValue::create(30, CSSPrimitiveValue::UnitType::Percentage);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> bottomPer = CSSPrimitiveValue::create(-30, CSSPrimitiveValue::UnitType::Percentage);

    value = roundTrip(CSSQuadValue::create(topPer.release(), rightPer.release(), bottomPer.release(), leftPer.release(), CSSQuadValue::SerializeAsRect));
    testQuadValue(value, 30, -30, 30, -30, CSSPrimitiveValue::UnitType::Percentage);
}

} // namespace blink
