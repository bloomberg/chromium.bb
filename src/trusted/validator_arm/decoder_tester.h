/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Tests the decoder.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_DECODER_TESTER_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_DECODER_TESTER_H_

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error This file is not meant for use in the TCB
#endif

#include "native_client/src/trusted/validator_arm/named_class_decoder.h"
#include "native_client/src/trusted/validator_arm/gen/arm32_decode_named_decoder.h"

namespace nacl_arm_test {

// Defines a decoder tester that enumerates an instruction pattern,
// and tests that all of the decoded patterns match the expected
// class decoder, and that any additional sanity checks, specific
// to the instruction apply.
//
// Patterns are sequences of characters as follows:
//   '1' - Bit must be value 1.
//   '0' - Bit must be value 0.
//   'aaa...aa' (for some sequence of m lower case letters) -
//       Try all possible combinations of bits for the m bytes.
//   'AAA...A'  (for some sequence of m upper case letters) -
//       Try the following combinations:
//         (1) All m bits set to 1.
//         (2) All m bits set to 0.
//         (3) For each 4-bit subsequence, try all combinations,
//             setting remaining bits to 1.
//         (4) For each 4-bit subsequence, try all combinations,
//             setting remaining bits to 0.
//
//  In addition, for Thumb 2 word instructions, the '|' is used
//  to separate word 1 from word 2.
//  Also, bits are specified from the largest bit downto the smallest
//  bit.
class DecoderTester {
 public:
  DecoderTester();
  virtual ~DecoderTester() {}

  // Once an instruction is decoded, this method is called to apply
  // sanity checks on the matched decoder. The default checks that the
  // expected class name matches the name of the decoder, and that the
  // safety level is MAY_BE_SAFE. Returns whether further checking of
  // the instruction should be performed. In particular, false is returned
  // if there is some unencoded assumption that isn't put into the
  // test patterns, and hence, should not be checked.
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

  // Returns the expected decoder.
  virtual const NamedClassDecoder& ExpectedDecoder() const = 0;

  // Defines what should be done once a test pattern has been generated.
  virtual void ProcessMatch() = 0;

  // Allows the injection of an instruction. Used one to inject the instruction
  // that the subsequent call ProcessMatch will use.
  virtual void InjectInstruction(nacl_arm_dec::Instruction inst) = 0;

 protected:
  // Returns a printable version of the contents of the tested instruction.
  // Used to print out useful test failures.
  // Note: This function may not be thread safe, and the result may
  // only be valid till the next call to this method.
  virtual const char* InstContents() const = 0;

  // Returns the character at the given index in the pattern that
  // should be tested.
  virtual char Pattern(int index) const = 0;

  // Conceptually sets the corresponding bit in the instruction.
  virtual void SetBit(int index, bool value) = 0;

  // Conceptually sets the corresponding sequence of bits in the
  // instruction to the given value.
  virtual void SetBitRange(int index, int length, uint32_t value) = 0;

  // Expands the pattern starting at the given index in the pattern
  // being expanded.
  void TestAtIndex(int index);

  // Expands the pattern starting at the given index, filling in all
  // possible combinations for the next length bits.
  void TestAtIndexExpandAll(int index, int length);

  // Expands the pattern starting at the given index, filling in the
  // next length bits with the given value.
  void TestAtIndexExpandFill(int index, int length, bool value);

  // Expands the pattern starting at the given index, filling the length
  // bits with each possible subpattern of four bits, surrounded by the
  // given value.
  void TestAtIndexExpandFill4(int index, int length, bool value);

  // Expands the pattern starting at the given index, filling the
  // next stride bits with all possible combinations of 0 and 1,
  // followed by the length-stride bits being set to the given value.
  // Note: Current implementation limits stride to less than 32.
  void TestAtIndexExpandFillAll(int index,
                                int stride, int length, bool Value);

  // Fills the next length bits with the corresponding value being repeated
  // length times.
  void FillBitRange(int index, int length, bool value);
};

// Helper macro for testing if preconditions are met within the
// ApplySanityChecks method of a DecoderTester. That is, if the
// precondition is not met, exit the routine and return false.
// Otherwise, the precondition of the remaining code has been met,
// and execution continues.
#define NC_PRECOND(test) \
  { if (!(test)) return false; }

// Helper macro for testing if an (error) precondition a != b is met
// within the ApplySanityChecks method of a DecoderTester. That is,
// if a == b, generate a gtest error and then stop the application
// from doing further checks for the given instruction.
#define NC_EXPECT_NE_PRECOND(a, b) \
  { EXPECT_NE(a, b) << InstContents(); NC_PRECOND((a) == (b)); }

// Defines a decoder tester that enumerates an Arm32 instruction pattern,
// and tests that all of the decoded patterns match the expected class
// decoder, and that any additional sanity checks, specific to the
// instruction apply.
//
// Note: Patterns must be of length 32.
class Arm32DecoderTester : public DecoderTester {
 public:
  explicit Arm32DecoderTester(
      const NamedClassDecoder& expected_decoder);
  virtual ~Arm32DecoderTester();
  void Test(const char* pattern);
  virtual const NamedClassDecoder& ExpectedDecoder() const;
  virtual void ProcessMatch();
  virtual void InjectInstruction(nacl_arm_dec::Instruction inst);

