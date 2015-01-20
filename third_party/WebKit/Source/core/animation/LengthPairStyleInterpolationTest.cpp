// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/LengthPairStyleInterpolation.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValue.h"
#include "core/css/Pair.h"
#include "core/css/StylePropertySet.h"

#include <gtest/gtest.h>

namespace blink {

class LengthPairStyleInterpolationTest : public ::testing::Test {

protected:
    static PassOwnPtrWillBeRawPtr<InterpolableValue> lengthPairToInterpolableValue(const CSSValue& value)
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
        EXPECT_TRUE(value->isPrimitiveValue());

        Pair* pair = toCSSPrimitiveValue(value.get())->getPairValue();
        ASSERT_TRUE(pair);

        EXPECT_EQ(pair->first()->getDoubleValue(), first);
        EXPECT_EQ(pair->second()->getDoubleValue(), second);

        EXPECT_EQ(pair->first()->primitiveType(), unitType);
        EXPECT_EQ(pair->second()->primitiveType(), unitType);
    }
};

TEST_F(LengthPairStyleInterpolationTest, ZeroTest)
{
    RefPtrWillBeRawPtr<CSSPrimitiveValue> firstPx = CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PX);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> secondPx = CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PX);
    RefPtrWillBeRawPtr<Pair> pairPx = Pair::create(firstPx, secondPx, Pair::KeepIdenticalValues);
    RefPtrWillBeRawPtr<CSSValue> value1 = roundTrip(CSSPrimitiveValue::create(pairPx.release()), RangeNonNegative);
    testPrimitiveValue(value1, 0, 0, CSSPrimitiveValue::CSS_PX);

    RefPtrWillBeRawPtr<CSSPrimitiveValue> firstPc = CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PERCENTAGE);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> secondPc = CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PERCENTAGE);
    RefPtrWillBeRawPtr<Pair> pairPc = Pair::create(firstPc, secondPc, Pair::KeepIdenticalValues);
    RefPtrWillBeRawPtr<CSSValue> value2 = roundTrip(CSSPrimitiveValue::create(pairPc.release()), RangeNonNegative);
    testPrimitiveValue(value2, 0, 0, CSSPrimitiveValue::CSS_PERCENTAGE);

    RefPtrWillBeRawPtr<CSSPrimitiveValue> firstEms = CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_EMS);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> secondEms = CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_EMS);
    RefPtrWillBeRawPtr<Pair> pairEms = Pair::create(firstEms, secondEms, Pair::KeepIdenticalValues);
    RefPtrWillBeRawPtr<CSSValue> value3 = roundTrip(CSSPrimitiveValue::create(pairEms.release()), RangeNonNegative);
    testPrimitiveValue(value3, 0, 0, CSSPrimitiveValue::CSS_EMS);

    RefPtrWillBeRawPtr<CSSPrimitiveValue> firstPcNeg = CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PERCENTAGE);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> secondPcNeg = CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PERCENTAGE);
    RefPtrWillBeRawPtr<Pair> pairPcNeg = Pair::create(firstPcNeg, secondPcNeg, Pair::KeepIdenticalValues);
    RefPtrWillBeRawPtr<CSSValue> value4 = roundTrip(CSSPrimitiveValue::create(pairPcNeg.release()), RangeAll);
    testPrimitiveValue(value4, 0, 0, CSSPrimitiveValue::CSS_PERCENTAGE);
}

TEST_F(LengthPairStyleInterpolationTest, MultipleValueTest)
{
    RefPtrWillBeRawPtr<CSSPrimitiveValue> firstPx = CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_PX);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> secondPx = CSSPrimitiveValue::create(20, CSSPrimitiveValue::CSS_PX);
    RefPtrWillBeRawPtr<Pair> pairPx = Pair::create(firstPx, secondPx, Pair::KeepIdenticalValues);
    RefPtrWillBeRawPtr<CSSValue> value5 = roundTrip(CSSPrimitiveValue::create(pairPx.release()), RangeNonNegative);
    testPrimitiveValue(value5, 10, 20, CSSPrimitiveValue::CSS_PX);

    RefPtrWillBeRawPtr<CSSPrimitiveValue> firstPc = CSSPrimitiveValue::create(30, CSSPrimitiveValue::CSS_PERCENTAGE);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> secondPc = CSSPrimitiveValue::create(-30, CSSPrimitiveValue::CSS_PERCENTAGE);
    RefPtrWillBeRawPtr<Pair> pairPc = Pair::create(firstPc, secondPc, Pair::KeepIdenticalValues);
    RefPtrWillBeRawPtr<CSSValue> value6 = roundTrip(CSSPrimitiveValue::create(pairPc.release()), RangeNonNegative);
    testPrimitiveValue(value6, 30, 0, CSSPrimitiveValue::CSS_PERCENTAGE);
}

}
