#include "core/animation/VisibilityStyleInterpolation.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/StylePropertySet.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class AnimationVisibilityStyleInterpolationTest : public ::testing::Test {
protected:
    static PassOwnPtr<InterpolableValue> visibilityToInterpolableValue(const CSSValue& value)
    {
        return VisibilityStyleInterpolation::visibilityToInterpolableValue(value);
    }

    static PassRefPtrWillBeRawPtr<CSSValue> interpolableValueToVisibility(InterpolableValue* value, CSSValueID notVisible)
    {
        return VisibilityStyleInterpolation::interpolableValueToVisibility(value, notVisible);
    }

    static PassRefPtrWillBeRawPtr<CSSValue> roundTrip(PassRefPtrWillBeRawPtr<CSSValue> value, CSSValueID valueID)
    {
        return interpolableValueToVisibility(visibilityToInterpolableValue(*value).get(), valueID);
    }

    static void testPrimitiveValue(PassRefPtrWillBeRawPtr<CSSValue> value, CSSValueID valueID)
    {
        EXPECT_TRUE(value->isPrimitiveValue());
        EXPECT_EQ(valueID, toCSSPrimitiveValue(value.get())->getValueID());
    }

    static InterpolableValue* getCachedValue(Interpolation& interpolation)
    {
        return interpolation.getCachedValueForTesting();
    }
};

TEST_F(AnimationVisibilityStyleInterpolationTest, ValueIDs)
{
    RefPtrWillBeRawPtr<CSSValue> value = roundTrip(CSSPrimitiveValue::createIdentifier(CSSValueVisible), CSSValueHidden);
    testPrimitiveValue(value, CSSValueVisible);

    value = roundTrip(CSSPrimitiveValue::createIdentifier(CSSValueCollapse), CSSValueCollapse);
    testPrimitiveValue(value, CSSValueCollapse);

    value = roundTrip(CSSPrimitiveValue::createIdentifier(CSSValueHidden), CSSValueHidden);
    testPrimitiveValue(value, CSSValueHidden);
}

TEST_F(AnimationVisibilityStyleInterpolationTest, Interpolation)
{
    RefPtr<Interpolation> interpolation = VisibilityStyleInterpolation::create(
        *CSSPrimitiveValue::createIdentifier(CSSValueHidden),
        *CSSPrimitiveValue::createIdentifier(CSSValueVisible),
        CSSPropertyVisibility);

    interpolation->interpolate(0, 0.0);
    RefPtrWillBeRawPtr<CSSValue> value = interpolableValueToVisibility(getCachedValue(*interpolation), CSSValueHidden);
    testPrimitiveValue(value, CSSValueHidden);

    interpolation->interpolate(0, 0.5);
    value = interpolableValueToVisibility(getCachedValue(*interpolation), CSSValueHidden);
    testPrimitiveValue(value, CSSValueVisible);
}
}
