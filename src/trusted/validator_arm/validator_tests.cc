/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

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
 * ARMv7-A ARM (available online).  Remember that ARM instructions are always
 * little-endian, and this is made explicit in these tests to ensure they pass
 * on both big- and little-endian machines.
 */

#include <vector>

#include "gtest/gtest.h"
#include "native_client/src/include/nacl_macros.h"

#include "native_client/src/trusted/validator_arm/validator.h"

using std::vector;

using nacl_arm_dec::Register;
using nacl_arm_dec::RegisterList;
using nacl_arm_dec::Instruction;

using nacl_arm_val::SfiValidator;
using nacl_arm_val::ProblemSink;
using nacl_arm_val::CodeSegment;

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
static const RegisterList kAbiReadOnlyRegisters = Register(9);
// The stack pointer can be used for "free" loads and stores.
static const RegisterList kAbiDataAddrRegisters = nacl_arm_dec::kRegisterStack;


/*
 * Support code
 */

// Simply records the arguments given to report_problem, below.
struct ProblemRecord {
  uint32_t vaddr;
  nacl_arm_dec::SafetyLevel safety;
  nacl::string problem_code;
  uint32_t ref_vaddr;
};

// A ProblemSink that records all calls (implementation of the Spy pattern)
class ProblemSpy : public ProblemSink {
 public:
  virtual void report_problem(uint32_t vaddr, nacl_arm_dec::SafetyLevel safety,
      const nacl::string &problem_code, uint32_t ref_vaddr = 0) {
    _problems.push_back(
        (ProblemRecord) { vaddr, safety, problem_code, ref_vaddr });
  }

  /*
   * We want *all* the errors that the validator produces.  Note that this means
   * we're not testing the should_continue functionality.  This is probably
   * okay.
   */
  virtual bool should_continue() { return true; }

  vector<ProblemRecord> &get_problems() { return _problems; }

 private:
  vector<ProblemRecord> _problems;
};

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

  /*
   * Tests a pattern that's expected to pass, at each possible bundle alignment.
   * This also tries the pattern across bundle boundaries, and makes sure it
   * fails.
   * Note that this only works with 8-byte patterns, but that's all we have
   * at the moment.
   */
  void validation_should_pass(const arm_inst *pattern,
                              size_t inst_count,
                              uint32_t base_addr,
                              const char * const msg);

  /*
   * Tests a pattern that is forbidden in the SFI model.
   *
   * Note that the 'msg1' and 'msg2' parameters are merely concatentated in the
   * output.
   */
  vector<ProblemRecord> validation_should_fail(const arm_inst *pattern,
                                               size_t inst_count,
                                               uint32_t base_addr,
                                               const char * const msg1,
                                               const char * const msg2);

  SfiValidator _validator;
};


/*
 * Primitive tests checking various constructor properties.  Any of these
 * failing would be a very bad sign indeed.
 */

