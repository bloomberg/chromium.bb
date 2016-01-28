// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebMediaConstraints.h"
#include "testing/gtest/include/gtest/gtest.h"

// The MediaTrackConstraintsTest group tests the types declared in
// WebKit/public/platform/WebMediaConstraints.h
TEST(MediaTrackConstraintsTest, LongConstraint)
{
    blink::LongConstraint rangeConstraint(nullptr);
    rangeConstraint.setMin(5);
    rangeConstraint.setMax(6);
    EXPECT_TRUE(rangeConstraint.matches(5));
    EXPECT_TRUE(rangeConstraint.matches(6));
    EXPECT_FALSE(rangeConstraint.matches(4));
    EXPECT_FALSE(rangeConstraint.matches(7));
    blink::LongConstraint exactConstraint(nullptr);
    exactConstraint.setExact(5);
    EXPECT_FALSE(exactConstraint.matches(4));
    EXPECT_TRUE(exactConstraint.matches(5));
    EXPECT_FALSE(exactConstraint.matches(6));
}

TEST(MediaTrackConstraintsTest, DoubleConstraint)
{
    blink::DoubleConstraint rangeConstraint(nullptr);
    EXPECT_TRUE(rangeConstraint.isEmpty());
    rangeConstraint.setMin(5.0);
    rangeConstraint.setMax(6.5);
    EXPECT_FALSE(rangeConstraint.isEmpty());
    // Matching within epsilon
    EXPECT_TRUE(rangeConstraint.matches(5.0 - blink::DoubleConstraint::kConstraintEpsilon / 2));
    EXPECT_TRUE(rangeConstraint.matches(6.5 + blink::DoubleConstraint::kConstraintEpsilon / 2));
    blink::DoubleConstraint exactConstraint(nullptr);
    exactConstraint.setExact(5.0);
    EXPECT_FALSE(rangeConstraint.isEmpty());
    EXPECT_FALSE(exactConstraint.matches(4.9));
    EXPECT_TRUE(exactConstraint.matches(5.0));
    EXPECT_TRUE(exactConstraint.matches(5.0 - blink::DoubleConstraint::kConstraintEpsilon / 2));
    EXPECT_TRUE(exactConstraint.matches(5.0 + blink::DoubleConstraint::kConstraintEpsilon / 2));
    EXPECT_FALSE(exactConstraint.matches(5.1));
}

TEST(MediaTrackConstraintsTest, BooleanConstraint)
{
    blink::BooleanConstraint boolConstraint(nullptr);
    EXPECT_TRUE(boolConstraint.isEmpty());
    EXPECT_TRUE(boolConstraint.matches(false));
    EXPECT_TRUE(boolConstraint.matches(true));
    boolConstraint.setExact(false);
    EXPECT_FALSE(boolConstraint.isEmpty());
    EXPECT_FALSE(boolConstraint.matches(true));
    EXPECT_TRUE(boolConstraint.matches(false));
    boolConstraint.setExact(true);
    EXPECT_FALSE(boolConstraint.matches(false));
    EXPECT_TRUE(boolConstraint.matches(true));
}

TEST(MediaTrackConstraintsTest, ConstraintSetEmpty)
{
    blink::WebMediaTrackConstraintSet theSet;
    EXPECT_TRUE(theSet.isEmpty());
    theSet.echoCancellation.setExact(false);
    EXPECT_FALSE(theSet.isEmpty());
}

TEST(MediaTrackConstraintsTest, ConstraintName)
{
    const char* theName = "name";
    blink::BooleanConstraint boolConstraint(theName);
    EXPECT_EQ(theName, boolConstraint.name());
}

TEST(MediaTrackConstraintsTest, MandatoryChecks)
{
    blink::WebMediaTrackConstraintSet theSet;
    std::string foundName;
    EXPECT_FALSE(theSet.hasMandatory());
    EXPECT_FALSE(theSet.hasMandatoryOutsideSet({ "width" }, foundName));
    EXPECT_FALSE(theSet.width.hasMandatory());
    theSet.width.setMax(240);
    EXPECT_TRUE(theSet.width.hasMandatory());
    EXPECT_TRUE(theSet.hasMandatory());
    EXPECT_FALSE(theSet.hasMandatoryOutsideSet({ "width" }, foundName));
    EXPECT_TRUE(theSet.hasMandatoryOutsideSet({ "height" }, foundName));
    EXPECT_EQ("width", foundName);
    theSet.googPayloadPadding.setExact(true);
    EXPECT_TRUE(theSet.hasMandatoryOutsideSet({ "width" }, foundName));
    EXPECT_EQ("googPayloadPadding", foundName);
}
