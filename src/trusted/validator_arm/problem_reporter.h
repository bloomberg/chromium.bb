/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SOURCE_TRUSTED_VALIDATOR_ARM_PROBLEM_REPORTER_H_
#define NATIVE_CLIENT_SOURCE_TRUSTED_VALIDATOR_ARM_PROBLEM_REPORTER_H_

// Problem reporter utility that converts reported problems into C
// strings.

#include "native_client/src/trusted/validator_arm/validator.h"

namespace nacl_arm_val {

// ProblemSink that converts (internal) user data back to high-level
// data, and converts reported problems into human readable text.
class ProblemReporter : public ProblemSink {
 public:
  inline ProblemReporter() {}
  virtual ~ProblemReporter() {}

 protected:
  // Extracts safety parameter from user data. Assumes that
  // corresponding method is kReportProblemSafety.
  void ExtractProblemSafety(
      const ValidatorProblemUserData user_data,
      nacl_arm_dec::SafetyLevel* safety);

  // Extracts out problem address parameter from user data. Assumes
  // that corrresponding method is kReportProblemAddress.
  void ExtractProblemAddress(
      const ValidatorProblemUserData user_data,
      uint32_t* problem_vaddr);

  // Extracts out problem instruction pair parameters from user data.
  // Assumes corresponding method is kReportProblemInstructionPair.
  // Note: Use NULL for output arguments you are not interested in.
  void ExtractProblemInstructionPair(
      const ValidatorProblemUserData user_data,
      ValidatorInstructionPairProblem* pair_problem,
      uint32_t* first_address,
      nacl_arm_dec::Instruction* first,
      uint32_t* second_address,
      nacl_arm_dec::Instruction* second);

  // Extracts out problem register parameter from user data.  Assumes
  // corresponding method is kReportProblemRegister.
  void ExtractProblemRegister(
      const ValidatorProblemUserData user_data,
      nacl_arm_dec::Register* reg);

  // Extracts out problem register and instruction pair parameters
  // from user data. Assumes corresponding method is
  // kReportProblemRegisterInstructionPair.  Note: Use NULL for output
  // arguments you are not interested in.
  void ExtractProblemRegisterInstructionPair(
      const ValidatorProblemUserData user_data,
      ValidatorInstructionPairProblem* pair_problem,
      nacl_arm_dec::Register* reg,
      uint32_t* first_address,
      nacl_arm_dec::Instruction* first,
      uint32_t* second_address,
      nacl_arm_dec::Instruction* second);

  // Extracts out the problem register list parameter from user data.
  // Assumes corresponding method is kReportProblemRegisterList.
  void ExtractProblemRegisterList(
      const ValidatorProblemUserData user_data,
      nacl_arm_dec::RegisterList* registers);

  // Extracts out the problem register list and instruction pair
  // parameters from user data. Assumes corresponding method is
  // ReportProblemRegisterListInstructionPair.  Note: Use NULL for
  // output arguments you are not interested in.
  void ExtractProblemRegisterListInstructionPair(
      const ValidatorProblemUserData user_data,
      ValidatorInstructionPairProblem* pair_problem,
      nacl_arm_dec::RegisterList* registers,
      uint32_t* first_address,
      nacl_arm_dec::Instruction* first,
      uint32_t* second_address,
      nacl_arm_dec::Instruction* second);

  // Converts the given error report into readable text.
  // Arguments are:
  //    buffer - The buffer to put the readable text in.
  //    buffer_size - The size of the array used to defined buffer.
  //    vaddr - the virtual address where the problem occurred.
  //    problem - The problem being reported.
  //    method - The reporting method used to generate user data.
  //    user_data - An array of additional information about the instruction.
  void ToText(char* buffer,
              size_t buffer_size,
              uint32_t vaddr,
              ValidatorProblem problem,
              ValidatorProblemMethod method,
              const ValidatorProblemUserData user_data);

 private:
  // Internal method to convert internal error report data
  // to corresponding readable text using the given format
  // string. See the definition of this method in problem_reporter.cc
  // to find a description of format directives that can appear in
  // the format string.
  // Aruments are:
  //    buffer - The buffer to put the readable text in.
  //    buffer_size - The size of the array used to defined buffer.
  //    vaddr - the virtual address where the problem occurred.
  //    problem - The problem being reported.
  //    method - The reporting method used to generate user data.
  //    user_data - An array of additional information about the instruction.
  void Render(char** buffer,
              size_t* buffer_size,
              const char* format,
              ValidatorProblem problem,
              ValidatorProblemMethod method,
              const ValidatorProblemUserData user_data);
};

}  // namespace

#endif  // NATIVE_CLIENT_SOURCE_TRUSTED_VALIDATOR_ARM_PROBLEM_REPORTER_H_
