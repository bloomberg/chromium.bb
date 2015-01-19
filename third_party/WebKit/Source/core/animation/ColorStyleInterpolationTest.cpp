// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/ColorStyleInterpolation.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/StylePropertySet.h"
#include "platform/graphics/Color.h"

#include <gtest/gtest.h>

namespace blink {

class AnimationColorStyleInterpolationTest : public ::testing::Test {
protected:
    static PassOwnPtrWillBeRawPtr<InterpolableValue> colorToInterpolableValue(const CSSValue& value)
    {
        return ColorStyleInterpolation::colorToInterpolableValue(value);
    }

    static PassRefPtrWillBeRawPtr<CSSValue> interpolableValueToColor(InterpolableValue* value)
    {
        return ColorStyleInterpolation::interpolableValueToColor(*value);
    }

    static PassRefPtrWillBeRawPtr<CSSValue> roundTrip(PassRefPtrWillBeRawPtr<CSSValue> value)
    {
        return interpolableValueToColor(colorToInterpolableValue(*value).get());
    }

    static void testPrimitiveValue(PassRefPtrWillBeRawPtr<CSSValue> value, RGBA32 rgbaValue)
    {
        EXPECT_TRUE(value->isPrimitiveValue());
        EXPECT_EQ(toCSSPrimitiveValue(value.get())->getRGBA32Value(), rgbaValue);
    }

    static InterpolableValue* interpolationValue(Interpolation& interpolation)
    {
        return interpolation.getCachedValueForTesting();
    }
};

TEST_F(AnimationColorStyleInterpolationTest, Color)
{
    RefPtrWillBeRawPtr<CSSValue> value = roundTrip(CSSPrimitiveValue::createColor(makeRGBA(54, 48, 214, 64)));
    testPrimitiveValue(value, makeRGBA(54, 48, 214, 64));
}

TEST_F(AnimationColorStyleInterpolationTest, ClampedColor)
{
    RefPtrWillBeRawPtr<CSSValue> value = roundTrip(CSSPrimitiveValue::createColor(makeRGBA(-10, -10, -10, -10)));
    testPrimitiveValue(value, makeRGBA(-10, -10, -10, -10));

    value = roundTrip(CSSPrimitiveValue::createColor(makeRGBA(-260, -260, -260, -260)));
    testPrimitiveValue(value, makeRGBA(-260, -260, -260, -260));
}

TEST_F(AnimationColorStyleInterpolationTest, ZeroAlpha)
{
    RefPtrWillBeRawPtr<CSSValue> value = roundTrip(CSSPrimitiveValue::createColor(makeRGBA(54, 58, 214, 0)));
    testPrimitiveValue(value, Color::transparent);
}

TEST_F(AnimationColorStyleInterpolationTest, ValueIDColor)
{
    RefPtrWillBeRawPtr<CSSValue> value = roundTrip(CSSPrimitiveValue::createIdentifier(CSSValueID::CSSValueBlue));
    testPrimitiveValue(value, makeRGB(0, 0, 255));
}

TEST_F(AnimationColorStyleInterpolationTest, Interpolation)
{
    RefPtrWillBeRawPtr<Interpolation> interpolation = Interpolation::create(
        colorToInterpolableValue(*CSSPrimitiveValue::createColor(makeRGBA(0, 0, 0, 255))),
        colorToInterpolableValue(*CSSPrimitiveValue::createColor(makeRGBA(255, 255, 255, 255)))
    );

    interpolation->interpolate(0, 0.5);
    RefPtrWillBeRawPtr<CSSValue> value = interpolableValueToColor(interpolationValue(*interpolation));

    testPrimitiveValue(value, makeRGBA(128, 128, 128, 255));
}
}
