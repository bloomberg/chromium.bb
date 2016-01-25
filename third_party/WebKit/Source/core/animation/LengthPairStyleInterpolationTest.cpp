// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/LengthPairStyleInterpolation.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValue.h"
#include "core/css/CSSValuePair.h"
#include "core/css/StylePropertySet.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class LengthPairStyleInterpolationTest : public ::testing::Test {

protected:
    static PassOwnPtr<InterpolableValue> lengthPairToInterpolableValue(const CSSValue& value)
    {
        return LengthPairStyleInterpolation::lengthPairToInterpolableValue(value);
    }

    static PassRefPtrWillBeRawPtr<CSSValue> interpolableValueToLengthPair(InterpolableValue* value, InterpolationRange range)
    {
        return LengthPairStyleInterpolation::interpolableValueToLengthPair(value, range);
    }

    static PassRefPtrWillBeRawPtr<CSSValue> roundTrip(PassRefPtrWillBeRawPtr<CSSValue> value, InterpolationRange range)
    {
        return interpolableValueToLengthPair(lengthPairToInterpolableValue(*value).get(), range);
    }

    static void testPrimitiveValue(RefPtrWillBeRawPtr<CSSValue> value, double first, double second, CSSPrimitiveValue::UnitType unitType)
    {
        EXPECT_TRUE(value->isValuePair());
        const CSSValuePair& pair = toCSSValuePair(*value);

        EXPECT_EQ(toCSSPrimitiveValue(pair.first()).getDoubleValue(), first);
        EXPECT_EQ(toCSSPrimitiveValue(pair.second()).getDoubleValue(), second);

        EXPECT_EQ(toCSSPrimitiveValue(pair.first()).typeWithCalcResolved(), unitType);
        EXPECT_EQ(toCSSPrimitiveValue(pair.second()).typeWithCalcResolved(), unitType);
    }
};

TEST_F(LengthPairStyleInterpolationTest, ZeroTest)
{
    RefPtrWillBeRawPtr<CSSPrimitiveValue> firstPx = CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Pixels);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> secondPx = CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Pixels);
    RefPtrWillBeRawPtr<CSSValuePair> pairPx = CSSValuePair::create(firstPx.release(), secondPx.release(), CSSValuePair::KeepIdenticalValues);
    RefPtrWillBeRawPtr<CSSValue> value1 = roundTrip(pairPx.release(), RangeNonNegative);
    testPrimitiveValue(value1, 0, 0, CSSPrimitiveValue::UnitType::Pixels);

    RefPtrWillBeRawPtr<CSSPrimitiveValue> firstPc = CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Percentage);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> secondPc = CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Percentage);
    RefPtrWillBeRawPtr<CSSValuePair> pairPc = CSSValuePair::create(firstPc.release(), secondPc.release(), CSSValuePair::KeepIdenticalValues);
    RefPtrWillBeRawPtr<CSSValue> value2 = roundTrip(pairPc.release(), RangeNonNegative);
    testPrimitiveValue(value2, 0, 0, CSSPrimitiveValue::UnitType::Percentage);

    RefPtrWillBeRawPtr<CSSPrimitiveValue> firstEms = CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Ems);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> secondEms = CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Ems);
    RefPtrWillBeRawPtr<CSSValuePair> pairEms = CSSValuePair::create(firstEms.release(), secondEms.release(), CSSValuePair::KeepIdenticalValues);
    RefPtrWillBeRawPtr<CSSValue> value3 = roundTrip(pairEms.release(), RangeNonNegative);
    testPrimitiveValue(value3, 0, 0, CSSPrimitiveValue::UnitType::Ems);

    RefPtrWillBeRawPtr<CSSPrimitiveValue> firstPcNeg = CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Percentage);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> secondPcNeg = CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Percentage);
    RefPtrWillBeRawPtr<CSSValuePair> pairPcNeg = CSSValuePair::create(firstPcNeg.release(), secondPcNeg.release(), CSSValuePair::KeepIdenticalValues);
    RefPtrWillBeRawPtr<CSSValue> value4 = roundTrip(pairPcNeg.release(), RangeAll);
    testPrimitiveValue(value4, 0, 0, CSSPrimitiveValue::UnitType::Percentage);
}

TEST_F(LengthPairStyleInterpolationTest, MultipleValueTest)
{
    RefPtrWillBeRawPtr<CSSPrimitiveValue> firstPx = CSSPrimitiveValue::create(10, CSSPrimitiveValue::UnitType::Pixels);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> secondPx = CSSPrimitiveValue::create(20, CSSPrimitiveValue::UnitType::Pixels);
    RefPtrWillBeRawPtr<CSSValuePair> pairPx = CSSValuePair::create(firstPx.release(), secondPx.release(), CSSValuePair::KeepIdenticalValues);
    RefPtrWillBeRawPtr<CSSValue> value5 = roundTrip(pairPx.release(), RangeNonNegative);
    testPrimitiveValue(value5, 10, 20, CSSPrimitiveValue::UnitType::Pixels);

    RefPtrWillBeRawPtr<CSSPrimitiveValue> firstPc = CSSPrimitiveValue::create(30, CSSPrimitiveValue::UnitType::Percentage);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> secondPc = CSSPrimitiveValue::create(-30, CSSPrimitiveValue::UnitType::Percentage);
    RefPtrWillBeRawPtr<CSSValuePair> pairPc = CSSValuePair::create(firstPc.release(), secondPc.release(), CSSValuePair::KeepIdenticalValues);
    RefPtrWillBeRawPtr<CSSValue> value6 = roundTrip(pairPc.release(), RangeNonNegative);
    testPrimitiveValue(value6, 30, 0, CSSPrimitiveValue::UnitType::Percentage);
}

} // namespace blink
