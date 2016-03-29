// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/BackspaceStateMachine.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(BackspaceStateMachineTest, DoNothingCase)
{
    BackspaceStateMachine machine;
    EXPECT_EQ(0, machine.finalizeAndGetCodeUnitCountToBeDeleted());
    EXPECT_EQ(0, machine.finalizeAndGetCodeUnitCountToBeDeleted());
}

TEST(BackspaceStateMachineTest, SingleCharacter)
{
    BackspaceStateMachine machine;
    EXPECT_TRUE(machine.updateState('a'));
    EXPECT_EQ(1, machine.finalizeAndGetCodeUnitCountToBeDeleted());
    EXPECT_EQ(1, machine.finalizeAndGetCodeUnitCountToBeDeleted());

    machine.reset();
    EXPECT_TRUE(machine.updateState('-'));
    EXPECT_EQ(1, machine.finalizeAndGetCodeUnitCountToBeDeleted());
    EXPECT_EQ(1, machine.finalizeAndGetCodeUnitCountToBeDeleted());

    machine.reset();
    EXPECT_TRUE(machine.updateState('\t'));
    EXPECT_EQ(1, machine.finalizeAndGetCodeUnitCountToBeDeleted());
    EXPECT_EQ(1, machine.finalizeAndGetCodeUnitCountToBeDeleted());

    machine.reset();
    // U+3042 HIRAGANA LETTER A.
    EXPECT_TRUE(machine.updateState(0x3042));
    EXPECT_EQ(1, machine.finalizeAndGetCodeUnitCountToBeDeleted());
    EXPECT_EQ(1, machine.finalizeAndGetCodeUnitCountToBeDeleted());
}

TEST(BackspaceStateMachineTest, SurrogatePair)
{
    BackspaceStateMachine machine;

    // U+1F5FA(WORLD MAP) is \uD83D\uDDFA in UTF-16.
    uint16_t leadSurrogate = 0xD83D;
    uint16_t trailSurrogate = 0xDDFA;

    EXPECT_FALSE(machine.updateState(trailSurrogate));
    EXPECT_TRUE(machine.updateState(leadSurrogate));
    EXPECT_EQ(2, machine.finalizeAndGetCodeUnitCountToBeDeleted());
    EXPECT_EQ(2, machine.finalizeAndGetCodeUnitCountToBeDeleted());

    // Edge cases
    // Unpaired trailing surrogate. Delete only broken trail surrogate.
    machine.reset();
    EXPECT_FALSE(machine.updateState(trailSurrogate));
    EXPECT_EQ(1, machine.finalizeAndGetCodeUnitCountToBeDeleted());
    EXPECT_EQ(1, machine.finalizeAndGetCodeUnitCountToBeDeleted());

    machine.reset();
    EXPECT_FALSE(machine.updateState(trailSurrogate));
    EXPECT_TRUE(machine.updateState('a'));
    EXPECT_EQ(1, machine.finalizeAndGetCodeUnitCountToBeDeleted());
    EXPECT_EQ(1, machine.finalizeAndGetCodeUnitCountToBeDeleted());

    machine.reset();
    EXPECT_FALSE(machine.updateState(trailSurrogate));
    EXPECT_TRUE(machine.updateState(trailSurrogate));
    EXPECT_EQ(1, machine.finalizeAndGetCodeUnitCountToBeDeleted());
    EXPECT_EQ(1, machine.finalizeAndGetCodeUnitCountToBeDeleted());

    // Unpaired leading surrogate. Delete only broken lead surrogate.
    machine.reset();
    EXPECT_TRUE(machine.updateState(leadSurrogate));
    EXPECT_EQ(1, machine.finalizeAndGetCodeUnitCountToBeDeleted());
    EXPECT_EQ(1, machine.finalizeAndGetCodeUnitCountToBeDeleted());
}

} // namespace blink
