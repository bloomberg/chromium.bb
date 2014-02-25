/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/animation/AnimationClock.h"

#include "wtf/OwnPtr.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class AnimationAnimationClockTest : public ::testing::Test {
protected:
    virtual void SetUp()
    {
        animationClock = AnimationClock::create(mockTimeFunction);
        mockTime = 200;
    }

    static double mockTimeFunction()
    {
        return mockTime++;
    }

    static double mockTime;
    OwnPtr<AnimationClock> animationClock;
};

double AnimationAnimationClockTest::mockTime;

TEST_F(AnimationAnimationClockTest, CurrentTime)
{
    EXPECT_EQ(200, animationClock->currentTime());
    EXPECT_EQ(200, animationClock->currentTime());
    animationClock->unfreeze();
    EXPECT_EQ(201, animationClock->currentTime());
    EXPECT_EQ(201, animationClock->currentTime());
}

TEST_F(AnimationAnimationClockTest, UpdateTime)
{
    animationClock->updateTime(100);
    EXPECT_EQ(100, animationClock->currentTime());
    EXPECT_EQ(100, animationClock->currentTime());
    animationClock->updateTime(150);
    EXPECT_EQ(150, animationClock->currentTime());
    EXPECT_EQ(150, animationClock->currentTime());
}

}