TEST_F(ValidatorTests, RecognizesDataAddressRegisters) {
  /*
   * Note that the logic below needs to be kept in sync with the definition
   * of kAbiDataAddrRegisters at the top of this file.
   *
   * This test is pretty trivial -- we can exercise the data_address_register
   * functionality more deeply with pattern tests below.
   */
  for (int i = 0; i < 16; i++) {
    Register r(i);
    if (r == nacl_arm_dec::kRegisterStack) {
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

/*
 * Code validation tests
 */

// This is where untrusted code starts.  Most tests use this.
static const uint32_t kDefaultBaseAddr = 0x20000;

struct GoodPattern {
  const char *about;
  arm_inst instructions[2];
};

TEST_F(ValidatorTests, SimpleSafeMaskPatterns) {
  static const GoodPattern patterns[] = {
    { "Simple unconditional store with in-place mask",
      { 0xE3C11103,  // bic r1, r1, #0xC0000000
        0xE5810000,  // str r0, [r1]
      }},
    { "Simple unconditional store with implicit address move",
      { 0xE3C31103,  // bic r1, r3, #0xC0000000
        0xE5810000,  // str r0, [r1]
      }},
    { "Conditional store with in-place mask",
      { 0x03C11103,  // biceq r1, r1, #0xC0000000
        0x05810000,  // streq r0, [r1]
      }},
  };

  for (unsigned i = 0; i < NACL_ARRAY_SIZE(patterns); i++) {
    validation_should_pass(patterns[i].instructions,
                           2,
                           kDefaultBaseAddr,
                           patterns[i].about);
  }
}

/*
 * Describes an invalid mask instruction for the test below.  Fun fact: this
 * can't be anonymous in the function below, because NACL_ARRAY_SIZE breaks.
 */
struct BadMask {
  arm_inst instruction;
  const char * about;
};

TEST_F(ValidatorTests, RejectsInvalidMaskInstructionBeforeStore) {
  static const BadMask invalid_masks[] = {
    // This writes the address register, but doesn't mask it.
    { 0xE1A01003, "mov r1, r3" },
    // This "masks" the register, but doesn't clear any bits.
    { 0xE3C31000, "bic r1, r3, #0" },
    // This "masks" the register, but doesn't clear the right bits.
    { 0xE3C31102, "bic r1, r3, #0x80000000" },
    // This mask is correct, but has a different predicate from the store.
    { 0x03C31103, "biceq r1, r3, #0xC0000000" },
  };

  arm_inst fragment[] = {
    0x00000000,  // room for the bad mask to be copied in.
    0xE5810000,  // str r0, [r1]
  };

  for (unsigned i = 0; i < NACL_ARRAY_SIZE(invalid_masks); i++) {
    fragment[0] = invalid_masks[i].instruction;

    vector<ProblemRecord> problems =
        validation_should_fail(fragment,
                               NACL_ARRAY_SIZE(fragment),
                               kDefaultBaseAddr,
                               "Store after a non-mask instruction must fail: ",
                               invalid_masks[i].about);

    // Explicitly continue rather than ASSERT so that we run the other cases.
    EXPECT_EQ(1U, problems.size());
    if (problems.size() != 1) continue;

    ProblemRecord first = problems[0];
    EXPECT_EQ(kDefaultBaseAddr + 4, first.vaddr)
        << "For bad mask after store, the problem must indicate the store: "
        << invalid_masks[i].about;
    EXPECT_EQ(nacl_arm_dec::MAY_BE_SAFE, first.safety)
        << "The store itself should decode as safe when used with bad mask: "
        << invalid_masks[i].about;
    EXPECT_EQ(nacl::string(nacl_arm_val::kProblemUnsafeStore),
              first.problem_code)
        << "Problem code should complain about the store; bad mask: "
        << invalid_masks[i].about;
  }
}

/*
 * Implementation of the ValidatorTests utility methods.  These are documented
 * toward the top of this file.
 */
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
                                            const char * const msg) {
  // A couple sanity checks for correct usage.
  ASSERT_EQ(2U, inst_count)
      << "This routine only supports 2-instruction patterns.";
  ASSERT_TRUE(
      _validator.bundle_for_address(base_addr).begin_addr() == base_addr)
      << "base_addr parameter must be bundle-aligned";

  uint32_t last_addr = base_addr + (kBytesPerBundle - 4);

  // Try the legitimate (non-overlapping) variations:
  for (uint32_t addr = base_addr; addr < last_addr; addr += 4) {
    ProblemSpy spy;
    bool validation_result = validate(pattern, inst_count, addr, &spy);

    // Blow all the way out on failure -- otherwise we get an error cascade.
    ASSERT_TRUE(validation_result) << msg << " should pass at " << addr;
    vector<ProblemRecord> &problems = spy.get_problems();
    EXPECT_EQ(0U, problems.size()) << msg
        << " should have no problems when located at " << addr;
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
  EXPECT_EQ(last_addr, first.vaddr)
      << "Problem in valid pseudo-instruction ("
      << msg
      << ") must be reported at end of bundle";
  EXPECT_EQ(nacl_arm_dec::MAY_BE_SAFE, first.safety)
      << "Just crossing a bundle should not make a safe instruction unsafe: "
      << msg;
  EXPECT_EQ(nacl::string(nacl_arm_val::kProblemPatternCrossesBundle),
            first.problem_code);
}

vector<ProblemRecord> ValidatorTests::validation_should_fail(
    const arm_inst *pattern,
    size_t inst_count,
    uint32_t base_addr,
    const char * const msg1,
    const char * const msg2) {
  /*
   * TODO(cbiffle): test at various overlapping and non-overlapping addresses,
   * like above.  Not that this is a spectacularly likely failure case, but
   * it's worth exercising.
   */
  ProblemSpy spy;
  bool validation_result = validate(pattern, inst_count, base_addr, &spy);
  EXPECT_FALSE(validation_result) << "Must fail: " << msg1 << msg2;

  vector<ProblemRecord> problems = spy.get_problems();
  // Would use ASSERT here, but cannot ASSERT in non-void functions :-(
  EXPECT_NE(0U, problems.size())
      << "Must report validation problems: " << msg1 << msg2;

  // The rest of the checking is done in the caller.
  return problems;
}

};  // anonymous namespace

// Test driver function.
int main(int argc, char *argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
