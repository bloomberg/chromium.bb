/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error This file is not meant for use in the TCB
#endif

/*
 * Unit tests for the ARM validator
 *
 * These tests use the google-test framework (gtest for short) to exercise the
 * ARM validator.  The tests currently fall into two rough categories:
 *  1. Simple method-level tests that exercise the validator's primitive
 *     capabilities, and
 *  2. Instruction pattern tests that run the entire validator.
 *
 * All instruction pattern tests use hand-assembled machine code fragments,
 * embedded as byte arrays.  This is somewhat ugly, but deliberate: it isolates
 * these tests from gas, which may be buggy or not installed.  It also lets us
 * hand-craft malicious bit patterns that gas may refuse to produce.
 *
 * To write a new instruction pattern, or make sense of an existing one, use the
 * ARMv7-A ARM (available online).  Instructions in this file are written as
 * 32-bit integers so the hex encoding matches the docs.
 */

#include <vector>
#include <string>
#include <sstream>

#include "gtest/gtest.h"
#include "native_client/src/include/nacl_macros.h"

#include "native_client/src/trusted/validator_arm/problem_reporter.h"
#include "native_client/src/trusted/validator_arm/validator.h"

using std::vector;
using std::string;
using std::ostringstream;

using nacl_arm_dec::Register;
using nacl_arm_dec::RegisterList;
using nacl_arm_dec::Instruction;

using nacl_arm_val::SfiValidator;
using nacl_arm_val::CodeSegment;
using nacl_arm_val::ProblemReporter;
using nacl_arm_val::ProblemSink;
using nacl_arm_val::ValidatorInstructionPairProblem;
using nacl_arm_val::ValidatorProblem;
using nacl_arm_val::ValidatorProblemMethod;
using nacl_arm_val::ValidatorProblemUserData;

namespace {

#ifdef __BIG_ENDIAN__
  #error This test will only succeed on a little-endian machine.  Sorry.
#endif

/*
 * Since ARM instructions are always little-endian, on a little-endian machine
 * we can represent them as ints.  This makes things somewhat easier to read
 * below.
 */
typedef uint32_t arm_inst;

/*
 * We use these parameters to initialize the validator, below.  They are
 * somewhat different from the parameters used in real NaCl systems, to test
 * degrees of validator freedom that we don't currently exercise in prod.
 */
// Number of bytes in each bundle.  Theoretically can also be 32 - not tested.
static const uint32_t kBytesPerBundle = 16;
// Limit code to 512MiB.
static const uint32_t kCodeRegionSize = 0x20000000;
// Limit data to 1GiB.
static const uint32_t kDataRegionSize = 0x40000000;
// Untrusted code must not write to r9.
static const RegisterList kAbiReadOnlyRegisters(Register(9));
// The stack pointer can be used for "free" loads and stores.
static const RegisterList kAbiDataAddrRegisters(nacl_arm_dec::kRegisterStack);


/*
 * Support code
 */

// Defines a buffer for error messages.
static const size_t kBufferSize = 256;
typedef char ErrorMessage[kBufferSize];

// Simply records the arguments given to ReportProblem, below.
class ProblemRecord {
 public:
  ProblemRecord(uint32_t vaddr,
                ValidatorProblem problem,
                ValidatorProblemMethod method,
                ValidatorProblemUserData user_data);

  ProblemRecord(const ProblemRecord& r);
  ~ProblemRecord() {}

  ProblemRecord& operator=(const ProblemRecord& r);

  uint32_t vaddr() const {
    return vaddr_;
  }

  ValidatorProblem problem() const {
    return problem_;
  }

  ValidatorProblemMethod method() const {
    return method_;
  }

  const uint32_t* user_data() const {
    return user_data_;
  }

 private:
  void Init(const ValidatorProblemUserData user_data);

  uint32_t vaddr_;
  ValidatorProblem problem_;
  ValidatorProblemMethod method_;
  ValidatorProblemUserData user_data_;
};

ProblemRecord::ProblemRecord(uint32_t vaddr,
                             ValidatorProblem problem,
                             ValidatorProblemMethod method,
                             ValidatorProblemUserData user_data)
    : vaddr_(vaddr),
      problem_(problem),
      method_(method) {
  Init(user_data);
}

ProblemRecord::ProblemRecord(const ProblemRecord& r)
    : vaddr_(r.vaddr_),
      problem_(r.problem_),
      method_(r.method_) {
  Init(r.user_data_);
}

ProblemRecord& ProblemRecord::operator=(const ProblemRecord& r) {
  vaddr_ = r.vaddr_;
  problem_ = r.problem_;
  method_ = r.method_;
  Init(r.user_data_);
  return *this;
}

void ProblemRecord::Init(const ValidatorProblemUserData user_data) {
  for (size_t i = 0; i < nacl_arm_val::ValidatorProblemUserDataSize; ++i) {
    user_data_[i] = user_data[i];
  }
}

// A ProblemSink that records all calls (implementation of the Spy pattern)
class ProblemSpy : public ProblemReporter {
 public:
  // We want *all* the errors that the validator produces.  Note that this means
  // we're not testing the should_continue functionality.  This is probably
  // okay.
  virtual bool should_continue() { return true; }

  // Returns the list of found problems.
  vector<ProblemRecord> &get_problems() { return problems_; }

  // Returns the safety level associated with the recorded problem.
  nacl_arm_dec::SafetyLevel GetSafetyLevel(const ProblemRecord& record);

  // Returns the 2-instruction validator problem with the instruction,
  // if the problem record is for a two-instruction problem, and
  // kNoSpecificPairProblem for any single instruction validator problem.
  ValidatorInstructionPairProblem GetPairProblem(const ProblemRecord& record);

