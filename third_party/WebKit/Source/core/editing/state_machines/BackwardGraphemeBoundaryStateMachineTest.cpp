// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/state_machines/BackwardGraphemeBoundaryStateMachine.h"

#include "core/editing/state_machines/StateMachineTestUtil.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/CharacterNames.h"

namespace blink {

// Notations:
// SOT indicates start of text.
// [Lead] indicates broken lonely lead surrogate.
// [Trail] indicates broken lonely trail surrogate.
// [U] indicates regional indicator symbol U.
// [S] indicates regional indicator symbol S.

namespace {
// kWatch kVS16, kEye kVS16 are valid standardized variants.
const UChar32 kWatch = 0x231A;
const UChar32 kEye = WTF::Unicode::eyeCharacter;
const UChar32 kVS16 = 0xFE0F;

// kHanBMP KVS17, kHanSIP kVS17 are valie IVD sequences.
const UChar32 kHanBMP = 0x845B;
const UChar32 kHanSIP = 0x20000;
const UChar32 kVS17 = 0xE0100;

// Following lead/trail values are used for invalid surrogate pairs.
const UChar kLead = 0xD83D;
const UChar kTrail = 0xDC66;

// U+1F1FA is REGIONAL INDICATOR SYMBOL LETTER U
// U+1F1F8 is REGIONAL INDICATOR SYMBOL LETTER S
const UChar32 kRisU = 0x1F1FA;
const UChar32 kRisS = 0x1F1F8;
} // namespace

TEST(BackwardGraphemeBoundaryStatemachineTest, DoNothingCase)
{
    BackwardGraphemeBoundaryStateMachine machine;

    EXPECT_EQ(0, machine.finalizeAndGetBoundaryOffset());
    EXPECT_EQ(0, machine.finalizeAndGetBoundaryOffset());
}

TEST(BackwardGraphemeBoundaryStatemachineTest, BrokenSurrogatePair)
{
    BackwardGraphemeBoundaryStateMachine machine;

    // [Lead]
    EXPECT_EQ("F", processSequenceBackward(&machine, { kLead }));
    EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [Trail]
    EXPECT_EQ("RF", processSequenceBackward(&machine, { 'a', kTrail }));
    EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

    // [Trail] + [Trail]
    EXPECT_EQ("RF", processSequenceBackward(&machine, { kTrail, kTrail }));
    EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

    // SOT + [Trail]
    EXPECT_EQ("RF", processSequenceBackward(&machine, { kTrail }));
    EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
}

TEST(BackwardGraphemeBoundaryStatemachineTest, BreakImmediately_BMP)
{
    BackwardGraphemeBoundaryStateMachine machine;

    // U+0000 + U+0000
    EXPECT_EQ("RF", processSequenceBackward(&machine, { 0, 0 }));
    EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

    // 'a' + 'a'
    EXPECT_EQ("RF", processSequenceBackward(&machine, { 'a', 'a' }));
    EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

    // U+1F441 + 'a'
    EXPECT_EQ("RRF", processSequenceBackward(&machine, { kEye, 'a' }));
    EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

    // SOT + 'a'
    EXPECT_EQ("RF", processSequenceBackward(&machine, { 'a' }));
    EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

    // Broken surrogates.
    // [Lead] + 'a'
    EXPECT_EQ("RF", processSequenceBackward(&machine, { kLead, 'a' }));
    EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [Trail] + 'a'
    EXPECT_EQ("RRF", processSequenceBackward(&machine, { 'a', kTrail, 'a' }));
    EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

    // [Trail] + [Trail] + 'a'
    EXPECT_EQ("RRF", processSequenceBackward(&machine,
        { kTrail, kTrail, 'a' }));
    EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

    // SOT + [Trail] + 'a'
    EXPECT_EQ("RRF", processSequenceBackward(&machine, { kTrail, 'a' }));
    EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
}

TEST(BackwardGraphemeBoundaryStatemachineTest,
    BreakImmediately_SupplementaryPlane)
{
    BackwardGraphemeBoundaryStateMachine machine;

    // 'a' + U+1F441
    EXPECT_EQ("RRF", processSequenceBackward(&machine, { 'a', kEye }));
    EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

    // U+1F441 + U+1F441
    EXPECT_EQ("RRRF", processSequenceBackward(&machine, { kEye, kEye }));
    EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

    // SOT + U+1F441
    EXPECT_EQ("RRF", processSequenceBackward(&machine, { kEye }));
    EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

    // Broken surrogates.
    // [Lead] + U+1F441
    EXPECT_EQ("RRF", processSequenceBackward(&machine, { kLead, kEye }));
    EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [Trail] + U+1F441
    EXPECT_EQ("RRRF", processSequenceBackward(&machine, { 'a', kTrail, kEye }));
    EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

    // [Trail] + [Trail] + U+1F441
    EXPECT_EQ("RRRF", processSequenceBackward(&machine,
        { kTrail, kTrail, kEye }));
    EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

    // SOT + [Trail] + U+1F441
    EXPECT_EQ("RRRF", processSequenceBackward(&machine, { kTrail, kEye }));
    EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
}

TEST(BackwardGraphemeBoundaryStatemachineTest,
    NotBreakImmediatelyBefore_BMP_BMP)
{
    BackwardGraphemeBoundaryStateMachine machine;

    // 'a' + U+231A + U+FE0F
    EXPECT_EQ("RRF", processSequenceBackward(&machine,
        { 'a', kWatch, kVS16 }));
    EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

    // U+1F441 + U+231A + U+FE0F
    EXPECT_EQ("RRRF", processSequenceBackward(&machine,
        { kEye, kWatch, kVS16 }));
    EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

    // SOT + U+231A + U+FE0F
    EXPECT_EQ("RRF", processSequenceBackward(&machine, { kWatch, kVS16 }));
    EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

    // [Lead] + U+231A + U+FE0F
    EXPECT_EQ("RRF", processSequenceBackward(&machine,
        { kLead, kWatch, kVS16 }));
    EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [Trail] + U+231A + U+FE0F
    EXPECT_EQ("RRRF", processSequenceBackward(&machine,
        { 'a', kTrail, kWatch, kVS16 }));
    EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

    // [Trail] + [Trail] + U+231A + U+FE0F
    EXPECT_EQ("RRRF", processSequenceBackward(&machine,
        { kTrail, kTrail, kWatch, kVS16 }));
    EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

    // SOT + [Trail] + U+231A + U+FE0F
    EXPECT_EQ("RRRF", processSequenceBackward(&machine,
        { kTrail, kWatch, kVS16 }));
    EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
}

TEST(BackwardGraphemeBoundaryStatemachineTest,
    NotBreakImmediatelyBefore_Supplementary_BMP)
{
    BackwardGraphemeBoundaryStateMachine machine;

    // 'a' + U+1F441 + U+FE0F
    EXPECT_EQ("RRRF", processSequenceBackward(&machine, { 'a', kEye, kVS16 }));
    EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());

