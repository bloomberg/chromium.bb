/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/validator_arm/validator_tests.h"

namespace nacl_val_arm_test {

void ProblemRecord::Init(const ValidatorProblemUserData user_data) {
  for (size_t i = 0; i < nacl_arm_val::kValidatorProblemUserDataSize; ++i) {
    user_data_[i] = user_data[i];
  }
}


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

ValidatorTests::ValidatorTests()
    : _validator(NULL),
      _is_valid_single_instruction_destination_register() {
  _is_valid_single_instruction_destination_register.
      Add(Register::Tp()).Add(Register::Sp()).Add(Register::Pc());
}

const nacl_arm_dec::ClassDecoder& ValidatorTests::decode(
    nacl_arm_dec::Instruction inst) const {
  return _validator->decode(inst);
}

bool ValidatorTests::validate(const arm_inst *pattern,
                              size_t inst_count,
                              uint32_t start_addr,
                              ProblemSink *sink) {
  // We think in instructions; CodeSegment thinks in bytes.
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(pattern);
  CodeSegment segment(bytes, start_addr, inst_count * sizeof(arm_inst));

  vector<CodeSegment> segments;
  segments.push_back(segment);

  return _validator->validate(segments, sink);
}

void ValidatorTests::validation_should_pass(const arm_inst *pattern,
                                            size_t inst_count,
                                            uint32_t base_addr,
                                            const string &msg) {
  ProblemSpy spy;
  bool validation_result = validate(pattern, inst_count, base_addr, &spy);

  std::string errors;
  vector<ProblemRecord> &problems = spy.get_problems();
  for (vector<ProblemRecord>::const_iterator it = problems.begin();
       it != problems.end();
       ++it) {
    ErrorMessage message;
    spy.GetErrorMessage(message, *it);
    if (it != problems.begin()) {
      errors += "\n";
    }
    errors += std::string("\t") + message;
  }

  ASSERT_TRUE(validation_result) << msg << " should pass at " << base_addr <<
      ":\n" << errors;
}

void ValidatorTests::validation_should_pass2(const arm_inst *pattern,
                                             size_t inst_count,
                                             uint32_t base_addr,
                                             const string &msg) {
  // A couple sanity checks for correct usage.
  ASSERT_EQ(2U, inst_count)
      << "This routine only supports 2-instruction patterns.";
  ASSERT_TRUE(
      _validator->bundle_for_address(base_addr).begin_addr() == base_addr)
      << "base_addr parameter must be bundle-aligned";

  // Try error case where second instruction occurs as first instruction.
  arm_inst second_as_program[] = {
    pattern[1]
  };
  ostringstream bad_first_message;
  bad_first_message << msg << ": without first instruction";
  validation_should_fail(second_as_program,
                         NACL_ARRAY_SIZE(second_as_program),
                         base_addr,
                         bad_first_message.str());

  // Try the legitimate (non-overlapping) variations:
  uint32_t last_addr = base_addr + (kBytesPerBundle - 4);
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
                                          const string &msg) {
  ProblemSpy spy;
  arm_inst test_inst = prototype;
  for (Instruction::Condition cond = Instruction::EQ;
       cond < Instruction::UNCONDITIONAL;
       cond = Instruction::Next(cond)) {
    test_inst = ChangeCond(test_inst, cond);
    EXPECT_TRUE(validate(&test_inst, 1, base_addr, &spy))
        << "Fails on cond " << Instruction::ToString(cond) << ": " << msg;
  }
}

void ValidatorTests::all_cond_values_fail(const arm_inst prototype,
                                          uint32_t base_addr,
                                          const string &msg) {
  ProblemSpy spy;
  arm_inst test_inst = prototype;
  for (Instruction::Condition cond = Instruction::EQ;
       cond < Instruction::UNCONDITIONAL;
       cond = Instruction::Next(cond)) {
    test_inst = ChangeCond(test_inst, cond);
    EXPECT_FALSE(validate(&test_inst, 1, base_addr, &spy))
      << std::hex << test_inst
        << ": Passes on cond " << Instruction::ToString(cond) << ": " << msg;
  }
}

}  // namespace nacl_val_arm_test