  // Returns true if the first instruction in the instruction pair appears
  // to be the reporting instruction (returns true for single instruction
  // problems).
  bool IsPairProblemAtFirst(const ProblemRecord& record);

  // Generates the corresponding error message into the given character
  // buffer.
  void GetErrorMessage(ErrorMessage message, const ProblemRecord& record);

 protected:
  virtual void ReportProblemInternal(uint32_t vaddr,
                                     ValidatorProblem problem,
                                     ValidatorProblemMethod method,
                                     ValidatorProblemUserData user_data);

 private:
  vector<ProblemRecord> problems_;
};

nacl_arm_dec::SafetyLevel ProblemSpy::GetSafetyLevel(
    const ProblemRecord& record) {
  nacl_arm_dec::SafetyLevel safety = nacl_arm_dec::MAY_BE_SAFE;
  if (record.method() == nacl_arm_val::kReportProblemSafety) {
    ExtractProblemSafety(record.user_data(), &safety);
  }
  return safety;
}

ValidatorInstructionPairProblem ProblemSpy::GetPairProblem(
    const ProblemRecord& record) {
  ValidatorInstructionPairProblem problem =
      nacl_arm_val::kNoSpecificPairProblem;
  if (record.method() == nacl_arm_val::kReportProblemInstructionPair) {
    ExtractProblemInstructionPair(record.user_data(), &problem,
                                  NULL, NULL, NULL, NULL);
  } else if (record.method() ==
             nacl_arm_val::kReportProblemRegisterInstructionPair) {
    ExtractProblemRegisterInstructionPair(
        record.user_data(), &problem, NULL, NULL, NULL, NULL, NULL);
  } else if (record.method() ==
             nacl_arm_val::kReportProblemRegisterListInstructionPair) {
    ExtractProblemRegisterListInstructionPair(
        record.user_data(), &problem, NULL, NULL, NULL, NULL, NULL);
  }
  return problem;
}

bool ProblemSpy::IsPairProblemAtFirst(const ProblemRecord& record) {
  uint32_t first_address = record.vaddr();  // default to current location.
  if (record.method() == nacl_arm_val::kReportProblemInstructionPair) {
    ExtractProblemInstructionPair(record.user_data(), NULL,
                                  &first_address, NULL, NULL, NULL);
  } else if (record.method() ==
             nacl_arm_val::kReportProblemRegisterInstructionPair) {
    ExtractProblemRegisterInstructionPair(
        record.user_data(), NULL, NULL, &first_address, NULL, NULL, NULL);
  } else if (record.method() ==
             nacl_arm_val::kReportProblemRegisterListInstructionPair) {
    ExtractProblemRegisterListInstructionPair(
        record.user_data(), NULL, NULL, &first_address, NULL, NULL, NULL);
  }
  return record.vaddr() == first_address;
}

void ProblemSpy::GetErrorMessage(ErrorMessage message,
                                 const ProblemRecord& record) {
  ToText(message, kBufferSize, record.vaddr(), record.problem(),
         record.method(), record.user_data());
}

void ProblemSpy::ReportProblemInternal(uint32_t vaddr,
                                       ValidatorProblem problem,
                                       ValidatorProblemMethod method,
                                       ValidatorProblemUserData user_data) {
  ProblemRecord prob(vaddr, problem, method, user_data);
  problems_.push_back(prob);
}


/*
 * Coordinates the fixture objects used by test cases below.  This is
 * forward-declared to the greatest extent possible so we can get on to the
 * important test stuff below.
 */
class ValidatorTests : public ::testing::Test {
 protected:
  ValidatorTests();

  // Utility method for validating a sequence of bytes.
  bool validate(const arm_inst *pattern,
                size_t inst_count,
                uint32_t start_addr,
                ProblemSink *sink);

  // Tests an arbitrary-size instruction fragment that is expected to pass.
  // Does not modulate or rewrite the pattern in any way.
  void validation_should_pass(const arm_inst *pattern,
                              size_t inst_count,
                              uint32_t base_addr,
                              const string &msg);

  // Tests a two-instruction pattern that's expected to pass, at each possible
  // bundle alignment. This also tries the pattern across bundle boundaries,
  // and makes sure it fails.
  void validation_should_pass2(const arm_inst *pattern,
                               size_t inst_count,
                               uint32_t base_addr,
                               const string &msg);

  // Tests a pattern that is forbidden in the SFI model.
  //
  // Note that the 'msg1' and 'msg2' parameters are merely concatentated in the
  // output.
  vector<ProblemRecord> validation_should_fail(const arm_inst *pattern,
                                               size_t inst_count,
                                               uint32_t base_addr,
                                               const string &msg);

  // Tests an instruction with all possible conditions (i.e. all except 1111),
  // verifying all cases are safe.
  void all_cond_values_pass(const arm_inst prototype,
                            uint32_t base_addr,
                            const string &msg);

  // Tests an instruction with all possible conditions (i.e. all except 1111),
  // verifying all cases are unsafe.
  void all_cond_values_fail(const arm_inst prototype,
                            uint32_t base_addr,
                            const string &msg);

  SfiValidator _validator;

