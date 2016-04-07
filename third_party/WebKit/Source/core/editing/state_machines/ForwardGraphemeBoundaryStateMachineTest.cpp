// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/state_machines/ForwardGraphemeBoundaryStateMachine.h"

#include "core/editing/state_machines/StateMachineTestUtil.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/CharacterNames.h"

namespace blink {

// Notations:
// | indicates inidicates initial offset position.
// SOT indicates start of text.
// EOT indicates end of text.
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
const UChar32 kRisU = 0x1F1FA;
// U+1F1F8 is REGIONAL INDICATOR SYMBOL LETTER S
const UChar32 kRisS = 0x1F1F8;
} // namespace


TEST(ForwardGraphemeBoundaryStatemachineTest, DoNothingCase)
{
    ForwardGraphemeBoundaryStateMachine machine;

    EXPECT_EQ(0, machine.finalizeAndGetBoundaryOffset());
    EXPECT_EQ(0, machine.finalizeAndGetBoundaryOffset());
}

TEST(ForwardGraphemeBoundaryStatemachineTest, PrecedingText)
{
    const std::vector<UChar32> kEmpty;
    ForwardGraphemeBoundaryStateMachine machine;
    // Preceding text should not affect the result except for flags.
    // SOT + | + 'a' + 'a'
    EXPECT_EQ("SRF", processSequenceForward(&machine, kEmpty, { 'a', 'a' }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // SOT + [U] + | + 'a' + 'a'
    EXPECT_EQ("RRSRF", processSequenceForward(&machine,
        { kRisU }, { 'a', 'a' }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // SOT + [U] + [S] + | + 'a' + 'a'
    EXPECT_EQ("RRRRSRF", processSequenceForward(&machine,
        { kRisU, kRisS }, { 'a', 'a' }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());

    // U+0000 + | + 'a' + 'a'
    EXPECT_EQ("SRF", processSequenceForward(&machine, { 0 }, { 'a', 'a' }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // U+0000 + [U] + | + 'a' + 'a'
    EXPECT_EQ("RRSRF", processSequenceForward(&machine,
        { 0, kRisU }, { 'a', 'a' }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // U+0000 + [U] + [S] + | + 'a' + 'a'
    EXPECT_EQ("RRRRSRF", processSequenceForward(&machine,
        { 0, kRisU, kRisS }, { 'a', 'a' }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());

    // 'a' + | + 'a' + 'a'
    EXPECT_EQ("SRF", processSequenceForward(&machine, { 'a' }, { 'a', 'a' }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // 'a' + [U] + | + 'a' + 'a'
    EXPECT_EQ("RRSRF", processSequenceForward(&machine,
        { 'a', kRisU }, { 'a', 'a' }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // 'a' + [U] + [S] + | + 'a' + 'a'
    EXPECT_EQ("RRRRSRF", processSequenceForward(&machine,
        { 'a', kRisU, kRisS }, { 'a', 'a' }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());

    // U+1F441 + | + 'a' + 'a'
    EXPECT_EQ("RSRF", processSequenceForward(&machine,
        { kEye }, { 'a', 'a' }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // U+1F441 + [U] + | + 'a' + 'a'
    EXPECT_EQ("RRRSRF", processSequenceForward(&machine,
        { kEye, kRisU }, { 'a', 'a' }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // U+1F441 + [U] + [S] + | + 'a' + 'a'
    EXPECT_EQ("RRRRRSRF", processSequenceForward(&machine,
        { kEye, kRisU, kRisS }, { 'a', 'a' }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());

    // Broken surrogates in preceding text.

    // [Lead] + | + 'a' + 'a'
    EXPECT_EQ("SRF", processSequenceForward(&machine,
        { kLead }, { 'a', 'a' }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // [Lead] + [U] + | + 'a' + 'a'
    EXPECT_EQ("RRSRF", processSequenceForward(&machine,
        { kLead, kRisU }, { 'a', 'a' }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // [Lead] + [U] + [S] + | + 'a' + 'a'
    EXPECT_EQ("RRRRSRF", processSequenceForward(&machine,
        { kLead, kRisU, kRisS }, { 'a', 'a' }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [Trail] + | + 'a' + 'a'
    EXPECT_EQ("RSRF", processSequenceForward(&machine,
        { 'a', kTrail }, { 'a', 'a' }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // 'a' + [Trail] + [U] + | + 'a' + 'a'
    EXPECT_EQ("RRRSRF", processSequenceForward(&machine,
        { 'a', kTrail, kRisU }, { 'a', 'a' }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // 'a' + [Trail] + [U] + [S] + | + 'a' + 'a'
    EXPECT_EQ("RRRRRSRF", processSequenceForward(&machine,
        { 'a', kTrail, kRisU, kRisS }, { 'a', 'a' }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());

    // [Trail] + [Trail] + | + 'a' + 'a'
    EXPECT_EQ("RSRF", processSequenceForward(&machine,
        { kTrail, kTrail }, { 'a', 'a' }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // [Trail] + [Trail] + [U] + | + 'a' + 'a'
    EXPECT_EQ("RRRSRF", processSequenceForward(&machine,
        { kTrail, kTrail, kRisU }, { 'a', 'a' }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // [Trail] + [Trail] + [U] + [S] + | + 'a' + 'a'
    EXPECT_EQ("RRRRRSRF", processSequenceForward(&machine,
        { kTrail, kTrail, kRisU, kRisS }, { 'a', 'a' }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());

    // SOT + [Trail] + | + 'a' + 'a'
    EXPECT_EQ("RSRF", processSequenceForward(&machine,
        { kTrail }, { 'a', 'a' }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // SOT + [Trail] + [U] + | + 'a' + 'a'
    EXPECT_EQ("RRRSRF", processSequenceForward(&machine,
        { kTrail, kRisU }, { 'a', 'a' }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // SOT + [Trail] + [U] + [S] + | + 'a' + 'a'
    EXPECT_EQ("RRRRRSRF", processSequenceForward(&machine,
        { kTrail, kRisU, kRisS }, { 'a', 'a' }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
}

TEST(ForwardGraphemeBoundaryStatemachineTest, BrokenSurrogatePair)
{
    const std::vector<UChar32> kEmpty;
    ForwardGraphemeBoundaryStateMachine machine;
    // SOT + | + [Trail]
    EXPECT_EQ("SF", processSequenceForward(&machine, kEmpty, { kTrail }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // SOT + | + [Lead] + 'a'
    EXPECT_EQ("SRF", processSequenceForward(&machine, kEmpty, { kLead, 'a' }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // SOT + | + [Lead] + [Lead]
    EXPECT_EQ("SRF", processSequenceForward(&machine, kEmpty,
        { kLead, kLead }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // SOT + | + [Lead] + EOT
    EXPECT_EQ("SR", processSequenceForward(&machine, kEmpty, { kLead }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
}

TEST(ForwardGraphemeBoundaryStatemachineTest, BreakImmediately_BMP)
{
    const std::vector<UChar32> kEmpty;
    ForwardGraphemeBoundaryStateMachine machine;

    // SOT + | + U+0000 + U+0000
    EXPECT_EQ("SRF", processSequenceForward(&machine, kEmpty, { 0, 0 }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + 'a' + 'a'
    EXPECT_EQ("SRF", processSequenceForward(&machine, kEmpty, { 'a', 'a' }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + 'a' + U+1F441
    EXPECT_EQ("SRRF", processSequenceForward(&machine, kEmpty, { 'a', kEye }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + 'a' + EOT
    EXPECT_EQ("SR", processSequenceForward(&machine, kEmpty, { 'a' }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + 'a' + [Trail]
    EXPECT_EQ("SRF", processSequenceForward(&machine, kEmpty, { 'a', kTrail }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + 'a' + [Lead] + 'a'
    EXPECT_EQ("SRRF", processSequenceForward(&machine, kEmpty,
        { 'a', kLead, 'a' }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + 'a' + [Lead] + [Lead]
    EXPECT_EQ("SRRF", processSequenceForward(&machine, kEmpty,
        { 'a', kLead, kLead }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + 'a' + [Lead] + EOT
    EXPECT_EQ("SRR", processSequenceForward(&machine, kEmpty, { 'a', kLead }));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
}

TEST(ForwardGraphemeBoundaryStatemachineTest, BreakImmediately_Supplementary)
{
    const std::vector<UChar32> kEmpty;
    ForwardGraphemeBoundaryStateMachine machine;

    // SOT + | + U+1F441 + 'a'
    EXPECT_EQ("SRRF", processSequenceForward(&machine, kEmpty, { kEye, 'a' }));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+1F441 + U+1F441
    EXPECT_EQ("SRRRF", processSequenceForward(&machine, kEmpty,
        { kEye, kEye }));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+1F441 + EOT
    EXPECT_EQ("SRR", processSequenceForward(&machine, kEmpty, { kEye }));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+1F441 + [Trail]
    EXPECT_EQ("SRRF", processSequenceForward(&machine, kEmpty,
        { kEye, kTrail }));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+1F441 + [Lead] + 'a'
    EXPECT_EQ("SRRRF", processSequenceForward(&machine, kEmpty,
        { kEye, kLead, 'a' }));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+1F441 + [Lead] + [Lead]
    EXPECT_EQ("SRRRF", processSequenceForward(&machine, kEmpty,
        { kEye, kLead, kLead }));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+1F441 + [Lead] + EOT
    EXPECT_EQ("SRRR", processSequenceForward(&machine, kEmpty,
        { kEye, kLead }));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());
}

TEST(ForwardGraphemeBoundaryStatemachineTest, NotBreakImmediatelyAfter_BMP_BMP)
{
    const std::vector<UChar32> kEmpty;
    ForwardGraphemeBoundaryStateMachine machine;

    // SOT + | + U+231A + U+FE0F + 'a'
    EXPECT_EQ("SRRF", processSequenceForward(&machine, kEmpty,
        { kWatch, kVS16, 'a' }));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+231A + U+FE0F + U+1F441
    EXPECT_EQ("SRRRF", processSequenceForward(&machine, kEmpty,
        { kWatch, kVS16, kEye }));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+231A + U+FE0F + EOT
    EXPECT_EQ("SRR", processSequenceForward(&machine, kEmpty,
        { kWatch, kVS16 }));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+231A + U+FE0F + [Trail]
    EXPECT_EQ("SRRF", processSequenceForward(&machine, kEmpty,
        { kWatch, kVS16, kTrail }));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+231A + U+FE0F + [Lead] + 'a'
    EXPECT_EQ("SRRRF", processSequenceForward(&machine, kEmpty,
        { kWatch, kVS16, kLead, 'a' }));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+231A + U+FE0F + [Lead] + [Lead]
    EXPECT_EQ("SRRRF", processSequenceForward(&machine, kEmpty,
        { kWatch, kVS16, kLead, kLead }));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+231A + U+FE0F + [Lead] + EOT
    EXPECT_EQ("SRRR", processSequenceForward(&machine, kEmpty,
        { kWatch, kVS16, kLead }));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());
}

TEST(ForwardGraphemeBoundaryStatemachineTest,
    NotBreakImmediatelyAfter_Supplementary_BMP)
{
    const std::vector<UChar32> kEmpty;
    ForwardGraphemeBoundaryStateMachine machine;

    // SOT + | + U+1F441 + U+FE0F + 'a'
    EXPECT_EQ("SRRRF", processSequenceForward(&machine, kEmpty,
        { kEye, kVS16, 'a' }));
    EXPECT_EQ(3, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+1F441 + U+FE0F + U+1F441
    EXPECT_EQ("SRRRRF", processSequenceForward(&machine, kEmpty,
        { kEye, kVS16, kEye }));
    EXPECT_EQ(3, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+1F441 + U+FE0F + EOT
    EXPECT_EQ("SRRR", processSequenceForward(&machine, kEmpty,
        { kEye, kVS16 }));
    EXPECT_EQ(3, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+1F441 + U+FE0F + [Trail]
    EXPECT_EQ("SRRRF", processSequenceForward(&machine, kEmpty,
        { kEye, kVS16, kTrail }));
    EXPECT_EQ(3, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+1F441 + U+FE0F + [Lead] + 'a'
    EXPECT_EQ("SRRRRF", processSequenceForward(&machine, kEmpty,
        { kEye, kVS16, kLead, 'a' }));
    EXPECT_EQ(3, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+1F441 + U+FE0F + [Lead] + [Lead]
    EXPECT_EQ("SRRRRF", processSequenceForward(&machine, kEmpty,
        { kEye, kVS16, kLead, kLead }));
    EXPECT_EQ(3, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+1F441 + U+FE0F + [Lead] + EOT
    EXPECT_EQ("SRRRR", processSequenceForward(&machine, kEmpty,
        { kEye, kVS16, kLead }));
    EXPECT_EQ(3, machine.finalizeAndGetBoundaryOffset());
}

TEST(ForwardGraphemeBoundaryStatemachineTest,
    NotBreakImmediatelyAfter_BMP_Supplementary)
{
    const std::vector<UChar32> kEmpty;
    ForwardGraphemeBoundaryStateMachine machine;

    // SOT + | + U+845B + U+E0100 + 'a'
    EXPECT_EQ("SRRRF", processSequenceForward(&machine, kEmpty,
        { kHanBMP, kVS17, 'a' }));
    EXPECT_EQ(3, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+845B + U+E0100 + U+1F441
    EXPECT_EQ("SRRRRF", processSequenceForward(&machine, kEmpty,
        { kHanBMP, kVS17, kEye }));
    EXPECT_EQ(3, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+845B + U+E0100 + EOT
    EXPECT_EQ("SRRR", processSequenceForward(&machine, kEmpty,
        { kHanBMP, kVS17 }));
    EXPECT_EQ(3, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+845B + U+E0100 + [Trail]
    EXPECT_EQ("SRRRF", processSequenceForward(&machine, kEmpty,
        { kHanBMP, kVS17, kTrail }));
    EXPECT_EQ(3, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+845B + U+E0100 + [Lead] + 'a'
    EXPECT_EQ("SRRRRF", processSequenceForward(&machine, kEmpty,
        { kHanBMP, kVS17, kLead, 'a' }));
    EXPECT_EQ(3, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+845B + U+E0100 + [Lead] + [Lead]
    EXPECT_EQ("SRRRRF", processSequenceForward(&machine, kEmpty,
        { kHanBMP, kVS17, kLead, kLead }));
    EXPECT_EQ(3, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+845B + U+E0100 + [Lead] + EOT
    EXPECT_EQ("SRRRR", processSequenceForward(&machine, kEmpty,
        { kHanBMP, kVS17, kLead }));
    EXPECT_EQ(3, machine.finalizeAndGetBoundaryOffset());
}

TEST(ForwardGraphemeBoundaryStatemachineTest,
    NotBreakImmediatelyAfter_Supplementary_Supplementary)
{
    const std::vector<UChar32> kEmpty;
    ForwardGraphemeBoundaryStateMachine machine;

    // SOT + | + U+20000 + U+E0100 + 'a'
    EXPECT_EQ("SRRRRF", processSequenceForward(&machine, kEmpty,
        { kHanSIP, kVS17, 'a' }));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+20000 + U+E0100 + U+1F441
    EXPECT_EQ("SRRRRRF", processSequenceForward(&machine, kEmpty,
        { kHanSIP, kVS17, kEye }));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+20000 + U+E0100 + EOT
    EXPECT_EQ("SRRRR", processSequenceForward(&machine, kEmpty,
        { kHanSIP, kVS17 }));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+20000 + U+E0100 + [Trail]
    EXPECT_EQ("SRRRRF", processSequenceForward(&machine, kEmpty,
        { kHanSIP, kVS17, kTrail }));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+20000 + U+E0100 + [Lead] + 'a'
    EXPECT_EQ("SRRRRRF", processSequenceForward(&machine, kEmpty,
        { kHanSIP, kVS17, kLead, 'a' }));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+20000 + U+E0100 + [Lead] + [Lead]
    EXPECT_EQ("SRRRRRF", processSequenceForward(&machine, kEmpty,
        { kHanSIP, kVS17, kLead, kLead }));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+20000 + U+E0100 + [Lead] + EOT
    EXPECT_EQ("SRRRRR", processSequenceForward(&machine, kEmpty,
        { kHanSIP, kVS17, kLead }));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());
}

TEST(ForwardGraphemeBoundaryStatemachineTest, MuchLongerCase)
{
    const std::vector<UChar32> kEmpty;
    ForwardGraphemeBoundaryStateMachine machine;

    const UChar32 kMan = WTF::Unicode::manCharacter;
    const UChar32 kZwj = WTF::Unicode::zeroWidthJoinerCharacter;
    const UChar32 kHeart = WTF::Unicode::heavyBlackHeartCharacter;
    const UChar32 kKiss = WTF::Unicode::kissMarkCharacter;

    // U+1F468 U+200D U+2764 U+FE0F U+200D U+1F48B U+200D U+1F468 is a valid ZWJ
    // emoji sequence.
    // SOT + | + ZWJ Emoji Sequence + 'a'
    EXPECT_EQ("SRRRRRRRRRRRF", processSequenceForward(&machine, kEmpty,
        { kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan, 'a' }));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + ZWJ Emoji Sequence + U+1F441
    EXPECT_EQ("SRRRRRRRRRRRRF", processSequenceForward(&machine, kEmpty,
        { kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan, kEye }));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + ZWJ Emoji Sequence + EOT
    EXPECT_EQ("SRRRRRRRRRRR", processSequenceForward(&machine, kEmpty,
        { kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan }));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + ZWJ Emoji Sequence + [Trail]
    EXPECT_EQ("SRRRRRRRRRRRF", processSequenceForward(&machine, kEmpty,
        { kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan, kTrail }));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + ZWJ Emoji Sequence + [Lead] + 'a'
    EXPECT_EQ("SRRRRRRRRRRRRF", processSequenceForward(&machine, kEmpty,
        { kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan, kLead, 'a' }));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + ZWJ Emoji Sequence + [Lead] + [Lead]
    EXPECT_EQ("SRRRRRRRRRRRRF", processSequenceForward(&machine, kEmpty,
        { kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan, kLead, kLead }));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + ZWJ Emoji Sequence + [Lead] + EOT
    EXPECT_EQ("SRRRRRRRRRRRR", processSequenceForward(&machine, kEmpty,
        { kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan, kLead }));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

    // Preceding text should not affect the result except for flags.
    // 'a' + | + ZWJ Emoji Sequence + [Lead] + EOT
    EXPECT_EQ("SRRRRRRRRRRRF", processSequenceForward(&machine,
        { 'a' },
        { kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan, 'a' }));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

    // U+1F441 + | + ZWJ Emoji Sequence + [Lead] + EOT
    EXPECT_EQ("RSRRRRRRRRRRRF", processSequenceForward(&machine,
        { kEye },
        { kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan, 'a' }));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

    // [Lead] + | + ZWJ Emoji Sequence + [Lead] + EOT
    EXPECT_EQ("SRRRRRRRRRRRF", processSequenceForward(&machine,
        { kLead },
        { kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan, 'a' }));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [Trail] + | + ZWJ Emoji Sequence + [Lead] + EOT
    EXPECT_EQ("RSRRRRRRRRRRRF", processSequenceForward(&machine,
        { 'a', kTrail },
        { kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan, 'a' }));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

    // [Trail] + [Trail] + | + ZWJ Emoji Sequence + [Lead] + EOT
    EXPECT_EQ("RSRRRRRRRRRRRF", processSequenceForward(&machine,
        { kTrail, kTrail },
        { kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan, 'a' }));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

    // SOT + [Trail] + | + ZWJ Emoji Sequence + [Lead] + EOT
    EXPECT_EQ("RSRRRRRRRRRRRF", processSequenceForward(&machine,
        { kTrail },
        { kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan, 'a' }));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [U] + | + ZWJ Emoji Sequence + [Lead] + EOT
    EXPECT_EQ("RRSRRRRRRRRRRRF", processSequenceForward(&machine,
        { 'a', kRisU },
        { kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan, 'a' }));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [U] + [S] + | + ZWJ Emoji Sequence + [Lead] + EOT
    EXPECT_EQ("RRRRSRRRRRRRRRRRF", processSequenceForward(&machine,
        { 'a', kRisU, kRisS },
        { kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan, 'a' }));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

}

TEST(ForwardGraphemeBoundaryStatemachineTest, singleFlags)
{
    const std::vector<UChar32> kEmpty;
    ForwardGraphemeBoundaryStateMachine machine;

    // SOT + | + [U] + [S]
    EXPECT_EQ("SRRRF", processSequenceForward(&machine,
        kEmpty, { kRisU, kRisS }));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // 'a' + | + [U] + [S]
    EXPECT_EQ("SRRRF", processSequenceForward(&machine,
        { 'a' }, { kRisU, kRisS }));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // U+1F441 + | + [U] + [S]
    EXPECT_EQ("RSRRRF", processSequenceForward(&machine,
        { kEye }, { kRisU, kRisS }));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // [Lead] + | + [U] + [S]
    EXPECT_EQ("SRRRF", processSequenceForward(&machine,
        { kLead }, { kRisU, kRisS }));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [Trail] + | + [U] + [S]
    EXPECT_EQ("RSRRRF", processSequenceForward(&machine,
        { 'a', kTrail }, { kRisU, kRisS }));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // [Trail] + [Trail] + | + [U] + [S]
    EXPECT_EQ("RSRRRF", processSequenceForward(&machine,
        { kTrail, kTrail }, { kRisU, kRisS }));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // SOT + [Trail] + | + [U] + [S]
    EXPECT_EQ("RSRRRF", processSequenceForward(&machine,
        { kTrail }, { kRisU, kRisS }));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());
}

TEST(ForwardGraphemeBoundaryStatemachineTest, twoFlags)
{
    ForwardGraphemeBoundaryStateMachine machine;

    // SOT + [U] + [S] + | + [U] + [S]
    EXPECT_EQ("RRRRSRRRF", processSequenceForward(&machine,
        { kRisU, kRisS }, { kRisU, kRisS }));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [U] + [S] + | + [U] + [S]
    EXPECT_EQ("RRRRSRRRF", processSequenceForward(&machine,
        { 'a', kRisU, kRisS }, { kRisU, kRisS }));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // U+1F441 + [U] + [S] + | + [U] + [S]
    EXPECT_EQ("RRRRRSRRRF", processSequenceForward(&machine,
        { kEye, kRisU, kRisS }, { kRisU, kRisS }));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // [Lead] + [U] + [S] + | + [U] + [S]
    EXPECT_EQ("RRRRSRRRF", processSequenceForward(&machine,
        { kLead, kRisU, kRisS }, { kRisU, kRisS }));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [Trail] + [U] + [S] + | + [U] + [S]
    EXPECT_EQ("RRRRRSRRRF", processSequenceForward(&machine,
        { 'a', kTrail, kRisU, kRisS }, { kRisU, kRisS }));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // [Trail] + [Trail] + [U] + [S] + | + [U] + [S]
    EXPECT_EQ("RRRRRSRRRF", processSequenceForward(&machine,
        { kTrail, kTrail, kRisU, kRisS }, { kRisU, kRisS }));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // SOT + [Trail] + [U] + [S] + | + [U] + [S]
    EXPECT_EQ("RRRRRSRRRF", processSequenceForward(&machine,
        { kTrail, kRisU, kRisS }, { kRisU, kRisS }));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());
}

TEST(ForwardGraphemeBoundaryStatemachineTest, oddNumberedFlags)
{
    ForwardGraphemeBoundaryStateMachine machine;

    // SOT + [U] + | + [S] + [S]
    EXPECT_EQ("RRSRRRF", processSequenceForward(&machine,
        { kRisU }, { kRisS, kRisU }));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [U] + | + [S] + [S]
    EXPECT_EQ("RRSRRRF", processSequenceForward(&machine,
        { 'a', kRisU }, { kRisS, kRisU }));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // U+1F441 + [U] + | + [S] + [S]
    EXPECT_EQ("RRRSRRRF", processSequenceForward(&machine,
        { kEye, kRisU }, { kRisS, kRisU }));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // [Lead] + [U] + | + [S] + [S]
    EXPECT_EQ("RRSRRRF", processSequenceForward(&machine,
        { kLead, kRisU }, { kRisS, kRisU }));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [Trail] + [U] + | + [S] + [S]
    EXPECT_EQ("RRRSRRRF", processSequenceForward(&machine,
        { 'a', kTrail, kRisU }, { kRisS, kRisU }));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // [Trail] + [Trail] + [U] + | + [S] + [S]
    EXPECT_EQ("RRRSRRRF", processSequenceForward(&machine,
        { kTrail, kTrail, kRisU }, { kRisS, kRisU }));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // SOT + [Trail] + [U] + | + [S] + [S]
    EXPECT_EQ("RRRSRRRF", processSequenceForward(&machine,
        { kTrail, kRisU }, { kRisS, kRisU }));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());
}

} // namespace blink
