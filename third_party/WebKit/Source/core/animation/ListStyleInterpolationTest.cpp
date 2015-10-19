// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/ListStyleInterpolation.h"

#include "core/animation/LengthStyleInterpolation.h"
#include "core/animation/ShadowStyleInterpolation.h"
#include "core/css/CSSColorValue.h"

#include <gtest/gtest.h>

namespace blink {

class ListStyleInterpolationTest : public ::testing::Test {

protected:
    static PassRefPtrWillBeRawPtr<CSSValue> lengthRoundTrip(PassRefPtrWillBeRawPtr<CSSValue> value, InterpolationRange range)
    {
        return ListStyleInterpolationImpl<LengthStyleInterpolation, void>::interpolableValueToList(
            ListStyleInterpolationImpl<LengthStyleInterpolation, void>::listToInterpolableValue(*value).get(), range);
    }

    static void compareLengthLists(PassRefPtrWillBeRawPtr<CSSValueList> expectedList, PassRefPtrWillBeRawPtr<CSSValue> actualList)
    {
        ASSERT(actualList->isValueList());

        for (size_t i = 0; i < 10; i++) {
            CSSValue* currentExpectedValue = expectedList->item(i);
            CSSValue* currentActualValue = toCSSValueList(*actualList).item(i);
            ASSERT(currentExpectedValue->isPrimitiveValue());
            ASSERT(currentActualValue->isPrimitiveValue());

            EXPECT_EQ(toCSSPrimitiveValue(currentExpectedValue)->getDoubleValue(), toCSSPrimitiveValue(currentActualValue)->getDoubleValue());
            EXPECT_EQ(toCSSPrimitiveValue(currentExpectedValue)->getDoubleValue(), toCSSPrimitiveValue(currentActualValue)->getDoubleValue());
        }
    }

    static PassRefPtrWillBeRawPtr<CSSValue> shadowRoundTrip(PassRefPtrWillBeRawPtr<CSSValue> value)
    {
        Vector<bool> nonInterpolableData;
        return ListStyleInterpolationImpl<ShadowStyleInterpolation, bool>::interpolableValueToList(
            ListStyleInterpolationImpl<ShadowStyleInterpolation, bool>::listToInterpolableValue(*value, &nonInterpolableData).get(), nonInterpolableData);
    }

    static RefPtrWillBeRawPtr<CSSShadowValue> createShadowValue()
    {
        RefPtrWillBeRawPtr<CSSColorValue> color = CSSColorValue::create(makeRGBA(112, 123, 175, 255));
        RefPtrWillBeRawPtr<CSSPrimitiveValue> x = CSSPrimitiveValue::create(10, CSSPrimitiveValue::UnitType::Pixels);
        RefPtrWillBeRawPtr<CSSPrimitiveValue> y = CSSPrimitiveValue::create(20, CSSPrimitiveValue::UnitType::Pixels);
        RefPtrWillBeRawPtr<CSSPrimitiveValue> blur = CSSPrimitiveValue::create(30, CSSPrimitiveValue::UnitType::Pixels);
        RefPtrWillBeRawPtr<CSSPrimitiveValue> spread = CSSPrimitiveValue::create(40, CSSPrimitiveValue::UnitType::Pixels);

        return CSSShadowValue::create(x, y, blur, spread, CSSPrimitiveValue::createIdentifier(CSSValueNone), color);

    }

    static void compareShadowList(RefPtrWillBeRawPtr<CSSValueList> expectedList, RefPtrWillBeRawPtr<CSSValue> actualList)
    {
        ASSERT(actualList->isValueList());

        for (size_t i = 0; i < 10; i++) {

            CSSShadowValue* expectedShadow = toCSSShadowValue(expectedList->item(i));
            CSSShadowValue* actualShadow = toCSSShadowValue(toCSSValueList(*actualList).item(i));

            EXPECT_EQ(expectedShadow->x->getDoubleValue(), actualShadow->x->getDoubleValue());
            EXPECT_EQ(expectedShadow->y->getDoubleValue(), actualShadow->y->getDoubleValue());
            EXPECT_EQ(expectedShadow->blur->getDoubleValue(), actualShadow->blur->getDoubleValue());
            EXPECT_EQ(expectedShadow->spread->getDoubleValue(), actualShadow->spread->getDoubleValue());
            EXPECT_EQ(toCSSColorValue(*expectedShadow->color).value(), toCSSColorValue(*actualShadow->color).value());

            EXPECT_EQ(expectedShadow->style->getValueID(), actualShadow->style->getValueID());
        }
    }
};

TEST_F(ListStyleInterpolationTest, LengthListMultipleValuesTest)
{
    RefPtrWillBeRawPtr<CSSValueList> expectedList = CSSValueList::createCommaSeparated();
    for (size_t i = 0; i < 10; i++) {
        RefPtrWillBeRawPtr<CSSPrimitiveValue> lengthValue = CSSPrimitiveValue::create(static_cast<double>(i), CSSPrimitiveValue::UnitType::Pixels);
        expectedList->append(lengthValue);
    }

    compareLengthLists(expectedList, lengthRoundTrip(expectedList, RangeNonNegative));
}

TEST_F(ListStyleInterpolationTest, ShadowListMultipleValuesTest)
{
    RefPtrWillBeRawPtr<CSSValueList> expectedList = CSSValueList::createCommaSeparated();
    RefPtrWillBeRawPtr<CSSShadowValue> shadowValue = createShadowValue();
    for (size_t i = 0; i < 10; i++) {
        expectedList->append(shadowValue);
    }

    compareShadowList(expectedList, shadowRoundTrip(expectedList));
}

} // namespace blink
