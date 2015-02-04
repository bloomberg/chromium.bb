// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/LengthStyleInterpolation.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/StylePropertySet.h"

#include <gtest/gtest.h>

namespace blink {

class AnimationLengthStyleInterpolationTest : public ::testing::Test {
protected:
    static PassOwnPtrWillBeRawPtr<InterpolableValue> lengthToInterpolableValue(const CSSValue& value)
    {
        return LengthStyleInterpolation::toInterpolableValue(value);
    }

    static PassRefPtrWillBeRawPtr<CSSValue> interpolableValueToLength(InterpolableValue* value, InterpolationRange range)
    {
        return LengthStyleInterpolation::fromInterpolableValue(*value, range);
    }

    static PassRefPtrWillBeRawPtr<CSSValue> roundTrip(PassRefPtrWillBeRawPtr<CSSValue> value)
    {
        return interpolableValueToLength(lengthToInterpolableValue(*value).get(), RangeAll);
    }

    static void testPrimitiveValue(RefPtrWillBeRawPtr<CSSValue> value, double doubleValue, CSSPrimitiveValue::UnitType unitType)
    {
        EXPECT_TRUE(value->isPrimitiveValue());
        EXPECT_EQ(doubleValue, toCSSPrimitiveValue(value.get())->getDoubleValue());
        EXPECT_EQ(unitType, toCSSPrimitiveValue(value.get())->primitiveType());
    }

    static PassOwnPtrWillBeRawPtr<InterpolableList> createInterpolableLength(double a, double b, double c, double d, double e, double f, double g, double h, double i, double j)
    {
        OwnPtrWillBeRawPtr<InterpolableList> list = InterpolableList::create(10);
        list->set(0, InterpolableNumber::create(a));
        list->set(1, InterpolableNumber::create(b));
        list->set(2, InterpolableNumber::create(c));
        list->set(3, InterpolableNumber::create(d));
        list->set(4, InterpolableNumber::create(e));
        list->set(5, InterpolableNumber::create(f));
        list->set(6, InterpolableNumber::create(g));
        list->set(7, InterpolableNumber::create(h));
        list->set(8, InterpolableNumber::create(i));
        list->set(9, InterpolableNumber::create(j));

        return list.release();
    }

    void initLengthArray(CSSLengthArray& lengthArray)
    {
        lengthArray.resize(CSSPrimitiveValue::LengthUnitTypeCount);
        for (size_t i = 0; i < CSSPrimitiveValue::LengthUnitTypeCount; ++i)
            lengthArray.at(i) = 0;
    }

    CSSLengthArray& setLengthArray(CSSLengthArray& lengthArray, String text)
    {
        CSSPrimitiveValue::CSSLengthTypeArray lengthTypeArray;
        initLengthArray(lengthArray);
        RefPtrWillBeRawPtr<MutableStylePropertySet> propertySet = MutableStylePropertySet::create();
        propertySet->setProperty(CSSPropertyLeft, text);
        toCSSPrimitiveValue(propertySet->getPropertyCSSValue(CSSPropertyLeft).get())->accumulateLengthArray(lengthArray);
        return lengthArray;
    }

    bool lengthArraysEqual(CSSLengthArray& a, CSSLengthArray& b)
    {
        for (size_t i = 0; i < CSSPrimitiveValue::LengthUnitTypeCount; ++i) {
            if (a.at(i) != b.at(i))
                return false;
        }
        return true;
    }
};

TEST_F(AnimationLengthStyleInterpolationTest, ZeroLength)
{
    RefPtrWillBeRawPtr<CSSValue> value1 = roundTrip(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PX));
    testPrimitiveValue(value1, 0, CSSPrimitiveValue::CSS_PX);

    RefPtrWillBeRawPtr<CSSValue> value2 = roundTrip(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PERCENTAGE));
    testPrimitiveValue(value2, 0, CSSPrimitiveValue::CSS_PERCENTAGE);

    RefPtrWillBeRawPtr<CSSValue> value3 = roundTrip(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_EMS));
    testPrimitiveValue(value3, 0, CSSPrimitiveValue::CSS_EMS);

}

TEST_F(AnimationLengthStyleInterpolationTest, SingleUnit)
{
    RefPtrWillBeRawPtr<CSSValue> value = roundTrip(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_PX));
    testPrimitiveValue(value, 10, CSSPrimitiveValue::CSS_PX);

    value = roundTrip(CSSPrimitiveValue::create(30, CSSPrimitiveValue::CSS_PERCENTAGE));
    testPrimitiveValue(value, 30, CSSPrimitiveValue::CSS_PERCENTAGE);

    value = roundTrip(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_EMS));
    testPrimitiveValue(value, 10, CSSPrimitiveValue::CSS_EMS);
}

