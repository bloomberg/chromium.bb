// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/LengthPoint3DStyleInterpolation.h"

#include "core/animation/LengthStyleInterpolation.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/StylePropertySet.h"

#include "stdio.h"

#include <gtest/gtest.h>

namespace blink {

class AnimationLengthPoint3DStyleInterpolationTest : public ::testing::Test {
protected:
    static PassOwnPtrWillBeRawPtr<InterpolableValue> lengthPoint3DToInterpolableValue(const CSSValue& value)
    {
        return LengthPoint3DStyleInterpolation::lengthPoint3DtoInterpolableValue(value);
    }

    static PassRefPtrWillBeRawPtr<CSSValue> interpolableValueToLengthPoint3D(InterpolableValue* value)
    {
        return LengthPoint3DStyleInterpolation::interpolableValueToLengthPoint3D(value);
    }

    static PassRefPtrWillBeRawPtr<CSSValue> roundTrip(PassRefPtrWillBeRawPtr<CSSValue> value)
    {
        return interpolableValueToLengthPoint3D(lengthPoint3DToInterpolableValue(*value).get());
    }

    static void testPrimitiveValue(RefPtrWillBeRawPtr<CSSValue> value, double x, double y, double z, CSSPrimitiveValue::UnitType unitTypeX, CSSPrimitiveValue::UnitType unitTypeY, CSSPrimitiveValue::UnitType unitTypeZ)
    {
        EXPECT_TRUE(value->isValueList());
        const int sizeOfList = 3;
        const CSSPrimitiveValue* length[sizeOfList] = {
        toCSSPrimitiveValue(toCSSValueList(*value).item(0)),
        toCSSPrimitiveValue(toCSSValueList(*value).item(1)),
        toCSSPrimitiveValue(toCSSValueList(*value).item(2)) };

        EXPECT_EQ(length[0]->getDoubleValue(), x);
        EXPECT_EQ(length[1]->getDoubleValue(), y);
        EXPECT_EQ(length[2]->getDoubleValue(), z);

        EXPECT_EQ(unitTypeX, length[0]->primitiveType());
        EXPECT_EQ(unitTypeY, length[1]->primitiveType());
        EXPECT_EQ(unitTypeZ, length[2]->primitiveType());
    }
};

TEST_F(AnimationLengthPoint3DStyleInterpolationTest, ZeroPoint3D)
{
    RefPtrWillBeRawPtr<CSSValueList> lengthPoint3D = CSSValueList::createCommaSeparated();
    lengthPoint3D->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PX));
    lengthPoint3D->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_EMS));
    lengthPoint3D->append(CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PERCENTAGE));
    RefPtrWillBeRawPtr<CSSValue> value = roundTrip(lengthPoint3D.release());
    testPrimitiveValue(value, 0, 0, 0, CSSPrimitiveValue::CSS_PX, CSSPrimitiveValue::CSS_PX, CSSPrimitiveValue::CSS_PX);
}

TEST_F(AnimationLengthPoint3DStyleInterpolationTest, SingleUnitPoint3D)
{
    RefPtrWillBeRawPtr<CSSValueList> lengthPoint3D = CSSValueList::createCommaSeparated();
    lengthPoint3D->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_PX));
    lengthPoint3D->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_PERCENTAGE));
    lengthPoint3D->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_EMS));
    RefPtrWillBeRawPtr<CSSValue> value = roundTrip(lengthPoint3D.release());
    testPrimitiveValue(value, 10, 10, 10, CSSPrimitiveValue::CSS_PX, CSSPrimitiveValue::CSS_PERCENTAGE, CSSPrimitiveValue::CSS_EMS);
}

TEST_F(AnimationLengthPoint3DStyleInterpolationTest, MultipleValuePoint3D)
{
    RefPtrWillBeRawPtr<CSSValueList> lengthPoint3DPx = CSSValueList::createCommaSeparated();
    lengthPoint3DPx->append(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_PX));
    lengthPoint3DPx->append(CSSPrimitiveValue::create(30, CSSPrimitiveValue::CSS_PERCENTAGE));
    lengthPoint3DPx->append(CSSPrimitiveValue::create(20, CSSPrimitiveValue::CSS_EMS));
    RefPtrWillBeRawPtr<CSSValue> value = roundTrip(lengthPoint3DPx.release());
    testPrimitiveValue(value, 10, 30, 20, CSSPrimitiveValue::CSS_PX, CSSPrimitiveValue::CSS_PERCENTAGE, CSSPrimitiveValue::CSS_EMS);
}

}
