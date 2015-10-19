// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/ColorStyleInterpolation.h"

#include "core/css/CSSColorValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/StylePropertySet.h"
#include "platform/graphics/Color.h"

#include <gtest/gtest.h>

namespace blink {

class AnimationColorStyleInterpolationTest : public ::testing::Test {
protected:
    static PassOwnPtr<InterpolableValue> colorToInterpolableValue(const CSSValue& value)
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

    static void testColorValue(PassRefPtrWillBeRawPtr<CSSValue> value, RGBA32 rgbaValue)
    {
        EXPECT_TRUE(value->isColorValue());
        EXPECT_EQ(toCSSColorValue(*value).value(), rgbaValue);
    }

    static InterpolableValue* interpolationValue(Interpolation& interpolation)
    {
        return interpolation.getCachedValueForTesting();
    }
};

TEST_F(AnimationColorStyleInterpolationTest, Color)
{
    RefPtrWillBeRawPtr<CSSValue> value = roundTrip(CSSColorValue::create(makeRGBA(54, 48, 214, 64)));
    testColorValue(value, makeRGBA(54, 48, 214, 64));
}

TEST_F(AnimationColorStyleInterpolationTest, ClampedColor)
{
    RefPtrWillBeRawPtr<CSSValue> value = roundTrip(CSSColorValue::create(makeRGBA(-10, -10, -10, -10)));
    testColorValue(value, makeRGBA(-10, -10, -10, -10));

    value = roundTrip(CSSColorValue::create(makeRGBA(-260, -260, -260, -260)));
    testColorValue(value, makeRGBA(-260, -260, -260, -260));
}

TEST_F(AnimationColorStyleInterpolationTest, ZeroAlpha)
{
    RefPtrWillBeRawPtr<CSSValue> value = roundTrip(CSSColorValue::create(makeRGBA(54, 58, 214, 0)));
    testColorValue(value, Color::transparent);
}

TEST_F(AnimationColorStyleInterpolationTest, ValueIDColor)
{
    RefPtrWillBeRawPtr<CSSValue> value = roundTrip(CSSPrimitiveValue::createIdentifier(CSSValueID::CSSValueBlue));
    testColorValue(value, makeRGB(0, 0, 255));
}

TEST_F(AnimationColorStyleInterpolationTest, Interpolation)
{
    RefPtr<Interpolation> interpolation = ColorStyleInterpolation::create(
        *CSSColorValue::create(makeRGBA(0, 0, 0, 255)),
        *CSSColorValue::create(makeRGBA(255, 255, 255, 255)),
        CSSPropertyColor
    );

    interpolation->interpolate(0, 0.5);
    RefPtrWillBeRawPtr<CSSValue> value = interpolableValueToColor(interpolationValue(*interpolation));

    testColorValue(value, makeRGBA(128, 128, 128, 255));
}
}