    // U+1F441 + U+1F441 + U+FE0F
    EXPECT_EQ("RRRRF", processSequenceBackward(&machine,
        { kEye, kEye, kVS16 }));
    EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());

    // SOT + U+1F441 + U+FE0F
    EXPECT_EQ("RRRF", processSequenceBackward(&machine, { kEye, kVS16 }));
    EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());

    // [Lead] + U+1F441 + U+FE0F
    EXPECT_EQ("RRRF", processSequenceBackward(&machine,
        { kLead, kEye, kVS16 }));
    EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [Trail] + U+1F441 + U+FE0F
    EXPECT_EQ("RRRRF", processSequenceBackward(&machine,
        { 'a', kTrail, kEye, kVS16 }));
    EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());

    // [Trail] + [Trail] + U+1F441 + U+FE0F
    EXPECT_EQ("RRRRF", processSequenceBackward(&machine,
        { kTrail, kTrail, kEye, kVS16 }));
    EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());

    // SOT + [Trail] + U+1F441 + U+FE0F
    EXPECT_EQ("RRRRF", processSequenceBackward(&machine,
        { kTrail, kEye, kVS16 }));
    EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());
}

TEST(BackwardGraphemeBoundaryStatemachineTest,
    NotBreakImmediatelyBefore_BMP_Supplementary)
{
    BackwardGraphemeBoundaryStateMachine machine;

    // 'a' + U+845B + U+E0100
    EXPECT_EQ("RRRF", processSequenceBackward(&machine,
        { 'a', kHanBMP, kVS17 }));
    EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());

    // U+1F441 + U+845B + U+E0100
    EXPECT_EQ("RRRRF", processSequenceBackward(&machine,
        { kEye, kHanBMP, kVS17 }));
    EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());

    // SOT + U+845B + U+E0100
    EXPECT_EQ("RRRF", processSequenceBackward(&machine,
        { kHanBMP, kVS17 }));
    EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());

    // [Lead] + U+845B + U+E0100
    EXPECT_EQ("RRRF", processSequenceBackward(&machine,
        { kLead, kHanBMP, kVS17 }));
    EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [Trail] + U+845B + U+E0100
    EXPECT_EQ("RRRRF", processSequenceBackward(&machine,
        { 'a', kTrail, kHanBMP, kVS17 }));
    EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());

    // [Trail] + [Trail] + U+845B + U+E0100
    EXPECT_EQ("RRRRF", processSequenceBackward(&machine,
        { kTrail, kTrail, kHanBMP, kVS17 }));
    EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());

    // SOT + [Trail] + U+845B + U+E0100
    EXPECT_EQ("RRRRF", processSequenceBackward(&machine,
        { kTrail, kHanBMP, kVS17 }));
    EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());
}

