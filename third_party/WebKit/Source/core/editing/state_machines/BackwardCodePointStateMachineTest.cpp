// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/state_machines/BackwardCodePointStateMachine.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {
const TextSegmentationMachineState kInvalid =
    TextSegmentationMachineState::Invalid;
const TextSegmentationMachineState kNeedMoreCodeUnit =
    TextSegmentationMachineState::NeedMoreCodeUnit;
const TextSegmentationMachineState kFinished =
    TextSegmentationMachineState::Finished;
}  // namespace

TEST(BackwardCodePointStateMachineTest, DoNothingCase) {
  BackwardCodePointStateMachine machine;
  EXPECT_EQ(0, machine.getBoundaryOffset());
}

TEST(BackwardCodePointStateMachineTest, SingleCharacter) {
  BackwardCodePointStateMachine machine;
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit('a'));
  EXPECT_EQ(-1, machine.getBoundaryOffset());

  machine.reset();
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit('-'));
  EXPECT_EQ(-1, machine.getBoundaryOffset());

  machine.reset();
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit('\t'));
  EXPECT_EQ(-1, machine.getBoundaryOffset());

  machine.reset();
  // U+3042 HIRAGANA LETTER A.
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(0x3042));
  EXPECT_EQ(-1, machine.getBoundaryOffset());
}

TEST(BackwardCodePointStateMachineTest, SurrogatePair) {
  BackwardCodePointStateMachine machine;

  // U+20BB7 is \uD83D\uDDFA in UTF-16.
  const UChar leadSurrogate = 0xD842;
  const UChar trailSurrogate = 0xDFB7;

  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(trailSurrogate));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(leadSurrogate));
  EXPECT_EQ(-2, machine.getBoundaryOffset());

  // Edge cases
  // Unpaired trailing surrogate. Nothing to delete.
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(trailSurrogate));
  EXPECT_EQ(kInvalid, machine.feedPrecedingCodeUnit('a'));
  EXPECT_EQ(0, machine.getBoundaryOffset());

  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(trailSurrogate));
  EXPECT_EQ(kInvalid, machine.feedPrecedingCodeUnit(trailSurrogate));
  EXPECT_EQ(0, machine.getBoundaryOffset());

  // Unpaired leading surrogate. Nothing to delete.
  machine.reset();
  EXPECT_EQ(kInvalid, machine.feedPrecedingCodeUnit(leadSurrogate));
  EXPECT_EQ(0, machine.getBoundaryOffset());
}

}  // namespace blink
