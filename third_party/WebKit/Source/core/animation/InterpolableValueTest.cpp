// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/InterpolableValue.h"

#include "core/animation/Interpolation.h"

#include <gtest/gtest.h>

namespace blink {

class AnimationInterpolableValueTest : public ::testing::Test {
protected:
    InterpolableValue* interpolationValue(Interpolation& interpolation)
    {
        return interpolation.getCachedValueForTesting();
    }

    double interpolateNumbers(double a, double b, double progress)
    {
        RefPtrWillBeRawPtr<Interpolation> i = Interpolation::create(InterpolableNumber::create(a), InterpolableNumber::create(b));
        i->interpolate(0, progress);
        return toInterpolableNumber(interpolationValue(*i.get()))->value();
    }

    bool interpolateBools(bool a, bool b, double progress)
    {
        RefPtrWillBeRawPtr<Interpolation> i = Interpolation::create(InterpolableBool::create(a), InterpolableBool::create(b));
        i->interpolate(0, progress);
        return toInterpolableBool(interpolationValue(*i.get()))->value();
    }

    PassRefPtrWillBeRawPtr<Interpolation> interpolateLists(PassOwnPtrWillBeRawPtr<InterpolableList> listA, PassOwnPtrWillBeRawPtr<InterpolableList> listB, double progress)
    {
        RefPtrWillBeRawPtr<Interpolation> i = Interpolation::create(listA, listB);
        i->interpolate(0, progress);
        return i;
    }

    double addNumbers(double a, double b)
    {
        OwnPtrWillBeRawPtr<InterpolableValue> numA = InterpolableNumber::create(a);
        OwnPtrWillBeRawPtr<InterpolableValue> numB = InterpolableNumber::create(b);

        OwnPtrWillBeRawPtr<InterpolableValue> resultNumber = InterpolableNumber::create(0);

        numA->add(*numB, *resultNumber);

        return toInterpolableNumber(resultNumber.get())->value();
    }

    bool addBools(bool a, bool b)
    {
        OwnPtrWillBeRawPtr<InterpolableValue> boolA = InterpolableBool::create(a);
        OwnPtrWillBeRawPtr<InterpolableValue> boolB = InterpolableBool::create(b);

        OwnPtrWillBeRawPtr<InterpolableValue> resultBool = InterpolableBool::create(false);

        boolA->add(*boolB, *resultBool);

        return toInterpolableBool(resultBool.get())->value();
    }

    PassOwnPtrWillBeRawPtr<InterpolableList> addLists(PassOwnPtrWillBeRawPtr<InterpolableValue> listA, PassOwnPtrWillBeRawPtr<InterpolableValue> listB)
    {
        OwnPtrWillBeRawPtr<InterpolableList> result = InterpolableList::create(toInterpolableList(*listA));

        listA->add(*listB, *result);

        return result.release();
    }

    double multiplyNumber(double scalar, double n)
    {
        OwnPtrWillBeRawPtr<InterpolableValue> num = InterpolableNumber::create(n);
        OwnPtrWillBeRawPtr<InterpolableValue> result = InterpolableNumber::create(0);

        num->multiply(scalar, *result);

        return toInterpolableNumber(result.get())->value();
    }

