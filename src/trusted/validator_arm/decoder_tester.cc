/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Tests the decoder.
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error This file is not meant for use in the TCB
#endif

#include "native_client/src/trusted/validator_arm/decoder_tester.h"

#include <string>
#include "gtest/gtest.h"

using nacl_arm_dec::kArm32InstSize;
using nacl_arm_dec::kThumbWordSize;
using nacl_arm_dec::Instruction;
using nacl_arm_dec::MAY_BE_SAFE;

namespace nacl_arm_test {

DecoderTester::DecoderTester()
{}

void DecoderTester::ApplySanityChecks(Instruction inst,
                                      const NamedClassDecoder& decoder) {
  UNREFERENCED_PARAMETER(inst);
  EXPECT_EQ(&decoder, &ExpectedDecoder())
      << "Expected " << ExpectedDecoder().name() << " but found "
      << decoder.name() << " for " << InstContents();
}

void DecoderTester::TestAtIndex(int index) {
  while (true) {
    // First see if we have completed generating test.
    if (index < 0) {
      ProcessMatch();
      return;
    }

    // If reached, more pattern to process.
    char selector = Pattern(index);

    if (selector == '0') {
      SetBit(index, false);
      --index;
    }

    if (selector == '1') {
      SetBit(index, true);
      --index;
    }

    if (islower(selector)) {
      // Find the length of the run of the same character (selector) in
      // the pattern. Then, test all combinations of zero and one for the
      // run length.
      int length = 1;
      while (Pattern(index - length) == selector) ++length;
      TestAtIndexExpandAll(index, length);
      return;
    }

    if (isupper(selector)) {
      // Find length of selector, then expand.
      int length = 1;
      while (Pattern(index - length) == selector) ++length;
      if (length <= 4) {
        TestAtIndexExpandAll(index, length);
      } else {
        TestAtIndexExpandFill4(index, length, true);
        TestAtIndexExpandFill4(index, length, false);
      }
      return;
    }
  }
}

void DecoderTester::TestAtIndexExpandAll(int index, int length) {
  if (length == 32) {
    // To be safe about overflow conditions, we use the recursive solution here.
    // Try setting index bit to false and continue testing.
    SetBit(index, false);
    TestAtIndexExpandAll(index-1, length-1);

    // Try setting index bit to true and continue testing.
    SetBit(index, true);
    TestAtIndexExpandAll(index-1, length-1);
  } else if (length == 0) {
    TestAtIndex(index);
  } else {
    // Use an (hopefully faster) iterative solution.
    uint32_t i;
    for (i = 0; i <= (((uint32_t) 1) << length) - 1; ++i) {
      SetBitRange(index - (length - 1), length, i);
      TestAtIndex(index - length);
    }
  }
}

void DecoderTester::FillBitRange(int index, int length, bool value) {
  if (length == 0)
    return;
  uint32_t val = (value ? 1 : 0);
  uint32_t bits = (val << length) - val;
  SetBitRange(index - (length - 1), length, bits);
}

void DecoderTester::TestAtIndexExpandFill(int index, int length, bool value) {
  FillBitRange(index, length, value);
  TestAtIndex(index - length);
}

void DecoderTester::TestAtIndexExpandFillAll(
    int index, int stride, int length, bool value) {
  ASSERT_LT(stride, 32) << "TestAtIndexExpandFillAll - illegal stride";
  if (stride == 0) {
    TestAtIndexExpandFill(index, length, value);
    return;
  } else {
    uint32_t i;
    for (i = 0; i <= (((uint32_t) 1) << stride) - 1; ++i) {
      SetBitRange(index - (stride - 1), stride, i);
      TestAtIndexExpandFill(index - stride, length - stride, value);
    }
  }
}

void DecoderTester::TestAtIndexExpandFill4(int index, int length, bool value) {
  int stride = 4;
  ASSERT_GT(length, stride);
  int window = length - stride;
  for (int i = 0; i <= window; ++i) {
    FillBitRange(index, i, value);
    TestAtIndexExpandFillAll(index - i, stride, length - i, value);
  }
}

Arm32DecoderTester::Arm32DecoderTester(
    const NamedClassDecoder& expected_decoder)
    : DecoderTester(),
      expected_decoder_(expected_decoder),
      pattern_(""),
      state_(),
      inst_((uint32_t) 0xFFFFFFFF) {
}

Arm32DecoderTester:: ~Arm32DecoderTester() {
}

void Arm32DecoderTester::Test(const char* pattern) {
  pattern_ = pattern;
  ASSERT_EQ(static_cast<int>(kArm32InstSize),
            static_cast<int>(strlen(pattern_)))
      << "Arm32 pattern length incorrect: " << pattern_;
  TestAtIndex(kArm32InstSize-1);
}

const NamedClassDecoder& Arm32DecoderTester::ExpectedDecoder() const {
  return expected_decoder_;
}

static inline char BoolToChar(bool value) {
  return '0' + (uint8_t) value;
}

const char* Arm32DecoderTester::InstContents() const {
  static char buffer[kArm32InstSize + 1];
  for (int i = 1; i <= kArm32InstSize; ++i) {
    buffer[kArm32InstSize - i] = BoolToChar(inst_.bit(i - 1));
  }
  buffer[kArm32InstSize] = '\0';
  return buffer;
}

char Arm32DecoderTester::Pattern(int index) const {
  return pattern_[kArm32InstSize - (index + 1)];
}

void Arm32DecoderTester::InjectInstruction(nacl_arm_dec::Instruction inst) {
  inst_ = inst;
}

void Arm32DecoderTester::ProcessMatch() {
  // Completed pattern, decode and test resulting state.
  const NamedClassDecoder& decoder = state_.decode_named(inst_);
  if (MAY_BE_SAFE == decoder.safety(inst_)) ApplySanityChecks(inst_, decoder);
  return;
}

void Arm32DecoderTester::SetBit(int index, bool value) {
  ASSERT_LT(index, kArm32InstSize)
      << "Arm32DecoderTester::SetBit(" << index << ", " << value << ")";
  ASSERT_GE(index , 0)
      << "Arm32DecoderTester::SetBit(" << index << ", " << value << ")";
  inst_.set_bit(index, value);
}

void Arm32DecoderTester::SetBitRange(int index, int length, uint32_t value) {
  ASSERT_LE(index + length, kArm32InstSize)
      << "Arm32DecoderTester::SetBitRange(" << index << ", " << length
      << ", " << value << ")";
  ASSERT_GE(index, 0)
      << "Arm32DecoderTester::SetBitRange(" << index << ", " << length
      << ", " << value << ")";
  inst_.set_bits(index + (length - 1), index, value);
}


ThumbWord1DecoderTester::ThumbWord1DecoderTester(
    ThumbDecoderTester* thumb_tester)
    : thumb_tester_(thumb_tester), pattern_("")
{}

ThumbWord1DecoderTester::~ThumbWord1DecoderTester() {}

void ThumbWord1DecoderTester::SetPattern(const char* pattern) {
  pattern_ = pattern;
  ASSERT_LE(static_cast<int>(kThumbWordSize),
            static_cast<int>(strlen(pattern))) <<
      "Thumb word 1 pattern length incorrect: " << pattern;
}

void ThumbWord1DecoderTester::Test() {
  TestAtIndex(kThumbWordSize-1);
}

const NamedClassDecoder& ThumbWord1DecoderTester::ExpectedDecoder() const {
  return thumb_tester_->ExpectedDecoder();
}

char ThumbWord1DecoderTester::Pattern(int index) const {
  return pattern_[kThumbWordSize - (index + 1)];
}

const char* ThumbWord1DecoderTester::InstContents() const {
  return thumb_tester_->InstContents();
}

void ThumbWord1DecoderTester::ProcessMatch() {
  thumb_tester_->word2_tester_.Test();
}

void ThumbWord1DecoderTester::InjectInstruction(
    nacl_arm_dec::Instruction inst) {
  thumb_tester_->InjectInstruction(inst);
}

void ThumbWord1DecoderTester::SetBit(int index, bool value) {
  ASSERT_LT(index, kThumbWordSize) << "ThumbWord1DecoderTester::SetBit("
                                   << index << ", " << ")";
  ASSERT_GE(index, 0) << "ThumbWord1DecoderTester::SetBit("
                      << index << ", " << ")";
  thumb_tester_->inst_.word1_set_bit(index, value);
}

void ThumbWord1DecoderTester::SetBitRange(
    int index, int length, uint32_t value) {
  ASSERT_LE(index + length, kThumbWordSize)
      << "ThumbWord1DecoderTester::SetBitRange(" << index << ", " << length
      << ", " << value << ")";
  ASSERT_GE(index, 0)
      << "ThumbWord1DecoderTester::SetBitRange(" << index << ", " << length
      << ", " << value << ")";
  thumb_tester_->inst_.word1_set_bits(index + (length - 1), index,
                                      (uint16_t) value);
}

ThumbWord2DecoderTester::ThumbWord2DecoderTester(
    ThumbDecoderTester* thumb_tester)
    : thumb_tester_(thumb_tester), pattern_("")
{}

ThumbWord2DecoderTester::~ThumbWord2DecoderTester() {}

void ThumbWord2DecoderTester::SetPattern(const char* pattern) {
  pattern_ = pattern;
  ASSERT_LE(static_cast<int>(kThumbWordSize),
            static_cast<int>(strlen(pattern))) <<
      "Thumb word 2 pattern length incorrect: " << pattern;
}

void ThumbWord2DecoderTester::Test() {
  TestAtIndex(kThumbWordSize);
}

const NamedClassDecoder& ThumbWord2DecoderTester::ExpectedDecoder() const {
  return thumb_tester_->ExpectedDecoder();
}

const char* ThumbWord2DecoderTester::InstContents() const {
  return thumb_tester_->InstContents();
}

char ThumbWord2DecoderTester::Pattern(int index) const {
  return pattern_[kThumbWordSize - (index + 1)];
}

void ThumbWord2DecoderTester::ProcessMatch() {
  // Expand word two (or if only a single word pattern, all possible
  // patterns that could follow it in word 2).
  thumb_tester_->ProcessMatch();
}

void ThumbWord2DecoderTester::InjectInstruction(
    nacl_arm_dec::Instruction inst) {
  thumb_tester_->InjectInstruction(inst);
}

void ThumbWord2DecoderTester::SetBit(int index, bool value) {
  ASSERT_LT(index, kThumbWordSize) << "ThumbWord2DecoderTester::SetBit("
                                   << index << ", " << ")";
  ASSERT_GE(index, 0) << "ThumbWord2DecoderTester::SetBit("
                      << index << ", " << ")";
  thumb_tester_->inst_.word2_set_bit(index, value);
}

void ThumbWord2DecoderTester::SetBitRange(
    int index, int length, uint32_t value) {
  ASSERT_LE(index + length, kThumbWordSize)
      << "ThumbWord2DecoderTester::SetBitRange(" << index << ", " << length
      << ", " << value << ")";
  ASSERT_GE(index, 0)
      << "ThumbWord1DecoderTester::SetBitRange(" << index << ", " << length
      << ", " << value << ")";
  thumb_tester_->inst_.word1_set_bits(index + (length - 1), index,
                                      (uint16_t) value);
}

ThumbDecoderTester::ThumbDecoderTester(
    const NamedClassDecoder& expected_decoder)
    : expected_decoder_(expected_decoder),
      pattern_(""),
      state_(),
      inst_((uint16_t) 0, (uint16_t) 0),
      word1_tester_(this),
      word2_tester_(this)
{}

ThumbDecoderTester::~ThumbDecoderTester() {
}

void ThumbDecoderTester::Test(const char* pattern) {
  int len = static_cast<int>(strlen(pattern_));
  pattern_ = pattern;
  if (kThumbWordSize == len) {
    word1_tester_.SetPattern(pattern_);
    word2_tester_.SetPattern("XXXXXXXXXXXXXXXX");
  } else {
    ASSERT_EQ(static_cast<int>((2*kThumbWordSize) + 1),
              static_cast<int>(strlen(pattern_)))
        << "Thumb attern length incorrect: " << pattern_;
    word1_tester_.SetPattern(pattern_);
    word2_tester_.SetPattern(pattern + kThumbWordSize + 1);
  }
  word1_tester_.Test();
}

const NamedClassDecoder& ThumbDecoderTester::ExpectedDecoder() const {
  return expected_decoder_;
}

const char* ThumbDecoderTester::InstContents() const {
  static char buffer[2 * kThumbWordSize + 2];
  for (int i = 1; i <= kThumbWordSize; ++i) {
    buffer[kThumbWordSize - i] = BoolToChar(inst_.word1_bit(i - 1));
  }
  buffer[kThumbWordSize] = '|';
  for (int i = 0; i < kThumbWordSize; ++i) {
    buffer[2 * kThumbWordSize - i] = BoolToChar(inst_.word2_bit(i));
  }
  buffer[2 * kThumbWordSize + 1] = '0';
  return buffer;
}

char ThumbDecoderTester::Pattern(int index) const {
  // This should never be called. The decoders word1_tester_
  // and word2_tester_ should be doing all the accesses to
  // the pattern.
  UNREFERENCED_PARAMETER(index);
  return '0';
}

void ThumbDecoderTester::ProcessMatch() {
  // Completed pattern, decode and test resulting state.
  const NamedClassDecoder& decoder = state_.decode_named(inst_);
  if (MAY_BE_SAFE == decoder.safety(inst_)) ApplySanityChecks(inst_, decoder);
  return;
}

void ThumbDecoderTester::InjectInstruction(nacl_arm_dec::Instruction inst) {
  inst_ = inst;
}

void ThumbDecoderTester::SetBit(int index, bool value) {
  // This should never be called. The decoders word1_tester_
  // and word2_tester_ should be doing all of the bit setting.
  GTEST_FAIL() << "ThumbDecoderTester::SetBit(" << index <<
      ", " << value << ")";
}

void ThumbDecoderTester::SetBitRange(int index, int length, uint32_t value) {
  // This should never be called. The decoders word1_tester_
  // and word2_tester_ should be doing all of the bit setting.
  GTEST_FAIL() << "ThumbDecoderTester::SetBitRange(" << index <<
      ", " << length << ", " << value << ")";
}

}  // namespace