 private:
  // Returns the given instruction, after modifying the instruction condition
  // to the given value.
  arm_inst ChangeCond(arm_inst inst, Instruction::Condition c) {
    return (inst & 0x0fffffff) | (static_cast<arm_inst>(c) << 28);
  }
};


/*
 * Primitive tests checking various constructor properties.  Any of these
 * failing would be a very bad sign indeed.
 */

TEST_F(ValidatorTests, RecognizesDataAddressRegisters) {
  // Note that the logic below needs to be kept in sync with the definition
  // of kAbiDataAddrRegisters at the top of this file.
  //
  // This test is pretty trivial -- we can exercise the data_address_register
  // functionality more deeply with pattern tests below.
  for (int i = 0; i < 16; i++) {
    Register r(i);
    if (r.Equals(nacl_arm_dec::kRegisterStack)) {
      EXPECT_TRUE(_validator.is_data_address_register(r))
          << "Stack pointer must be a data address register.";
    } else {
      EXPECT_FALSE(_validator.is_data_address_register(r))
          << "Only the stack pointer must be a data address register.";
    }
  }
}

TEST_F(ValidatorTests, GeneratesCorrectMasksFromSizes) {
  EXPECT_EQ(0xC0000000, _validator.data_address_mask());
  EXPECT_EQ(0xE000000F, _validator.code_address_mask());

  // Reinitialize the validator to test a different bundle size.
  _validator = SfiValidator(32,
                            kCodeRegionSize,
                            kDataRegionSize,
                            kAbiReadOnlyRegisters,
                            kAbiDataAddrRegisters);
  EXPECT_EQ(0xC0000000, _validator.data_address_mask())
      << "Changes in bundle size should not affect the data mask.";
  EXPECT_EQ(0xE000001F, _validator.code_address_mask())
      << "Changes in bundle size must affect the code mask.";
}

// Code validation tests

// This is where untrusted code starts.  Most tests use this.
static const uint32_t kDefaultBaseAddr = 0x20000;

// Here are examples of every form of safe store permitted in a Native Client
// program.  These stores have common properties:
//  1. The high nibble is 0, to allow tests to write an arbitrary predicate.
//  2. They address memory only through r1.
//  3. They do not do anything dumb, like try to alter SP or PC.
struct AnnotatedInstruction {
  arm_inst inst;
  const char *about;
};
static const AnnotatedInstruction examples_of_safe_stores[] = {
  // Single-register stores
  { 0x05810000, "str r0, [r1]: simple no-displacement store" },
  { 0x05810123, "str r0, [r1, #0x123]: positive displacement" },
  { 0x05010123, "str r0, [r1, #-0x123]: negative displacement" },
  { 0x05A10123, "str r0, [r1, #0x123]!: positive disp + writeback" },
  { 0x05210123, "str r0, [r1, #-0x123]!: negative disp + writeback" },
  { 0x04810123, "str r0, [r1], #0x123: positive post-indexing" },
  { 0x04010123, "str r0, [r1], #-0x123: negative post-indexing" },
  { 0x06810002, "str r0, [r1], r2: positive register post-indexing" },
  { 0x06010002, "str r0, [r1], -r2: negative register post-indexing" },

  // Two-register store
  { 0x01C120F0, "strd r2, r3, [r1]: basic 64-bit store" },
  { 0x01C124F2, "strd r2, r3, [r1, #42]: positive disp 64-bit store" },
  { 0x014124F2, "strd r2, r3, [r1, #-42]: negative disp 64-bit store" },
  { 0x01E124F2, "strd r2, r3, [r1, #42]!: positive disp 64-bit store + wb" },
  { 0x016124F2, "strd r2, r3, [r1, #-42]!: negative disp 64-bit store + wb" },
  { 0x00C124F2, "strd r2, r3, [r1], #42: post-inc 64-bit store" },
  { 0x004124F2, "strd r2, r3, [r1], #-42: post-dec 64-bit store" },

  // Store-exclusive
  { 0x01810F92, "strex r0, r2, [r1]: store exclusive" },

  // Store-multiple
  { 0x0881FFFF, "stm r1, { r0-r15 }: store multiple, no writeback" },
  { 0x08A1FFFF, "stm r1!, { r0-r15 }: store multiple, writeback" },

  // Stores from the floating point / vector register file
  // These all compile to STC instructions.
  { 0x0D810B00, "vstr d0, [r1]: direct vector store" },
  { 0x0D810B99, "vstr d0, [r1, #0x99]: positive displacement vector store" },
  { 0x0D010B99, "vstr d0, [r1, #-0x99]: negative displacement vector store" },
  { 0x0C810B10, "vstmia r1, { d0-d7 }: no writeback" },
  { 0x0CA10B10, "vstmia r1!, { d0-d7 }: writeback" },
};

static const AnnotatedInstruction examples_of_safe_masks[] = {
  { 0x03C11103, "bic r1, r1, #0xC0000000: simple in-place mask (form 1)" },
  { 0x03C114C0, "bic r1, r1, #0xC0000000: simple in-place mask (form 2)" },
  { 0x03C314C0, "bic r1, r3, #0xC0000000: mask with register move" },
  { 0x03C114FF, "bic r1, r1, #0xFF000000: overzealous but correct mask" },
};

TEST_F(ValidatorTests, SafeMaskedStores) {
  // Produces many examples of masked stores using the safe store table (above)
  // and the list of possible masking instructions (below).
  //
  // Each mask instruction must leave a valid (data) address in r1.
  for (unsigned p = 0; p < 15; p++) {
    // Conditionally executed instructions have a top nibble of 0..14.
    // 15 is an escape sequence used to fit in additional encodings.
    arm_inst predicate = p << 28;

    for (unsigned m = 0; m < NACL_ARRAY_SIZE(examples_of_safe_masks); m++) {
      for (unsigned s = 0; s < NACL_ARRAY_SIZE(examples_of_safe_stores); s++) {
        ostringstream message;
        message << examples_of_safe_masks[m].about
                << ", "
                << examples_of_safe_stores[s].about
                << " (predicate #" << p << ")";
        arm_inst program[] = {
          examples_of_safe_masks[m].inst | predicate,
          examples_of_safe_stores[s].inst | predicate,
        };
        validation_should_pass2(program,
                                2,
                                kDefaultBaseAddr,
                                message.str());
      }
    }
  }
}

/* TODO(karl): Add these back once simd instructions are turned back on.
// These stores can't be predicated, so we must use a different, simpler
// fixture generator.
static const AnnotatedInstruction examples_of_safe_unconditional_stores[] = {
  // Vector stores
  { 0xF481A5AF, "vst2.16 {d10[2],d12[2]}, [r1]: simple vector store" },
  { 0xF401A64F, "vst1.16 {d10-d12}, [r1]: larger vector store" },
  { 0xF4010711, "vst1.8 {d0}, [r1, :64], r1: register post-increment" },
};

TEST_F(ValidatorTests, SafeUnconditionalMaskedStores) {
  // Produces many examples of unconditional masked stores using the safe
  // unconditional store table (above) and the list of possible masking
  // instructions (below).
  //
  // Each mask instruction must leave a valid (data) address in r1.

  // These instructions can't be predicated.
  arm_inst predicate = 0xE0000000;  // "always"

  for (unsigned m = 0; m < NACL_ARRAY_SIZE(examples_of_safe_masks); m++) {
    for (unsigned s = 0;
         s < NACL_ARRAY_SIZE(examples_of_safe_unconditional_stores);
         s++) {
      ostringstream message;
      message << examples_of_safe_masks[m].about
              << ", "
              << examples_of_safe_unconditional_stores[s].about;
      arm_inst program[] = {
        examples_of_safe_masks[m].inst | predicate,
        examples_of_safe_unconditional_stores[s].inst,
      };
      validation_should_pass2(program,
                              2,
                              kDefaultBaseAddr,
                              message.str());
    }
  }
}
*/

TEST_F(ValidatorTests, SafeConditionalStores) {
  // Produces many examples of conditional stores using the safe store table
  // (above) and the list of possible conditional guards (below).
  //
  // Each conditional guard must set the Z flag iff r1 contains a valid address.
  static const AnnotatedInstruction guards[] = {
    { 0x03110103, "tst r1, #0xC0000000: precise guard, GCC encoding" },
    { 0x031104C0, "tst r1, #0xC0000000: precise guard, alternative encoding" },
    { 0x031101C3, "tst r1, #0xF0000000: overzealous (but correct) guard" },
  };

  // Currently we only support *unconditional* conditional stores.
  // Meaning the guard is unconditional and the store is if-equal.
  arm_inst guard_predicate = 0xE0000000, store_predicate = 0x00000000;
  for (unsigned m = 0; m < NACL_ARRAY_SIZE(guards); m++) {
    for (unsigned s = 0; s < NACL_ARRAY_SIZE(examples_of_safe_stores); s++) {
      ostringstream message;
      message << guards[m].about
              << ", "
              << examples_of_safe_stores[s].about
              << " (predicate #" << guard_predicate << ")";
      arm_inst program[] = {
        guards[m].inst | guard_predicate,
        examples_of_safe_stores[s].inst | store_predicate,
      };
      validation_should_pass2(program,
                              2,
                              kDefaultBaseAddr,
                              message.str());
    }
  }
}


TEST_F(ValidatorTests, InvalidMasksOnSafeStores) {
  static const AnnotatedInstruction examples_of_invalid_masks[] = {
    { 0x01A01003, "mov r1, r3: not even a mask" },
    { 0x03C31000, "bic r1, r3, #0: doesn't mask anything" },
    { 0x03C31102, "bic r1, r3, #0x80000000: doesn't mask enough bits" },
    { 0x03C311C1, "bic r1, r3, #0x70000000: masks the wrong bits" },
  };

  for (unsigned p = 0; p < 15; p++) {
    // Conditionally executed instructions have a top nibble of 0..14.
    // 15 is an escape sequence used to fit in additional encodings.
    arm_inst predicate = p << 28;

    for (unsigned m = 0; m < NACL_ARRAY_SIZE(examples_of_invalid_masks); m++) {
      for (unsigned s = 0; s < NACL_ARRAY_SIZE(examples_of_safe_stores); s++) {
        ostringstream message;
        message << examples_of_invalid_masks[m].about
                << ", "
                << examples_of_safe_stores[s].about
                << " (predicate #" << p << ")";
        arm_inst program[] = {
          examples_of_invalid_masks[m].inst | predicate,
          examples_of_safe_stores[s].inst | predicate,
        };

        vector<ProblemRecord> problems =
            validation_should_fail(program,
                                   NACL_ARRAY_SIZE(program),
                                   kDefaultBaseAddr,
                                   message.str());

        // EXPECT/continue rather than ASSERT so that we run the other cases.
        EXPECT_EQ(1U, problems.size());
        if (problems.size() != 1) continue;

        ProblemRecord first = problems[0];
        EXPECT_EQ(kDefaultBaseAddr + 4, first.vaddr())
            << "Problem report must point to the store: "
            << message.str();
        EXPECT_NE(nacl_arm_val::kReportProblemSafety, first.method())
            << "Store should not be unsafe even though mask is bogus: "
            << message.str();
        EXPECT_EQ(nacl_arm_val::kProblemUnsafeLoadStore, first.problem())
            << message;
      }
    }
  }
}

TEST_F(ValidatorTests, InvalidGuardsOnSafeStores) {
  static const AnnotatedInstruction invalid_guards[] = {
    { 0x03110100, "tst r1, #0: always sets Z" },
    { 0x03110102, "tst r1, #0x80000000: doesn't test enough bits" },
    { 0x031101C1, "tst r1, #0x70000000: doesn't test the right bits" },
    { 0x01A01003, "mov r1, r3: not even a test" },
    { 0x03310103, "teq r1, #0xC0000000: does the inverse of what we want" },
    { 0x03510103, "cmp r1, #0xC0000000: does the inverse of what we want" },
  };

  // We don't currently support conditional versions of the conditional guard.
  //
  // TODO(cbiffle): verify this in the test
  static const arm_inst guard_predicate = 0xE0000000;  // unconditional
  static const arm_inst store_predicate = 0x00000000;  // if-equal

  for (unsigned m = 0; m < NACL_ARRAY_SIZE(invalid_guards); m++) {
    for (unsigned s = 0; s < NACL_ARRAY_SIZE(examples_of_safe_stores); s++) {
      ostringstream message;
      message << invalid_guards[m].about
              << ", "
              << examples_of_safe_stores[s].about;
      arm_inst program[] = {
        invalid_guards[m].inst | guard_predicate,
        examples_of_safe_stores[s].inst | store_predicate,
      };

      vector<ProblemRecord> problems =
          validation_should_fail(program,
                                 NACL_ARRAY_SIZE(program),
                                 kDefaultBaseAddr,
                                 message.str());

      // EXPECT/continue rather than ASSERT so that we run the other cases.
      EXPECT_EQ(1U, problems.size());
      if (problems.size() != 1) continue;

      ProblemRecord first = problems[0];
      EXPECT_EQ(kDefaultBaseAddr + 4, first.vaddr())
          << "Problem report must point to the store: "
          << message.str();
      EXPECT_NE(nacl_arm_val::kReportProblemSafety, first.method())
          << "Store should not be unsafe even though guard is bogus: "
          << message.str();
      EXPECT_EQ(nacl_arm_val::kProblemUnsafeLoadStore, first.problem())
          << message;
    }
  }
}

TEST_F(ValidatorTests, ValidMasksOnUnsafeStores) {
  static const AnnotatedInstruction invalid_stores[] = {
    { 0x07810002, "str r0, [r1, r2]: register-plus-register addressing" },
    { 0x07010002, "str r0, [r1, -r2]: register-minus-register addressing" },
    { 0x07810182, "str r0, [r1, r2, LSL #3]: complicated addressing 1" },
    { 0x07018482, "str r0, [r1, -r2, ASR #16]: complicated addressing 2" },
  };

  for (unsigned p = 0; p < 15; p++) {
    // Conditionally executed instructions have a top nibble of 0..14.
    // 15 is an escape sequence used to fit in additional encodings.
    arm_inst predicate = p << 28;

    for (unsigned m = 0; m < NACL_ARRAY_SIZE(examples_of_safe_masks); m++) {
      for (unsigned s = 0; s < NACL_ARRAY_SIZE(invalid_stores); s++) {
        ostringstream message;
        message << examples_of_safe_masks[m].about
                << ", "
                << invalid_stores[s].about
                << " (predicate #" << p << ")";
        arm_inst program[] = {
          examples_of_safe_masks[m].inst | predicate,
          invalid_stores[s].inst | predicate,
        };

        vector<ProblemRecord> problems =
            validation_should_fail(program,
                                   NACL_ARRAY_SIZE(program),
                                   kDefaultBaseAddr,
                                   message.str());

        // EXPECT/continue rather than ASSERT so that we run the other cases.
        EXPECT_EQ(1U, problems.size());
        if (problems.size() != 1) continue;

        ProblemRecord first = problems[0];
        EXPECT_EQ(kDefaultBaseAddr + 4, first.vaddr())
            << "Problem report must point to the store: "
            << message.str();
        EXPECT_EQ(nacl_arm_val::kReportProblemSafety, first.method())
            << "Store must be flagged by the decoder as unsafe: "
            << message.str();
        EXPECT_EQ(nacl_arm_val::kProblemUnsafe, first.problem())
            << message;
      }
    }
  }
}

TEST_F(ValidatorTests, ScaryUndefinedInstructions) {
  // These instructions are undefined today (ARMv7-A) but may become defined
  // tomorrow.  We ban them since we can't reason about their side effects.
  static const AnnotatedInstruction undefined_insts[] = {
    { 0xE05DEA9D, "An undefined instruction in the multiply space" },
  };
  for (unsigned i = 0; i < NACL_ARRAY_SIZE(undefined_insts); i++) {
    arm_inst program[] = { undefined_insts[i].inst };

    vector<ProblemRecord> problems =
        validation_should_fail(program,
                               NACL_ARRAY_SIZE(program),
                               kDefaultBaseAddr,
                               undefined_insts[i].about);

    // EXPECT/continue rather than ASSERT so that we run the other cases.
    EXPECT_EQ(1U, problems.size());
    if (problems.size() != 1) continue;

    ProblemSpy spy;
    ProblemRecord first = problems[0];
    EXPECT_EQ(kDefaultBaseAddr, first.vaddr())
        << "Problem report must point to the only instruction: "
        << undefined_insts[i].about;
    EXPECT_EQ(nacl_arm_val::kReportProblemSafety, first.method())
        << "Store must be flagged by the decoder as unsafe: "
        << undefined_insts[i].about;
    EXPECT_EQ(nacl_arm_dec::UNDEFINED, spy.GetSafetyLevel(first))
        << "Instruction must be flagged as UNDEFINED: "
        << undefined_insts[i].about;
    EXPECT_EQ(nacl_arm_val::kProblemUnsafe, first.problem())
        << "Instruction must be marked unsafe: "
        << undefined_insts[i].about;
  }
}

TEST_F(ValidatorTests, LessScaryUndefinedInstructions) {
  // These instructions are specified by ARM as *permanently* undefined, so we
  // treat them as a reliable Illegal Instruction trap.

  static const AnnotatedInstruction perm_undefined[] = {
    { 0xE7FFDEFE, "permanently undefined instruction produced by LLVM" },
  };

  for (unsigned i = 0; i < NACL_ARRAY_SIZE(perm_undefined); i++) {
    arm_inst program[] = { perm_undefined[i].inst };
    validation_should_pass(program,
                           1,
                           kDefaultBaseAddr,
                           perm_undefined[i].about);
  }
}

TEST_F(ValidatorTests, PcRelativeFirstInst) {
  // Note: This tests the fix for issue 2771.
  static const arm_inst pcrel_boundary_tests[] = {
    0xe59f0000,  // ldr     r0, [pc, #0]
    0xe320f000,  // nop     {0}
    0xe320f000,  // nop     {0}
    0xe320f000,  // nop     {0}"
  };
  validation_should_pass(pcrel_boundary_tests,
                         NACL_ARRAY_SIZE(pcrel_boundary_tests),
                         kDefaultBaseAddr,
                         "pc relative first instruction in first bundle");
}

TEST_F(ValidatorTests, PcRelativeFirst2ndBundle) {
  // Note: This tests the fix for issue 2771.
  static const arm_inst pcrel_boundary_tests[] = {
    0xe320f000,  // nop     {0}
    0xe320f000,  // nop     {0}
    0xe320f000,  // nop     {0}
    0xe320f000,  // nop     {0}
    0xe59f0000,  // ldr     r0, [pc, #0]
  };
  validation_should_pass(pcrel_boundary_tests,
                         NACL_ARRAY_SIZE(pcrel_boundary_tests),
                         kDefaultBaseAddr,
                         "pc relative first instruction in 2nd bundle");
}

TEST_F(ValidatorTests, SafeConditionalBicLdrTest) {
  // Test if we fixed bug with conditional Bic Loads (issue 2769).
  static const arm_inst bic_ldr_safe_test[] = {
    0x03c22103,  // biceq   r2, r2, #-1073741824    ; 0xc0000000
    0x01920f9f,  // ldrexeq r0, [r2]
  };
  validation_should_pass(bic_ldr_safe_test,
                         NACL_ARRAY_SIZE(bic_ldr_safe_test),
                         kDefaultBaseAddr,
                         "Safe conditional bic ldr test");
}

TEST_F(ValidatorTests, ConditionalBicsLdrTest) {
  // Test if we fail because Bic updates the flags register, making
  // the conditional Bic load incorrect (issue 2769).
  static const arm_inst bics_ldr_unsafe_test[] = {
    0x03d22103,  // bicseq  r2, r2, #-1073741824    ; 0xc0000000
    0x01920f9f,  // ldrexeq r0, [r2]
  };
  vector<ProblemRecord> problems =
      validation_should_fail(bics_ldr_unsafe_test,
                             NACL_ARRAY_SIZE(bics_ldr_unsafe_test),
                             kDefaultBaseAddr,
                             "Conditional bics ldr test");
  EXPECT_EQ(1U, problems.size());
  if (problems.size() == 0) return;

  ProblemSpy spy;
  ProblemRecord problem = problems[0];
  EXPECT_EQ(kDefaultBaseAddr + 4, problem.vaddr())
      << "Problem report should point to the ldr instruction.";
  EXPECT_NE(nacl_arm_val::kReportProblemSafety, problem.method());
  EXPECT_EQ(nacl_arm_dec::MAY_BE_SAFE, spy.GetSafetyLevel(problem));
  EXPECT_EQ(nacl_arm_val::kProblemUnsafeLoadStore, problem.problem());
}

TEST_F(ValidatorTests, DifferentConditionsBicLdrTest) {
  // Test if we fail because the Bic and Ldr instructions have
  // different conditional flags.
  static const arm_inst bic_ldr_diff_conds[] = {
    0x03c22103,  // biceq   r2, r2, #-1073741824    ; 0xc0000000
    0xc1920f9f,  // ldrexgt r0, [r2]
  };
  vector<ProblemRecord> problems=
      validation_should_fail(bic_ldr_diff_conds,
                             NACL_ARRAY_SIZE(bic_ldr_diff_conds),
                             kDefaultBaseAddr,
                             "Different conditions bic ldr test");
  EXPECT_EQ(1U, problems.size());
  if (problems.size() == 0) return;

  ProblemSpy spy;
  ProblemRecord problem = problems[0];
  EXPECT_EQ(kDefaultBaseAddr + 4, problem.vaddr())
      << "Problem report should point to the ldr instruction.";
  EXPECT_NE(nacl_arm_val::kReportProblemSafety, problem.method());
  EXPECT_EQ(nacl_arm_dec::MAY_BE_SAFE, spy.GetSafetyLevel(problem));
  EXPECT_EQ(nacl_arm_val::kProblemUnsafeLoadStore, problem.problem());
}

TEST_F(ValidatorTests, BfcLdrInstGoodTest) {
  // Test if we can use bfc to clear mask bits.
  static const arm_inst bfc_inst[] = {
    0xe7df2f1f,  // bfc r2, #30, #2
    0xe1920f9f,  // ldrex r0, [r2]
  };
  validation_should_pass(bfc_inst,
                         NACL_ARRAY_SIZE(bfc_inst),
                         kDefaultBaseAddr,
                         "Bfc Lcr instruction mask good test");
}

TEST_F(ValidatorTests, BfcLdrInstMaskTooBigTest) {
  // Run test where bfc mask is too big (acceptable to mask off more than
  // needed).
  static const arm_inst bfc_inst[] = {
    0xe7df2e9f,  // bfc r2, #29, #3
    0xe1920f9f,  // ldrex r0, [r2]
  };
  validation_should_pass(bfc_inst,
                         NACL_ARRAY_SIZE(bfc_inst),
                         kDefaultBaseAddr,
                         "Bfc Ldr instruction mask too big test");
}

TEST_F(ValidatorTests, BfcLdrInstMaskWrongPlaceTest) {
  // Run test where bfc mask is in the wrong place.
  static const arm_inst bfc_inst[] = {
    0xe7da2c9f,  // bfc r2, #25, #2
    0xe1920f9f,  // ldrex r0, [r2]
  };
  validation_should_fail(bfc_inst,
                         NACL_ARRAY_SIZE(bfc_inst),
                         kDefaultBaseAddr,
                         "Bfc Ldr instruction mask wrong place test");
}

TEST_F(ValidatorTests, BfcLdrexMaskOkTestDoesPrecede) {
  // Run test where Bfc is unconditional, and followed by ldrex.
  // Note: Implicitly tests provably_precedes.
  static const arm_inst bfc_inst[] = {
    0xe7df2f1f,  // bfc r2, #30, #2
    0x01920f9f,  // ldrexeq r0, [r2]
  };
  validation_should_pass(bfc_inst,
                         NACL_ARRAY_SIZE(bfc_inst),
                         kDefaultBaseAddr,
                         "Bfc Ldreq instruction mask ok, "
                         "tests provably_precedes succeeds");
}

TEST_F(ValidatorTests, BfcLdrexeqMaskOkTestDoesntPrecede) {
  // Run test where Bfc, and followed by ldrexeq.
  // Note: Implicitly tests provably_precedes.
  static const arm_inst bfc_inst[] = {
    0x07df2f1f,  // bfceq r2, #30, #2
    0xe1920f9f,  // ldrex r0, [r2]
  };
  validation_should_fail(bfc_inst,
                         NACL_ARRAY_SIZE(bfc_inst),
                         kDefaultBaseAddr,
                         "Bfc Ldrexeq instruction mask ok, "
                         "tests provably_precedes fails");
}

TEST_F(ValidatorTests, BicMaskTestDoesPrecede) {
  // Test bic followed by bxeq.
  // Note: Implicitly tests provably_precedes.
  static const arm_inst inst[] = {
    0xe3cee2fe,  // bic lr, lr, #-536870897     ; 0xe000000f
    0x012fff1e,  // bxeq lr
  };
  validation_should_pass(inst,
                         NACL_ARRAY_SIZE(inst),
                         kDefaultBaseAddr,
                         "Bic mask label, then bxeq jump, "
                         "tests provably_precedes succeeds");
}

TEST_F(ValidatorTests, BicMaskTestDoesntPrecede) {
  // Test biceq followed by bx.
  // Note: Implicitly tests provably_precedes.
  static const arm_inst inst[] = {
    0x03cee2fe,  // biceq lr, lr, #-536870897   ; 0xe000000f
    0xe12fff1e,  // bx lr
  };
  validation_should_fail(inst,
                         NACL_ARRAY_SIZE(inst),
                         kDefaultBaseAddr,
                         "Biceq mask label, then bx jump, "
                         "tests provably_precedes fails");
}

// TODO(karl): Add pattern rules and test cases for using bfc to update SP.

TEST_F(ValidatorTests, AddConstToSpTest) {
  // Show that we can add a constant to the stack pointer is fine,
  // so long as we follow it with a mask instruction.
  static const arm_inst sp_inst[] = {
    0xe28dd00c,  // add sp, sp, #12
    0xe3cdd2ff,  // bic     sp, sp, #-268435441     ; 0xf000000f
  };
  validation_should_pass(sp_inst,
                         NACL_ARRAY_SIZE(sp_inst),
                         kDefaultBaseAddr,
                         "Add constant (12) to sp, then mask with bic");
}

TEST_F(ValidatorTests, AddConstToSpBicTestDoesFollows) {
  // Run test where we add a constant to a stack pointer, followed
  // by a maks.
  // Note: Implicitly tests provably_follows.
  static const arm_inst sp_inst[] = {
    0x028dd00c,  // addeq sp, sp, #12
    0xe3cdd2ff,  // bic sp, sp, #-268435441     ; 0xf000000f
  };
  validation_should_pass(sp_inst,
                         NACL_ARRAY_SIZE(sp_inst),
                         kDefaultBaseAddr,
                         "Add constant (12) to sp, then mask with bic, "
                         "tests provably_follows succeeds");
}

TEST_F(ValidatorTests, AddConstToSpBicTestDoesntFollows) {
  // Run test where we add a constant to a stack pointer, followed
  // by a maks.
  // Note: Implicitly tests provably_follows.
  static const arm_inst sp_inst[] = {
    0xe28dd00c,  // add sp, sp, #12
    0x03cdd2ff,  // biceq sp, sp, #-268435441   ; 0xf000000f
  };
  validation_should_fail(sp_inst,
                         NACL_ARRAY_SIZE(sp_inst),
                         kDefaultBaseAddr,
                         "Add constant (12) to sp, then mask with bic, "
                         "tests provably_follows fails");
}

TEST_F(ValidatorTests, CheckVectorLoadPcRelative) {
  // Run test where we do a vector load using a pc relative address.
  // Corresponds to issue 2906.
  static const arm_inst load_inst[] = {
    0xed9f0b04,  // vldr        d0, [pc, #16]
  };
  validation_should_pass(load_inst,
                         NACL_ARRAY_SIZE(load_inst),
                         kDefaultBaseAddr,
                         "Load vector register using pc relative address");
}

TEST_F(ValidatorTests, CheckPushSpUnpredictable) {
  // Run test to verify that "Push {sp}", encoding A2 on a8-248 of ARM manual,
  // is unpredictable (i.e. unsafe).
  all_cond_values_fail(0xe52dd004,  // push {sp}
                       kDefaultBaseAddr,
                       "push {sp} (A2 a8-248) should be unpredictable");
}

TEST_F(ValidatorTests, CheckPushPcUnpredictable) {
  // Run test to verify that "Push {pc}", encoding A2 on a8-248 of ARM manual,
  // is unsafe.
  all_cond_values_fail(0xe52dF004,  // push {pc}
                       kDefaultBaseAddr,
                       "push {pc} (A2 A8-248) should be unpredictable");
}

// Implementation of the ValidatorTests utility methods.  These are documented
// toward the top of this file.

ValidatorTests::ValidatorTests()
  : _validator(kBytesPerBundle,
               kCodeRegionSize,
               kDataRegionSize,
               kAbiReadOnlyRegisters,
               kAbiDataAddrRegisters) {}

bool ValidatorTests::validate(const arm_inst *pattern,
                              size_t inst_count,
                              uint32_t start_addr,
                              ProblemSink *sink) {
  // We think in instructions; CodeSegment thinks in bytes.
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(pattern);
  CodeSegment segment(bytes, start_addr, inst_count * sizeof(arm_inst));

  vector<CodeSegment> segments;
  segments.push_back(segment);

  return _validator.validate(segments, sink);
}

void ValidatorTests::validation_should_pass(const arm_inst *pattern,
                                            size_t inst_count,
                                            uint32_t base_addr,
                                            const string &msg) {
  ProblemSpy spy;
  bool validation_result = validate(pattern, inst_count, base_addr, &spy);

  ASSERT_TRUE(validation_result) << msg << " should pass at " << base_addr;
  vector<ProblemRecord> &problems = spy.get_problems();
  EXPECT_EQ(0U, problems.size()) << msg
      << " should have no problems when located at " << base_addr;
}

void ValidatorTests::validation_should_pass2(const arm_inst *pattern,
                                             size_t inst_count,
                                             uint32_t base_addr,
                                             const string &msg) {
  // A couple sanity checks for correct usage.
  ASSERT_EQ(2U, inst_count)
      << "This routine only supports 2-instruction patterns.";
  ASSERT_TRUE(
      _validator.bundle_for_address(base_addr).begin_addr() == base_addr)
      << "base_addr parameter must be bundle-aligned";

  uint32_t last_addr = base_addr + (kBytesPerBundle - 4);

  // Try the legitimate (non-overlapping) variations:
  for (uint32_t addr = base_addr; addr < last_addr; addr += 4) {
    validation_should_pass(pattern, inst_count, addr, msg);
  }

  // Make sure it fails over bundle boundaries.
  ProblemSpy spy;
  bool overlap_result = validate(pattern, inst_count, last_addr, &spy);
  EXPECT_FALSE(overlap_result)
      << msg << " should fail at overlapping address " << last_addr;
  vector<ProblemRecord> &problems = spy.get_problems();
  ASSERT_EQ(1U, problems.size())
      << msg << " should have 1 problem at overlapping address " << last_addr;

  ProblemRecord first = problems[0];
  if (!spy.IsPairProblemAtFirst(first)) {
    last_addr += 4;
  }
  EXPECT_EQ(last_addr, first.vaddr())
      << "Problem in valid but mis-aligned pseudo-instruction ("
      << msg
      << ") must be reported at end of bundle";
  EXPECT_NE(nacl_arm_val::kReportProblemSafety, first.method())
      << "Just crossing a bundle should not make a safe instruction unsafe: "
      << msg;
  EXPECT_EQ(nacl_arm_dec::MAY_BE_SAFE, spy.GetSafetyLevel(first))
      << "Just crossing a bundle should not make a safe instruction unsafe: "
      << msg;
  // Be sure that we get one of the crosses bundle error messages.
  if (nacl_arm_val::kProblemPatternCrossesBundle != first.problem()) {
    EXPECT_EQ(nacl_arm_val::kPairCrossesBundle, spy.GetPairProblem(first));
  }
}

vector<ProblemRecord> ValidatorTests::validation_should_fail(
    const arm_inst *pattern,
    size_t inst_count,
    uint32_t base_addr,
    const string &msg) {
  // TODO(cbiffle): test at various overlapping and non-overlapping addresses,
  // like above.  Not that this is a spectacularly likely failure case, but
  // it's worth exercising.
  ProblemSpy spy;
  bool validation_result = validate(pattern, inst_count, base_addr, &spy);
  EXPECT_FALSE(validation_result) << "Expected to fail: " << msg;

  vector<ProblemRecord> problems = spy.get_problems();
  // Would use ASSERT here, but cannot ASSERT in non-void functions :-(
  EXPECT_NE(0U, problems.size())
      << "Must report validation problems: " << msg;

  // The rest of the checking is done in the caller.
  return problems;
}

void ValidatorTests::all_cond_values_pass(const arm_inst prototype,
                                          uint32_t base_addr,
                                          const string &message) {
  ProblemSpy spy;
  arm_inst test_inst = prototype;
  for (int cond = Instruction::EQ; cond < Instruction::UNCONDITIONAL; ++cond) {
    test_inst = ChangeCond(test_inst,
                           static_cast<Instruction::Condition>(cond));
    EXPECT_TRUE(validate(&test_inst, 1, base_addr, &spy))
        << "Fails on cond " << cond << ": " << message;
  }
}

void ValidatorTests::all_cond_values_fail(const arm_inst prototype,
                                          uint32_t base_addr,
                                          const string &message) {
  ProblemSpy spy;
  arm_inst test_inst = prototype;
  for (int cond = Instruction::EQ; cond < Instruction::UNCONDITIONAL; ++cond) {
    test_inst = ChangeCond(test_inst,
                           static_cast<Instruction::Condition>(cond));
    EXPECT_FALSE(validate(&test_inst, 1, base_addr, &spy))
        << "Passes on cond " << cond << ": " << message;
  }
}

};  // anonymous namespace

// Test driver function.
int main(int argc, char *argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