    PassOwnPtrWillBeRawPtr<InterpolableList> multiplyList(double scalar, PassOwnPtrWillBeRawPtr<InterpolableValue> list)
    {
        OwnPtrWillBeRawPtr<InterpolableList> result = InterpolableList::create(toInterpolableList(*list));

        list->multiply(scalar, *result);

        return result.release();
    }
};

TEST_F(AnimationInterpolableValueTest, InterpolateNumbers)
{
    EXPECT_FLOAT_EQ(126, interpolateNumbers(42, 0, -2));
    EXPECT_FLOAT_EQ(42, interpolateNumbers(42, 0, 0));
    EXPECT_FLOAT_EQ(29.4f, interpolateNumbers(42, 0, 0.3));
    EXPECT_FLOAT_EQ(21, interpolateNumbers(42, 0, 0.5));
    EXPECT_FLOAT_EQ(0, interpolateNumbers(42, 0, 1));
    EXPECT_FLOAT_EQ(-21, interpolateNumbers(42, 0, 1.5));
}

TEST_F(AnimationInterpolableValueTest, InterpolateBools)
{
    EXPECT_FALSE(interpolateBools(false, true, -1));
    EXPECT_FALSE(interpolateBools(false, true, 0));
    EXPECT_FALSE(interpolateBools(false, true, 0.3));
    EXPECT_TRUE(interpolateBools(false, true, 0.5));
    EXPECT_TRUE(interpolateBools(false, true, 1));
    EXPECT_TRUE(interpolateBools(false, true, 2));
}

TEST_F(AnimationInterpolableValueTest, SimpleList)
{
    OwnPtrWillBeRawPtr<InterpolableList> listA = InterpolableList::create(3);
    listA->set(0, InterpolableNumber::create(0));
    listA->set(1, InterpolableNumber::create(42));
    listA->set(2, InterpolableNumber::create(20.5));

    OwnPtrWillBeRawPtr<InterpolableList> listB = InterpolableList::create(3);
    listB->set(0, InterpolableNumber::create(100));
    listB->set(1, InterpolableNumber::create(-200));
    listB->set(2, InterpolableNumber::create(300));

    RefPtrWillBeRawPtr<Interpolation> i = interpolateLists(listA.release(), listB.release(), 0.3);
    InterpolableList* outList = toInterpolableList(interpolationValue(*i.get()));
    EXPECT_FLOAT_EQ(30, toInterpolableNumber(outList->get(0))->value());
    EXPECT_FLOAT_EQ(-30.6f, toInterpolableNumber(outList->get(1))->value());
    EXPECT_FLOAT_EQ(104.35f, toInterpolableNumber(outList->get(2))->value());
}

TEST_F(AnimationInterpolableValueTest, NestedList)
{
    OwnPtrWillBeRawPtr<InterpolableList> listA = InterpolableList::create(3);
    listA->set(0, InterpolableNumber::create(0));
    OwnPtrWillBeRawPtr<InterpolableList> subListA = InterpolableList::create(1);
    subListA->set(0, InterpolableNumber::create(100));
    listA->set(1, subListA.release());
    listA->set(2, InterpolableBool::create(false));

    OwnPtrWillBeRawPtr<InterpolableList> listB = InterpolableList::create(3);
    listB->set(0, InterpolableNumber::create(100));
    OwnPtrWillBeRawPtr<InterpolableList> subListB = InterpolableList::create(1);
    subListB->set(0, InterpolableNumber::create(50));
    listB->set(1, subListB.release());
    listB->set(2, InterpolableBool::create(true));

    RefPtrWillBeRawPtr<Interpolation> i = interpolateLists(listA.release(), listB.release(), 0.5);
    InterpolableList* outList = toInterpolableList(interpolationValue(*i.get()));
    EXPECT_FLOAT_EQ(50, toInterpolableNumber(outList->get(0))->value());
    EXPECT_FLOAT_EQ(75, toInterpolableNumber(toInterpolableList(outList->get(1))->get(0))->value());
    EXPECT_TRUE(toInterpolableBool(outList->get(2))->value());
}

TEST_F(AnimationInterpolableValueTest, AddNumbers)
{
    EXPECT_FLOAT_EQ(42, addNumbers(20, 22));
    EXPECT_FLOAT_EQ(42, addNumbers(22, 20));
    EXPECT_FLOAT_EQ(50, addNumbers(0, 50));
    EXPECT_FLOAT_EQ(50, addNumbers(50, 0));
    EXPECT_FLOAT_EQ(32, addNumbers(42, -10));
    EXPECT_FLOAT_EQ(32, addNumbers(-10, 42));
    EXPECT_FLOAT_EQ(-32, addNumbers(-42, 10));
    EXPECT_FLOAT_EQ(-32, addNumbers(10, -42));
}

TEST_F(AnimationInterpolableValueTest, AddBools)
{
    EXPECT_FALSE(addBools(false, false));
    EXPECT_TRUE(addBools(true, false));
    EXPECT_TRUE(addBools(false, true));
    EXPECT_TRUE(addBools(true, true));
}

TEST_F(AnimationInterpolableValueTest, AddLists)
{
    OwnPtrWillBeRawPtr<InterpolableList> listA = InterpolableList::create(3);
    listA->set(0, InterpolableNumber::create(31));
    listA->set(1, InterpolableNumber::create(-20));
    listA->set(2, InterpolableNumber::create(42));

    OwnPtrWillBeRawPtr<InterpolableList> listB = InterpolableList::create(3);
    listB->set(0, InterpolableNumber::create(20));
    listB->set(1, InterpolableNumber::create(43));
    listB->set(2, InterpolableNumber::create(-60));

    OwnPtrWillBeRawPtr<InterpolableList> result = addLists(listA.release(), listB.release());
    EXPECT_FLOAT_EQ(51, toInterpolableNumber(result->get(0))->value());
    EXPECT_FLOAT_EQ(23, toInterpolableNumber(result->get(1))->value());
    EXPECT_FLOAT_EQ(-18, toInterpolableNumber(result->get(2))->value());
}

TEST_F(AnimationInterpolableValueTest, AddNestedLists)
{
    OwnPtrWillBeRawPtr<InterpolableList> listA = InterpolableList::create(3);
    listA->set(0, InterpolableNumber::create(19));
    OwnPtrWillBeRawPtr<InterpolableList> subListA = InterpolableList::create(1);
    subListA->set(0, InterpolableNumber::create(67));
    listA->set(1, subListA.release());
    listA->set(2, InterpolableBool::create(false));

    OwnPtrWillBeRawPtr<InterpolableList> listB = InterpolableList::create(3);
    listB->set(0, InterpolableNumber::create(21));
    OwnPtrWillBeRawPtr<InterpolableList> subListB = InterpolableList::create(1);
    subListB->set(0, InterpolableNumber::create(31));
    listB->set(1, subListB.release());
    listB->set(2, InterpolableBool::create(true));

    OwnPtrWillBeRawPtr<InterpolableList> result = addLists(listA.release(), listB.release());
    EXPECT_FLOAT_EQ(40, toInterpolableNumber(result->get(0))->value());
    EXPECT_FLOAT_EQ(98, toInterpolableNumber(toInterpolableList(result->get(1))->get(0))->value());
    EXPECT_TRUE(toInterpolableBool(result->get(2))->value());
}

TEST_F(AnimationInterpolableValueTest, NumberScalarMultiplication)
{
    EXPECT_FLOAT_EQ(42, multiplyNumber(6, 7));
    EXPECT_FLOAT_EQ(-20, multiplyNumber(-2, 10));
    EXPECT_FLOAT_EQ(-42, multiplyNumber(21, -2));
    EXPECT_FLOAT_EQ(20, multiplyNumber(-4, -5));

    EXPECT_FLOAT_EQ(0, multiplyNumber(0, 10));
    EXPECT_FLOAT_EQ(0, multiplyNumber(10, 0));
}

TEST_F(AnimationInterpolableValueTest, ListScalarMultiplication)
{
    OwnPtrWillBeRawPtr<InterpolableList> listA = InterpolableList::create(3);
    listA->set(0, InterpolableNumber::create(5));
    OwnPtrWillBeRawPtr<InterpolableList> subListA = InterpolableList::create(2);
    subListA->set(0, InterpolableNumber::create(4));
    subListA->set(1, InterpolableNumber::create(7));
    listA->set(1, subListA.release());
    listA->set(2, InterpolableNumber::create(3));

    OwnPtrWillBeRawPtr<InterpolableList> resultA = multiplyList(6, listA.release());
    EXPECT_FLOAT_EQ(30, toInterpolableNumber(resultA->get(0))->value());
    EXPECT_FLOAT_EQ(24, toInterpolableNumber(toInterpolableList(resultA->get(1))->get(0))->value());
    EXPECT_FLOAT_EQ(42, toInterpolableNumber(toInterpolableList(resultA->get(1))->get(1))->value());
    EXPECT_FLOAT_EQ(18, toInterpolableNumber(resultA->get(2))->value());

    OwnPtrWillBeRawPtr<InterpolableList> listB = InterpolableList::create(3);
    listB->set(0, InterpolableNumber::create(8));
    listB->set(1, InterpolableNumber::create(-10));
    listB->set(2, InterpolableNumber::create(9));

    OwnPtrWillBeRawPtr<InterpolableList> resultB = multiplyList(0, listB.release());
    EXPECT_FLOAT_EQ(0, toInterpolableNumber(resultB->get(0))->value());
    EXPECT_FLOAT_EQ(0, toInterpolableNumber(resultB->get(1))->value());
    EXPECT_FLOAT_EQ(0, toInterpolableNumber(resultB->get(2))->value());
}

}
