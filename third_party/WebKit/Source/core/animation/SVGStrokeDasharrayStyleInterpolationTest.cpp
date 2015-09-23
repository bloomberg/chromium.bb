// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/SVGStrokeDasharrayStyleInterpolation.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValueList.h"

#include <gtest/gtest.h>

namespace blink {

class AnimationSVGStrokeDasharrayStyleInterpolationTest : public ::testing::Test {
protected:
    static PassRefPtrWillBeRawPtr<CSSValueList> interpolableValueToStrokeDasharray(const InterpolableValue& value)
    {
        return SVGStrokeDasharrayStyleInterpolation::interpolableValueToStrokeDasharray(value);
    }

    static PassRefPtrWillBeRawPtr<CSSValueList> roundTrip(const CSSValue& value)
    {
        RefPtr<SVGStrokeDasharrayStyleInterpolation> interpolation = SVGStrokeDasharrayStyleInterpolation::maybeCreate(value, value, CSSPropertyStrokeDasharray);
        ASSERT(interpolation);
        return SVGStrokeDasharrayStyleInterpolation::interpolableValueToStrokeDasharray(*interpolation->m_start);
    }

    static void testPrimitiveValue(const CSSValue& cssValue, double value, CSSPrimitiveValue::UnitType unitType)
    {
        EXPECT_TRUE(cssValue.isPrimitiveValue());
        const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(cssValue);

        EXPECT_EQ(primitiveValue.typeWithCalcResolved(), unitType);
        EXPECT_EQ(primitiveValue.getDoubleValue(), value);
    }
};

TEST_F(AnimationSVGStrokeDasharrayStyleInterpolationTest, Zero)
{
    RefPtrWillBeRawPtr<CSSValueList> start = CSSValueList::createCommaSeparated();
    start->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Percentage));
    start->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Ems));
    start->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Pixels));

    RefPtrWillBeRawPtr<CSSValueList> value = roundTrip(*start);
    EXPECT_EQ(value->length(), 3u);
    testPrimitiveValue(*value->item(0), 0, CSSPrimitiveValue::UnitType::Percentage);
    testPrimitiveValue(*value->item(1), 0, CSSPrimitiveValue::UnitType::Ems);
    testPrimitiveValue(*value->item(2), 0, CSSPrimitiveValue::UnitType::Pixels);
}

TEST_F(AnimationSVGStrokeDasharrayStyleInterpolationTest, SingleUnit)
{
    RefPtrWillBeRawPtr<CSSValueList> start = CSSValueList::createCommaSeparated();
    start->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Pixels));
    start->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::UnitType::Ems));
    start->append(CSSPrimitiveValue::create(-20, CSSPrimitiveValue::UnitType::Ems));
    start->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Percentage));
    start->append(CSSPrimitiveValue::create(30, CSSPrimitiveValue::UnitType::Ems));

    RefPtrWillBeRawPtr<CSSValueList> value = roundTrip(*start);
    EXPECT_EQ(value->length(), 5u);
    testPrimitiveValue(*value->item(0), 0, CSSPrimitiveValue::UnitType::Pixels);
    testPrimitiveValue(*value->item(1), 10, CSSPrimitiveValue::UnitType::Ems);
    testPrimitiveValue(*value->item(2), 0, CSSPrimitiveValue::UnitType::Ems);
    testPrimitiveValue(*value->item(3), 0, CSSPrimitiveValue::UnitType::Percentage);
    testPrimitiveValue(*value->item(4), 30, CSSPrimitiveValue::UnitType::Ems);
}

TEST_F(AnimationSVGStrokeDasharrayStyleInterpolationTest, None)
{
    RefPtrWillBeRawPtr<CSSPrimitiveValue> start = CSSPrimitiveValue::createIdentifier(CSSValueNone);

    RefPtrWillBeRawPtr<CSSValueList> value = roundTrip(*start);
    EXPECT_EQ(value->length(), 1u);
    testPrimitiveValue(*value->item(0), 0, CSSPrimitiveValue::UnitType::Pixels);
}

}