 protected:
  virtual const char* InstContents() const;
  virtual char Pattern(int index) const;
  virtual void SetBit(int index, bool value);
  virtual void SetBitRange(int index, int length, uint32_t value);

  // The expected decoder class.
  const NamedClassDecoder& expected_decoder_;

  // The pattern being enumerated.
  const char* pattern_;

  // The decoder to use.
  NamedArm32DecoderState state_;

  // The instruction currently being enumerated.
  nacl_arm_dec::Instruction inst_;
};

class ThumbDecoderTester;

// Defines a decoder tester that enumerates the first word of
// a thumb instruction pattern, and tests that all of the decoded patterns
// match the expected class decoder, and that any additional sanity checks,
// specific to the instruction apply.
//
// Note: This class is used by a ThumbDecoderTester to enumerate the
// first word of the thumb instruction pattern.
class ThumbWord1DecoderTester : public DecoderTester {
 public:
  explicit ThumbWord1DecoderTester(ThumbDecoderTester* thumb_tester);
  virtual ~ThumbWord1DecoderTester();

  // Defines the pattern to use for word 1.
  void SetPattern(const char* pattern);

  // Test all possible patterns for word 1.
  void Test();

  virtual const NamedClassDecoder& ExpectedDecoder() const;
  virtual void ProcessMatch();
  virtual void InjectInstruction(nacl_arm_dec::Instruction inst);

 protected:
  virtual const char* InstContents() const;
  virtual char Pattern(int index) const;
  virtual void SetBit(int index, bool value);
  virtual void SetBitRange(int index, int length, uint32_t value);

  // The thumb tester that uses this decoder.
  ThumbDecoderTester* thumb_tester_;

  // The pattern for the first word of the thumb instruction.
  const char* pattern_;

  friend class ThumbDecoderTester;
};

// Defines a decoder tester that enumerates the second word of
// a thumb instruction pattern, and tests that all of the decoded patterns
// match the expected class decoder, and that any additional sanity checks,
// specific to the instruction apply.
//
// Note: This class is used by a ThumbDecoderTester to enumerate the
// second word of the thumb instruction pattern, or all possible patterns
// if the thumb instruction is a single word.
class ThumbWord2DecoderTester : public DecoderTester {
 public:
  explicit ThumbWord2DecoderTester(ThumbDecoderTester* thumb_tester);
  virtual ~ThumbWord2DecoderTester();

  // Defines the pattern to use for word 2.
  void SetPattern(const char* pattern);

  // Tests all patterns for word 2.
  void Test();

  virtual const NamedClassDecoder& ExpectedDecoder() const;
  virtual void ProcessMatch();
  virtual void InjectInstruction(nacl_arm_dec::Instruction inst);

 protected:
  virtual const char* InstContents() const;
  virtual char Pattern(int index) const;
  virtual void SetBit(int index, bool value);
  virtual void SetBitRange(int index, int length, uint32_t value);

  // The thumb tester that uses this decoder.
  ThumbDecoderTester* thumb_tester_;

  // The pattern for the second word of the thumb instruction.
  const char* pattern_;

 private:
  friend class ThumbDecoderTester;
  NACL_DISALLOW_COPY_AND_ASSIGN(ThumbWord2DecoderTester);
};

// Defines a decoder tester that enumerates a thumb instruction pattern,
// and tests that all of the decoded patterns match the expected class
// decoder, and that any additional sanity checks, specific to the
// instruction apply.
//
// Note: One word thumb instructions must be of length 16.
// Two word thumb instructions must be of length 32 (with a '|'
// separating each 16 character word pattern).
class ThumbDecoderTester : public DecoderTester {
 public:
  explicit ThumbDecoderTester(
      const NamedClassDecoder& expected_decoder);
  virtual ~ThumbDecoderTester();
  void Test(const char* pattern);
  virtual const NamedClassDecoder& ExpectedDecoder() const;
  virtual void ProcessMatch();
  virtual void InjectInstruction(nacl_arm_dec::Instruction inst);

 protected:
  virtual const char* InstContents() const;
  virtual char Pattern(int index) const;
  virtual void SetBit(int index, bool value);
  virtual void SetBitRange(int index, int length, uint32_t value);

  // The expected decoder class name.
  const NamedClassDecoder& expected_decoder_;

  // The pattern being enumerated.
  const char* pattern_;

  // TODO(karl): replace with thumb decoder state once defined.
  // The decoder to use.
  NamedArm32DecoderState state_;

  // The instruction currently being enumerated.
  nacl_arm_dec::Instruction inst_;

  // Decoder tester for the first word of the thumb instruction.
  ThumbWord1DecoderTester word1_tester_;

  // Decoeder tester for the second word of the thumb instruction.
  ThumbWord2DecoderTester word2_tester_;

 private:
  friend class ThumbWord1DecoderTester;
  friend class ThumbWord2DecoderTester;
  NACL_DISALLOW_COPY_AND_ASSIGN(ThumbDecoderTester);
};

}  // namespace

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_DECODER_TESTER_H_
