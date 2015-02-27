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
    static PassRefPtrWillBeRawPtr<CSSValueList> interpolableValueToStrokeDasharray(const InterpolableValue& value, const Vector<CSSPrimitiveValue::UnitType>& types)
    {
        return SVGStrokeDasharrayStyleInterpolation::interpolableValueToStrokeDasharray(value, types);
    }

    static PassRefPtrWillBeRawPtr<CSSValueList> roundTrip(const CSSValue& value, const Vector<CSSPrimitiveValue::UnitType>& types)
    {
        RefPtrWillBeRawPtr<SVGStrokeDasharrayStyleInterpolation> interpolation = SVGStrokeDasharrayStyleInterpolation::maybeCreate(value, value, CSSPropertyStrokeDasharray);
        ASSERT(interpolation);
        return SVGStrokeDasharrayStyleInterpolation::interpolableValueToStrokeDasharray(*interpolation->m_start, types);
    }

    static void testPrimitiveValue(const CSSValue& cssValue, double value, CSSPrimitiveValue::UnitType unitType)
    {
        EXPECT_TRUE(cssValue.isPrimitiveValue());
        const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(cssValue);

        EXPECT_EQ(primitiveValue.primitiveType(), unitType);
        EXPECT_EQ(primitiveValue.getDoubleValue(), value);
    }
};

TEST_F(AnimationSVGStrokeDasharrayStyleInterpolationTest, Zero)
{
    RefPtrWillBeRawPtr<CSSValueList> start = CSSValueList::createCommaSeparated();
    start->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PERCENTAGE));
    start->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_EMS));
    start->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PX));

    Vector<CSSPrimitiveValue::UnitType> types(3);
    types[0] = CSSPrimitiveValue::CSS_PERCENTAGE;
    types[1] = CSSPrimitiveValue::CSS_EMS;
    types[2] = CSSPrimitiveValue::CSS_PX;
    RefPtrWillBeRawPtr<CSSValueList> value = roundTrip(*start, types);
    EXPECT_EQ(value->length(), 3u);
    testPrimitiveValue(*value->item(0), 0, CSSPrimitiveValue::CSS_PERCENTAGE);
    testPrimitiveValue(*value->item(1), 0, CSSPrimitiveValue::CSS_EMS);
    testPrimitiveValue(*value->item(2), 0, CSSPrimitiveValue::CSS_PX);
}

TEST_F(AnimationSVGStrokeDasharrayStyleInterpolationTest, SingleUnit)
{
    RefPtrWillBeRawPtr<CSSValueList> start = CSSValueList::createCommaSeparated();
    start->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PX));
    start->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_EMS));
    start->append(CSSPrimitiveValue::create(-20, CSSPrimitiveValue::CSS_EMS));
    start->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PERCENTAGE));
    start->append(CSSPrimitiveValue::create(30, CSSPrimitiveValue::CSS_EMS));

    Vector<CSSPrimitiveValue::UnitType> types(5);
    types[0] = CSSPrimitiveValue::CSS_PX;
    types[1] = CSSPrimitiveValue::CSS_EMS;
    types[2] = CSSPrimitiveValue::CSS_EMS;
    types[3] = CSSPrimitiveValue::CSS_PERCENTAGE;
    types[4] = CSSPrimitiveValue::CSS_EMS;
    RefPtrWillBeRawPtr<CSSValueList> value = roundTrip(*start, types);
    EXPECT_EQ(value->length(), 5u);
    testPrimitiveValue(*value->item(0), 0, CSSPrimitiveValue::CSS_PX);
    testPrimitiveValue(*value->item(1), 10, CSSPrimitiveValue::CSS_EMS);
    testPrimitiveValue(*value->item(2), 0, CSSPrimitiveValue::CSS_EMS);
    testPrimitiveValue(*value->item(3), 0, CSSPrimitiveValue::CSS_PERCENTAGE);
    testPrimitiveValue(*value->item(4), 30, CSSPrimitiveValue::CSS_EMS);
}

TEST_F(AnimationSVGStrokeDasharrayStyleInterpolationTest, None)
{
    RefPtrWillBeRawPtr<CSSPrimitiveValue> start = CSSPrimitiveValue::createIdentifier(CSSValueNone);

    Vector<CSSPrimitiveValue::UnitType> types(1);
    types[0] = CSSPrimitiveValue::CSS_PX;
    RefPtrWillBeRawPtr<CSSValueList> value = roundTrip(*start, types);
    EXPECT_EQ(value->length(), 1u);
    testPrimitiveValue(*value->item(0), 0, CSSPrimitiveValue::CSS_PX);
}

}