TEST(BackwardGraphemeBoundaryStatemachineTest,
    NotBreakImmediatelyBefore_Supplementary_Supplementary)
{
    BackwardGraphemeBoundaryStateMachine machine;

    // 'a' + U+20000 + U+E0100
    EXPECT_EQ("RRRRF", processSequenceBackward(&machine,
        { 'a', kHanSIP, kVS17 }));
    EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

    // U+1F441 + U+20000 + U+E0100
    EXPECT_EQ("RRRRRF", processSequenceBackward(&machine,
        { kEye, kHanSIP, kVS17 }));
    EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

    // SOT + U+20000 + U+E0100
    EXPECT_EQ("RRRRF", processSequenceBackward(&machine,
        { kHanSIP, kVS17 }));
    EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

    // [Lead] + U+20000 + U+E0100
    EXPECT_EQ("RRRRF", processSequenceBackward(&machine,
        { kLead, kHanSIP, kVS17 }));
    EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [Trail] + U+20000 + U+E0100
    EXPECT_EQ("RRRRRF", processSequenceBackward(&machine,
        { 'a', kTrail, kHanSIP, kVS17 }));
    EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

    // [Trail] + [Trail] + U+20000 + U+E0100
    EXPECT_EQ("RRRRRF", processSequenceBackward(&machine,
        { kTrail, kTrail, kHanSIP, kVS17 }));
    EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

    // SOT + [Trail] + U+20000 + U+E0100
    EXPECT_EQ("RRRRRF", processSequenceBackward(&machine,
        { kTrail, kHanSIP, kVS17 }));
    EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());
}

TEST(BackwardGraphemeBoundaryStatemachineTest, MuchLongerCase)
{
    const UChar32 kMan = WTF::Unicode::manCharacter;
    const UChar32 kZwj = WTF::Unicode::zeroWidthJoinerCharacter;
    const UChar32 kHeart = WTF::Unicode::heavyBlackHeartCharacter;
    const UChar32 kKiss = WTF::Unicode::kissMarkCharacter;

    BackwardGraphemeBoundaryStateMachine machine;

    // U+1F468 U+200D U+2764 U+FE0F U+200D U+1F48B U+200D U+1F468 is a valid ZWJ
    // emoji sequence.
    // 'a' + ZWJ Emoji Sequence
    EXPECT_EQ("RRRRRRRRRRRF", processSequenceBackward(&machine,
        { 'a', kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan}));
    EXPECT_EQ(-11, machine.finalizeAndGetBoundaryOffset());

    // U+1F441 + ZWJ Emoji Sequence
    EXPECT_EQ("RRRRRRRRRRRRF", processSequenceBackward(&machine,
        { kEye, kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan}));
    EXPECT_EQ(-11, machine.finalizeAndGetBoundaryOffset());

    // SOT + ZWJ Emoji Sequence
    EXPECT_EQ("RRRRRRRRRRRF", processSequenceBackward(&machine,
        { kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan}));
    EXPECT_EQ(-11, machine.finalizeAndGetBoundaryOffset());

    // [Lead] + ZWJ Emoji Sequence
    EXPECT_EQ("RRRRRRRRRRRF", processSequenceBackward(&machine,
        { kLead, kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan}));
    EXPECT_EQ(-11, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [Trail] + ZWJ Emoji Sequence
    EXPECT_EQ("RRRRRRRRRRRRF", processSequenceBackward(&machine,
        { 'a', kTrail, kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan}));
    EXPECT_EQ(-11, machine.finalizeAndGetBoundaryOffset());

    // [Trail] + [Trail] + ZWJ Emoji Sequence
    EXPECT_EQ("RRRRRRRRRRRRF", processSequenceBackward(&machine,
        { kTrail, kTrail, kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan}));
    EXPECT_EQ(-11, machine.finalizeAndGetBoundaryOffset());

    // SOT + [Trail] + ZWJ Emoji Sequence
    EXPECT_EQ("RRRRRRRRRRRRF", processSequenceBackward(&machine,
        { kTrail, kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan}));
    EXPECT_EQ(-11, machine.finalizeAndGetBoundaryOffset());
}

TEST(BackwardGraphemeBoundaryStatemachineTest, Flags_singleFlag)
{
    BackwardGraphemeBoundaryStateMachine machine;

    // 'a' + [U] + [S]
    EXPECT_EQ("RRRRF", processSequenceBackward(&machine,
        { 'a', kRisU, kRisS }));
    EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

    // U+1F441 + [U] + [S]
    EXPECT_EQ("RRRRRF", processSequenceBackward(&machine,
        { kEye, kRisU, kRisS }));
    EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

    // SOT + [U] + [S]
    EXPECT_EQ("RRRRF", processSequenceBackward(&machine,
        { kRisU, kRisS }));
    EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

    // [Lead] + [U] + [S]
    EXPECT_EQ("RRRRF", processSequenceBackward(&machine,
        { kLead, kRisU, kRisS }));
    EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [Trail] + [U] + [S]
    EXPECT_EQ("RRRRRF", processSequenceBackward(&machine,
        { 'a', kTrail, kRisU, kRisS }));
    EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

    // [Trail] + [Trail] + [U] + [S]
    EXPECT_EQ("RRRRRF", processSequenceBackward(&machine,
        { kTrail, kTrail, kRisU, kRisS }));
    EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

    // SOT + [Trail] + [U] + [S]
    EXPECT_EQ("RRRRRF", processSequenceBackward(&machine,
        { kTrail, kRisU, kRisS }));
    EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());
}

