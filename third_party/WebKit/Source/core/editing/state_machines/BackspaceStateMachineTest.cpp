// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/state_machines/BackspaceStateMachine.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {
const TextSegmentationMachineState kNeedMoreCodeUnit = TextSegmentationMachineState::NeedMoreCodeUnit;
const TextSegmentationMachineState kFinished = TextSegmentationMachineState::Finished;
} // namespace

TEST(BackspaceStateMachineTest, DoNothingCase)
{
    BackspaceStateMachine machine;
    EXPECT_EQ(0, machine.finalizeAndGetBoundaryOffset());
    EXPECT_EQ(0, machine.finalizeAndGetBoundaryOffset());
}

TEST(BackspaceStateMachineTest, SingleCharacter)
{
    BackspaceStateMachine machine;
    EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit('a'));
    EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
    EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

    machine.reset();
    EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit('-'));
    EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
    EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

    machine.reset();
    EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit('\t'));
    EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
    EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

    machine.reset();
    // U+3042 HIRAGANA LETTER A.
    EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(0x3042));
    EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
    EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
}

TEST(BackspaceStateMachineTest, SurrogatePair)
{
    BackspaceStateMachine machine;

    // U+1F5FA(WORLD MAP) is \uD83D\uDDFA in UTF-16.
    uint16_t leadSurrogate = 0xD83D;
    uint16_t trailSurrogate = 0xDDFA;

    EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(trailSurrogate));
    EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(leadSurrogate));
    EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
    EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

    // Edge cases
    // Unpaired trailing surrogate. Delete only broken trail surrogate.
    machine.reset();
    EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(trailSurrogate));
    EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
    EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

    machine.reset();
    EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(trailSurrogate));
    EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit('a'));
    EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
    EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

    machine.reset();
    EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(trailSurrogate));
    EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(trailSurrogate));
    EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
    EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

    // Unpaired leading surrogate. Delete only broken lead surrogate.
    machine.reset();
    EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(leadSurrogate));
    EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
    EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
}

} // namespace blink
