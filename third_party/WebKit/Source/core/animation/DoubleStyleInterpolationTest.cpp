// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/DoubleStyleInterpolation.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/StylePropertySet.h"

#include <gtest/gtest.h>

namespace blink {

class AnimationDoubleStyleInterpolationTest : public ::testing::Test {
protected:
    static PassOwnPtrWillBeRawPtr<InterpolableValue> doubleToInterpolableValue(const CSSValue& value)
    {
        return DoubleStyleInterpolation::doubleToInterpolableValue(value);
    }

    static PassRefPtrWillBeRawPtr<CSSValue> interpolableValueToDouble(InterpolableValue* value, ClampRange clamp)
    {
        return DoubleStyleInterpolation::interpolableValueToDouble(value, clamp);
    }

    static PassRefPtrWillBeRawPtr<CSSValue> roundTrip(PassRefPtrWillBeRawPtr<CSSValue> value)
    {
        return interpolableValueToDouble(doubleToInterpolableValue(*value).get(), NoClamp);
    }

    static void testPrimitiveValue(RefPtrWillBeRawPtr<CSSValue> value, double doubleValue, CSSPrimitiveValue::UnitType unitType)
    {
        EXPECT_TRUE(value->isPrimitiveValue());
        EXPECT_EQ(doubleValue, toCSSPrimitiveValue(value.get())->getDoubleValue());
        EXPECT_EQ(unitType, toCSSPrimitiveValue(value.get())->primitiveType());
    }

    static InterpolableValue* getCachedValue(Interpolation& interpolation)
    {
        return interpolation.getCachedValueForTesting();
    }
};

TEST_F(AnimationDoubleStyleInterpolationTest, ZeroValue)
{
    RefPtrWillBeRawPtr<CSSValue> value = roundTrip(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_NUMBER));
    testPrimitiveValue(value, 0, CSSPrimitiveValue::CSS_NUMBER);
}

TEST_F(AnimationDoubleStyleInterpolationTest, Value)
{
    RefPtrWillBeRawPtr<CSSValue> value = roundTrip(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_NUMBER));
    testPrimitiveValue(value, 10, CSSPrimitiveValue::CSS_NUMBER);
}

TEST_F(AnimationDoubleStyleInterpolationTest, Clamping)
{
    RefPtrWillBeRawPtr<Interpolation> interpolableDouble = Interpolation::create(InterpolableNumber::create(0), InterpolableNumber::create(0.6));
    interpolableDouble->interpolate(0, 0.4);
    // progVal = start*(1-prog) + end*prog
    EXPECT_EQ(0.24, toInterpolableNumber(getCachedValue(*interpolableDouble))->value());
}

}
