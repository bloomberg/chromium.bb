// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "public/platform/WebMediaConstraints.h"
#include "testing/gtest/include/gtest/gtest.h"

// The MediaStreamConstraintsTest group tests the templates of
// WebKit/public/platform/WebMediaConstraints.h
TEST(MediaTrackConstraintsTest, LongConstraint)
{
    blink::LongConstraint rangeConstraint;
    rangeConstraint.setMin(5);
    rangeConstraint.setMax(6);
    EXPECT_TRUE(rangeConstraint.matches(5));
    EXPECT_TRUE(rangeConstraint.matches(6));
    EXPECT_FALSE(rangeConstraint.matches(4));
    EXPECT_FALSE(rangeConstraint.matches(7));
    blink::LongConstraint exactConstraint;
    exactConstraint.setExact(5);
    EXPECT_FALSE(exactConstraint.matches(4));
    EXPECT_TRUE(exactConstraint.matches(5));
    EXPECT_FALSE(exactConstraint.matches(6));
}

TEST(MediaTrackConstraintsTest, DoubleConstraint)
{
    blink::DoubleConstraint rangeConstraint;
    rangeConstraint.setMin(5.0);
    rangeConstraint.setMax(6.5);
    // Matching within epsilon
    EXPECT_TRUE(rangeConstraint.matches(5.0 - blink::DoubleConstraint::kConstraintEpsilon / 2));
    EXPECT_TRUE(rangeConstraint.matches(6.5 + blink::DoubleConstraint::kConstraintEpsilon / 2));
    blink::DoubleConstraint exactConstraint;
    exactConstraint.setExact(5.0);
    EXPECT_FALSE(exactConstraint.matches(4.9));
    EXPECT_TRUE(exactConstraint.matches(5.0));
    EXPECT_TRUE(exactConstraint.matches(5.0 - blink::DoubleConstraint::kConstraintEpsilon / 2));
    EXPECT_TRUE(exactConstraint.matches(5.0 + blink::DoubleConstraint::kConstraintEpsilon / 2));
    EXPECT_FALSE(exactConstraint.matches(5.1));
}

TEST(MediaTrackConstraintsTest, BooleanConstraint)
{
    blink::BooleanConstraint boolConstraint;
    EXPECT_TRUE(boolConstraint.matches(false));
    EXPECT_TRUE(boolConstraint.matches(true));
    boolConstraint.setExact(false);
    EXPECT_FALSE(boolConstraint.matches(true));
    EXPECT_TRUE(boolConstraint.matches(false));
    boolConstraint.setExact(true);
    EXPECT_FALSE(boolConstraint.matches(false));
    EXPECT_TRUE(boolConstraint.matches(true));
}