TEST(BackwardGraphemeBoundaryStatemachineTest, Flags_twoFlags)
{
    BackwardGraphemeBoundaryStateMachine machine;

    // 'a' + [U] + [S] + [U] + [S]
    EXPECT_EQ("RRRRRRRRF", processSequenceBackward(&machine,
        { 'a', kRisU, kRisS, kRisU, kRisS }));
    EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

    // U+1F441 + [U] + [S] + [U] + [S]
    EXPECT_EQ("RRRRRRRRRF", processSequenceBackward(&machine,
        { kEye, kRisU, kRisS, kRisU, kRisS }));
    EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

    // SOT + [U] + [S] + [U] + [S]
    EXPECT_EQ("RRRRRRRRF", processSequenceBackward(&machine,
        { kRisU, kRisS, kRisU, kRisS }));
    EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

    // [Lead] + [U] + [S] + [U] + [S]
    EXPECT_EQ("RRRRRRRRF", processSequenceBackward(&machine,
        { kLead, kRisU, kRisS, kRisU, kRisS }));
    EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [Trail] + [U] + [S] + [U] + [S]
    EXPECT_EQ("RRRRRRRRRF", processSequenceBackward(&machine,
        { 'a', kTrail, kRisU, kRisS, kRisU, kRisS }));
    EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

    // [Trail] + [Trail] + [U] + [S] + [U] + [S]
    EXPECT_EQ("RRRRRRRRRF", processSequenceBackward(&machine,
        { kTrail, kTrail, kRisU, kRisS, kRisU, kRisS }));
    EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

    // SOT + [Trail] + [U] + [S] + [U] + [S]
    EXPECT_EQ("RRRRRRRRRF", processSequenceBackward(&machine,
        { kTrail, kRisU, kRisS, kRisU, kRisS }));
    EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());
}

TEST(BackwardGraphemeBoundaryStatemachineTest, Flags_oddNumberedRIS)
{
    BackwardGraphemeBoundaryStateMachine machine;

    // 'a' + [U] + [S] + [U]
    EXPECT_EQ("RRRRRRF", processSequenceBackward(&machine,
        { 'a', kRisU, kRisS, kRisU }));
    EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

    // U+1F441 + [U] + [S] + [U]
    EXPECT_EQ("RRRRRRRF", processSequenceBackward(&machine,
        { kEye, kRisU, kRisS, kRisU }));
    EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

    // SOT + [U] + [S] + [U]
    EXPECT_EQ("RRRRRRF", processSequenceBackward(&machine,
        { kRisU, kRisS, kRisU }));
    EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

    // [Lead] + [U] + [S] + [U]
    EXPECT_EQ("RRRRRRF", processSequenceBackward(&machine,
        { kLead, kRisU, kRisS, kRisU }));
    EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [Trail] + [U] + [S] + [U]
    EXPECT_EQ("RRRRRRRF", processSequenceBackward(&machine,
        { 'a', kTrail, kRisU, kRisS, kRisU }));
    EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

    // [Trail] + [Trail] + [U] + [S] + [U]
    EXPECT_EQ("RRRRRRRF", processSequenceBackward(&machine,
        { kTrail, kTrail, kRisU, kRisS, kRisU }));
    EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

    // SOT + [Trail] + [U] + [S] + [U]
    EXPECT_EQ("RRRRRRRF", processSequenceBackward(&machine,
        { kTrail, kRisU, kRisS, kRisU }));
    EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
}
} // namespace blink