TEST_F(AnimationLengthStyleInterpolationTest, SingleClampedUnit)
{
    RefPtrWillBeRawPtr<CSSValue> value1 = CSSPrimitiveValue::create(-10, CSSPrimitiveValue::CSS_PX);
    value1 = interpolableValueToLength(lengthToInterpolableValue(*value1).get(), RangeNonNegative);
    testPrimitiveValue(value1, 0, CSSPrimitiveValue::CSS_PX);

    RefPtrWillBeRawPtr<CSSValue> value2 = CSSPrimitiveValue::create(-10, CSSPrimitiveValue::CSS_EMS);
    value2 = interpolableValueToLength(lengthToInterpolableValue(*value2).get(), RangeNonNegative);
    testPrimitiveValue(value2, 0, CSSPrimitiveValue::CSS_EMS);
}

TEST_F(AnimationLengthStyleInterpolationTest, MultipleUnits)
{
    CSSLengthArray expectation, actual;
    initLengthArray(expectation);
    OwnPtrWillBeRawPtr<InterpolableList> result = InterpolableList::create(2);
    result->set(0, createInterpolableLength(0, 10, 0, 10, 0, 10, 0, 10, 0, 10));
    result->set(1, createInterpolableLength(0, 1, 0, 1, 0, 1, 0, 1, 0, 1));
    toCSSPrimitiveValue(interpolableValueToLength(result.get(), RangeAll).get())->accumulateLengthArray(expectation);
    EXPECT_TRUE(lengthArraysEqual(expectation, setLengthArray(actual, "calc(10% + 10ex + 10ch + 10vh + 10vmax)")));
}

TEST_F(AnimationLengthStyleInterpolationTest, MultipleUnitsWithSingleValues)
{
    CSSLengthArray expectation, actual;
    initLengthArray(expectation);
    OwnPtrWillBeRawPtr<InterpolableList> result = InterpolableList::create(2);
    result->set(0, createInterpolableLength(0, 10, 0, 10, 0, 10, 0, 10, 0, 10));
    result->set(1, createInterpolableLength(0, 1, 0, 1, 0, 1, 0, 1, 0, 1));
    toCSSPrimitiveValue(interpolableValueToLength(result.get(), RangeAll).get())->accumulateLengthArray(expectation);
    EXPECT_TRUE(lengthArraysEqual(expectation, setLengthArray(actual, "calc(10% + 10ex + 10ch + 10vh + 10vmax)")));

}

TEST_F(AnimationLengthStyleInterpolationTest, MultipleUnitsWithMultipleValues)
{
    CSSLengthArray expectation, actual;
    initLengthArray(expectation);
    OwnPtrWillBeRawPtr<InterpolableList> result = InterpolableList::create(2);
    result->set(0, createInterpolableLength(0, 20, 0, 30, 0, 8, 0, 10, 0, 7));
    result->set(1, createInterpolableLength(0, 1, 0, 1, 0, 1, 0, 1, 0, 1));
    toCSSPrimitiveValue(interpolableValueToLength(result.get(), RangeAll).get())->accumulateLengthArray(expectation);
    EXPECT_TRUE(lengthArraysEqual(expectation, setLengthArray(actual, "calc(20% + 30ex + 8ch + 10vh + 7vmax)")));

}

TEST_F(AnimationLengthStyleInterpolationTest, MultipleUnitsWithZeroValue)
{
    CSSLengthArray expectation, actual;
    initLengthArray(expectation);
    OwnPtrWillBeRawPtr<InterpolableList> result = InterpolableList::create(2);
    result->set(0, createInterpolableLength(0, 10, 0, 10, 0, 10, 0, 10, 0, 10));
    result->set(1, createInterpolableLength(1, 1, 0, 1, 0, 1, 0, 1, 0, 1));
    toCSSPrimitiveValue(interpolableValueToLength(result.get(), RangeAll).get())->accumulateLengthArray(expectation);
    EXPECT_TRUE(lengthArraysEqual(expectation, setLengthArray(actual, "calc(0px + 10% + 10ex + 10ch + 10vh + 10vmax)")));
}

TEST_F(AnimationLengthStyleInterpolationTest, MultipleUnitsWithZeroValues)
{
    CSSLengthArray expectation, actual;
    initLengthArray(expectation);
    OwnPtrWillBeRawPtr<InterpolableList> result = InterpolableList::create(2);
    result->set(0, createInterpolableLength(0, 10, 0, 10, 0, 10, 0, 10, 0, 10));
    result->set(1, createInterpolableLength(1, 1, 1, 1, 0, 1, 0, 1, 1, 1));
    toCSSPrimitiveValue(interpolableValueToLength(result.get(), RangeAll).get())->accumulateLengthArray(expectation);
    EXPECT_TRUE(lengthArraysEqual(expectation, setLengthArray(actual, "calc(0px + 10% + 0em + 10ex + 10ch + 10vh + 0vmin + 10vmax)")));
}

}
