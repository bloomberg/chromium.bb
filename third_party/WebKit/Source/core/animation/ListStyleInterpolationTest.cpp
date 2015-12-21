// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/ListStyleInterpolation.h"

#include "core/animation/LengthStyleInterpolation.h"
#include "testing/gtest/include/gtest/gtest.h"

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

} // namespace blink
