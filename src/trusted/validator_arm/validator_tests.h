/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_VALIDATOR_TESTS_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_VALIDATOR_TESTS_H_

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
 *
 * Also see validator_tests.cc and validator_large_tests.cc
 */

#include <limits>
#include <string>
#include <sstream>
#include <vector>

#include "gtest/gtest.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability_bits.h"

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

namespace nacl_val_arm_test {

#ifdef __BIG_ENDIAN__
#error This test will only succeed on a little-endian machine.  Sorry.
#endif

// Since ARM instructions are always little-endian, on a little-endian machine
// we can represent them as ints.  This makes things somewhat easier to read
// below.
typedef uint32_t arm_inst;

// This is where untrusted code starts.  Most tests use this.
static const uint32_t kDefaultBaseAddr = 0x20000;

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
// Untrusted code must not write to the thread pointer.
static const RegisterList kAbiReadOnlyRegisters(Register::Tp());
// The stack pointer can be used for "free" loads and stores.
static const RegisterList kAbiDataAddrRegisters(nacl_arm_dec::Register::Sp());

// Defines a buffer for error messages.
static const size_t kBufferSize = 256;
typedef char ErrorMessage[kBufferSize];

// Simply records the arguments given to ReportProblem, below.
class ProblemRecord {
 public:
  ProblemRecord(uint32_t vaddr,
                ValidatorProblem problem,
                ValidatorProblemMethod method,
                ValidatorProblemUserData user_data)
      : vaddr_(vaddr), problem_(problem), method_(method) {
    Init(user_data);
  }

  ProblemRecord(const ProblemRecord& r)
      : vaddr_(r.vaddr_), problem_(r.problem_), method_(r.method_) {
    Init(r.user_data_);
  }

  ~ProblemRecord() {}

  ProblemRecord& operator=(const ProblemRecord& r) {
    vaddr_ = r.vaddr_;
    problem_ = r.problem_;
    method_ = r.method_;
    Init(r.user_data_);
    return *this;
  }

  uint32_t vaddr() const { return vaddr_; }
  ValidatorProblem problem() const { return problem_; }
  ValidatorProblemMethod method() const { return method_; }
  const uint32_t* user_data() const { return user_data_; }

 private:
  void Init(const ValidatorProblemUserData user_data);

  uint32_t vaddr_;
  ValidatorProblem problem_;
  ValidatorProblemMethod method_;
  ValidatorProblemUserData user_data_;
};

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

// Coordinates the fixture objects used by test cases below.
class ValidatorTests : public ::testing::Test {
 public:
  // Utility method to decode an instruction.
  const nacl_arm_dec::ClassDecoder& decode(
      nacl_arm_dec::Instruction inst) const;

  // Returns the given instruction, after modifying the instruction condition
  // to the given value.
  static arm_inst ChangeCond(arm_inst inst, Instruction::Condition c) {
    return (inst & 0x0fffffff) | (static_cast<arm_inst>(c) << 28);
  }

 protected:
  ValidatorTests()
      : _validator(kBytesPerBundle,
                   kCodeRegionSize,
                   kDataRegionSize,
                   kAbiReadOnlyRegisters,
                   kAbiDataAddrRegisters) {}

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

  // Returns the given instruction, after modifying its S bit (bit 20) to
  // the given value.
  static arm_inst SetSBit(arm_inst inst, bool s) {
    return (inst & 0xffefffff) | (static_cast<arm_inst>(s) << 20);
  }

  SfiValidator _validator;
};

}  // namespace nacl_val_arm_test

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_VALIDATOR_TESTS_H_
