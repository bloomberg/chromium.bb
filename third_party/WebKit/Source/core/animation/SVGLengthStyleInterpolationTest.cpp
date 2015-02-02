// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/SVGLengthStyleInterpolation.h"

#include "core/css/CSSPrimitiveValue.h"

#include <gtest/gtest.h>

namespace blink {

class AnimationSVGLengthStyleInterpolationTest : public ::testing::Test {
protected:
    static PassOwnPtrWillBeRawPtr<InterpolableValue> lengthToInterpolableValue(const CSSPrimitiveValue& value)
    {
        return SVGLengthStyleInterpolation::lengthToInterpolableValue(value);
    }

    static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> interpolableValueToLength(const InterpolableValue& value, CSSPrimitiveValue::UnitType type, InterpolationRange range)
    {
        return SVGLengthStyleInterpolation::interpolableValueToLength(value, type, range);
    }

    static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> roundTrip(const CSSPrimitiveValue& value, InterpolationRange range = RangeAll)
    {
        return interpolableValueToLength(*lengthToInterpolableValue(value), value.primitiveType(), range);
    }

    static void testPrimitiveValue(const CSSPrimitiveValue& primitiveValue, double value, CSSPrimitiveValue::UnitType unitType)
    {
        EXPECT_EQ(primitiveValue.primitiveType(), unitType);
        EXPECT_EQ(primitiveValue.getDoubleValue(), value);
    }
};

TEST_F(AnimationSVGLengthStyleInterpolationTest, ZeroLength)
{
    RefPtrWillBeRawPtr<CSSPrimitiveValue> value = roundTrip(*CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PX));
    testPrimitiveValue(*value, 0, CSSPrimitiveValue::CSS_PX);

    value = roundTrip(*CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PERCENTAGE));
    testPrimitiveValue(*value, 0, CSSPrimitiveValue::CSS_PERCENTAGE);

    value = roundTrip(*CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_EMS));
    testPrimitiveValue(*value, 0, CSSPrimitiveValue::CSS_EMS);
}

TEST_F(AnimationSVGLengthStyleInterpolationTest, SingleUnit)
{
    RefPtrWillBeRawPtr<CSSPrimitiveValue> value = roundTrip(*CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_PX));
    testPrimitiveValue(*value, 10, CSSPrimitiveValue::CSS_PX);

    value = roundTrip(*CSSPrimitiveValue::create(40, CSSPrimitiveValue::CSS_PERCENTAGE), RangeNonNegative);
    testPrimitiveValue(*value, 40, CSSPrimitiveValue::CSS_PERCENTAGE);

    value = roundTrip(*CSSPrimitiveValue::create(-10, CSSPrimitiveValue::CSS_EMS));
    testPrimitiveValue(*value, -10, CSSPrimitiveValue::CSS_EMS);
}

TEST_F(AnimationSVGLengthStyleInterpolationTest, SingleClampedUnit)
{
    RefPtrWillBeRawPtr<CSSPrimitiveValue> value = roundTrip(*CSSPrimitiveValue::create(-10, CSSPrimitiveValue::CSS_PX), RangeNonNegative);
    testPrimitiveValue(*value, 0, CSSPrimitiveValue::CSS_PX);

    value = roundTrip(*CSSPrimitiveValue::create(-40, CSSPrimitiveValue::CSS_PERCENTAGE), RangeNonNegative);
    testPrimitiveValue(*value, 0, CSSPrimitiveValue::CSS_PERCENTAGE);
}

}
