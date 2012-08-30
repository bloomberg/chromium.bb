// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include <public/WebAnimation.h>

#include <gtest/gtest.h>
#include <public/WebFloatAnimationCurve.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

using namespace WebKit;

namespace {

// Linux/Win bots failed on this test.
// https://bugs.webkit.org/show_bug.cgi?id=90651
#if OS(WINDOWS)
#define MAYBE_DefaultSettings DISABLED_DefaultSettings
#elif OS(LINUX)
#define MAYBE_DefaultSettings DISABLED_DefaultSettings
#else
#define MAYBE_DefaultSettings DefaultSettings
#endif
TEST(WebAnimationTest, MAYBE_DefaultSettings)
{
    OwnPtr<WebAnimationCurve> curve = adoptPtr(WebFloatAnimationCurve::create());
    OwnPtr<WebAnimation> animation = adoptPtr(WebAnimation::create(*curve, WebAnimation::TargetPropertyOpacity));

    // Ensure that the defaults are correct.
    EXPECT_EQ(1, animation->iterations());
    EXPECT_EQ(0, animation->startTime());
    EXPECT_EQ(0, animation->timeOffset());
    EXPECT_FALSE(animation->alternatesDirection());
}

// Linux/Win bots failed on this test.
// https://bugs.webkit.org/show_bug.cgi?id=90651
#if OS(WINDOWS)
#define MAYBE_ModifiedSettings DISABLED_ModifiedSettings
#elif OS(LINUX)
#define MAYBE_ModifiedSettings DISABLED_ModifiedSettings
#else
#define MAYBE_ModifiedSettings ModifiedSettings
#endif
TEST(WebAnimationTest, MAYBE_ModifiedSettings)
{
    OwnPtr<WebFloatAnimationCurve> curve = adoptPtr(WebFloatAnimationCurve::create());
    OwnPtr<WebAnimation> animation = adoptPtr(WebAnimation::create(*curve, WebAnimation::TargetPropertyOpacity));
    animation->setIterations(2);
    animation->setStartTime(2);
    animation->setTimeOffset(2);
    animation->setAlternatesDirection(true);

    EXPECT_EQ(2, animation->iterations());
    EXPECT_EQ(2, animation->startTime());
    EXPECT_EQ(2, animation->timeOffset());
    EXPECT_TRUE(animation->alternatesDirection());
}

} // namespace
