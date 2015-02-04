// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/ShadowStyleInterpolation.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSShadowValue.h"
#include "core/css/CSSValue.h"
#include "core/css/resolver/StyleBuilder.h"
#include "platform/graphics/Color.h"

#include <gtest/gtest.h>

namespace blink {

class AnimationShadowStyleInterpolationTest : public ::testing::Test {

protected:

    static PassOwnPtrWillBeRawPtr<InterpolableValue> shadowToInterpolableValue(const CSSValue& value, bool styleFlag)
    {
        return ShadowStyleInterpolation::shadowToInterpolableValue(value, styleFlag);
    }

    static PassRefPtrWillBeRawPtr<CSSValue> interpolableValueToShadow(InterpolableValue* value, bool styleFlag)
    {
        return ShadowStyleInterpolation::interpolableValueToShadow(*value, styleFlag);
    }

    static PassRefPtrWillBeRawPtr<CSSValue> roundTrip(PassRefPtrWillBeRawPtr<CSSValue> value, bool styleFlag)
    {
        return interpolableValueToShadow(shadowToInterpolableValue(*value, styleFlag).get(), styleFlag);
    }

    static void testPrimitiveValues(RefPtrWillBeRawPtr<CSSValue> value, double xExpected, double yExpected, double blurExpected, double spreadExpected,
        RGBA32 colorExpected, RefPtrWillBeRawPtr<CSSPrimitiveValue> idExpected, CSSPrimitiveValue::UnitType unitType)
    {
        EXPECT_TRUE(value->isShadowValue());

        CSSShadowValue& shadowValue = toCSSShadowValue(*value);

        EXPECT_EQ(xExpected, shadowValue.x->getDoubleValue());
        EXPECT_EQ(yExpected, shadowValue.y->getDoubleValue());
        EXPECT_EQ(blurExpected, shadowValue.blur->getDoubleValue());
        EXPECT_EQ(spreadExpected, shadowValue.spread->getDoubleValue());
        EXPECT_EQ(colorExpected, shadowValue.color->getRGBA32Value());

        EXPECT_EQ(idExpected->getValueID(), shadowValue.style->getValueID());

        EXPECT_EQ(shadowValue.x->primitiveType(), unitType);
        EXPECT_EQ(shadowValue.y->primitiveType(), unitType);
        EXPECT_EQ(shadowValue.blur->primitiveType(), unitType);
        EXPECT_EQ(shadowValue.spread->primitiveType(), unitType);
    }

};

TEST_F(AnimationShadowStyleInterpolationTest, ZeroTest)
{
    RefPtrWillBeRawPtr<CSSPrimitiveValue> color = CSSPrimitiveValue::createColor(makeRGBA(0, 0, 0, 0));
    RefPtrWillBeRawPtr<CSSPrimitiveValue> x = CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PX);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> y = CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PX);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> blur = CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PX);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> spread = CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PX);

    RefPtrWillBeRawPtr<CSSShadowValue> shadowValue = CSSShadowValue::create(x, y, blur, spread, CSSPrimitiveValue::createIdentifier(CSSValueNone), color);

    RefPtrWillBeRawPtr<CSSValue> value = roundTrip(shadowValue.release(), false);

    testPrimitiveValues(value, 0, 0, 0, 0, makeRGBA(0, 0, 0, 0), CSSPrimitiveValue::createIdentifier(CSSValueNone), CSSPrimitiveValue::CSS_PX);
}

TEST_F(AnimationShadowStyleInterpolationTest, MultipleValueNonPixelTest)
{
    RefPtrWillBeRawPtr<CSSPrimitiveValue> color = CSSPrimitiveValue::createColor(makeRGBA(112, 123, 175, 255));
    RefPtrWillBeRawPtr<CSSPrimitiveValue> x = CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_EMS);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> y = CSSPrimitiveValue::create(20, CSSPrimitiveValue::CSS_EMS);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> blur = CSSPrimitiveValue::create(30, CSSPrimitiveValue::CSS_EMS);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> spread = CSSPrimitiveValue::create(40, CSSPrimitiveValue::CSS_EMS);

    RefPtrWillBeRawPtr<CSSShadowValue> shadowValue = CSSShadowValue::create(x, y, blur, spread, CSSPrimitiveValue::createIdentifier(CSSValueNone), color);

    RefPtrWillBeRawPtr<CSSValue> value = roundTrip(shadowValue.release(), false);

    testPrimitiveValues(value, 10, 20, 30, 40, makeRGBA(112, 123, 175, 255), CSSPrimitiveValue::createIdentifier(CSSValueNone), CSSPrimitiveValue::CSS_EMS);
}

TEST_F(AnimationShadowStyleInterpolationTest, InsetShadowTest)
{
    RefPtrWillBeRawPtr<CSSPrimitiveValue> color = CSSPrimitiveValue::createColor(makeRGBA(54, 48, 214, 64));
    RefPtrWillBeRawPtr<CSSPrimitiveValue> x = CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_PX);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> y = CSSPrimitiveValue::create(20, CSSPrimitiveValue::CSS_PX);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> blur = CSSPrimitiveValue::create(30, CSSPrimitiveValue::CSS_PX);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> spread = CSSPrimitiveValue::create(40, CSSPrimitiveValue::CSS_PX);

    RefPtrWillBeRawPtr<CSSShadowValue> shadowValue = CSSShadowValue::create(x, y, blur, spread, CSSPrimitiveValue::createIdentifier(CSSValueInset), color);

    RefPtrWillBeRawPtr<CSSValue> value = roundTrip(shadowValue.release(), true);

    testPrimitiveValues(value, 10, 20, 30, 40, makeRGBA(54, 48, 214, 64), CSSPrimitiveValue::createIdentifier(CSSValueInset), CSSPrimitiveValue::CSS_PX);
}

}
