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

// Defines the maximum number of data elements assocated with validator
// problem user data.
static const size_t kValidatorProblemUserDataSize = 6;

// Defines array used to hold user data associated with a problem.
typedef uint32_t ValidatorProblemUserData[kValidatorProblemUserDataSize];

// Defines Which ReportProblem... function was called to generate
// user data associated with the problem.
typedef enum {
  kReportProblemSafety,
  kReportProblem,
  kReportProblemAddress,
  kReportProblemInstructionPair,
  kReportProblemRegister,
  kReportProblemRegisterInstructionPair,
  kReportProblemRegisterList,
  kReportProblemRegisterListInstructionPair
} ValidatorProblemMethod;

// ProblemSink that converts the (internal) user data into a string
// error message, which is then processed using the (derived) method
// ReportProblemMessage.
class ProblemReporter : public ProblemSink {
 public:
  ProblemReporter();
  virtual ~ProblemReporter() {}

  // The following override inherited virtuals.
  virtual void ReportProblemDiagnostic(nacl_arm_dec::Violation violation,
                                       uint32_t vaddr,
                                       const char* format, ...)
               // Note: format is the 4th argument because of implicit this.
               ATTRIBUTE_FORMAT_PRINTF(4, 5);

  virtual void ReportProblemSafety(nacl_arm_dec::Violation violation,
                                   const DecodedInstruction& inst);

  virtual void ReportProblem(uint32_t vaddr, ValidatorProblem problem);

  virtual void ReportProblemAddress(uint32_t vaddr, ValidatorProblem problem,
                                    uint32_t problem_vaddr);

  virtual void ReportProblemInstructionPair(
      uint32_t vaddr, ValidatorProblem problem,
      ValidatorInstructionPairProblem pair_problem,
      const DecodedInstruction& first, const DecodedInstruction& second);

  virtual void ReportProblemRegister(uint32_t vaddr, ValidatorProblem problem,
                                     nacl_arm_dec::Register reg);

  virtual void ReportProblemRegisterInstructionPair(
      uint32_t vaddr, ValidatorProblem problem,
      ValidatorInstructionPairProblem pair_problem,
      nacl_arm_dec::Register reg,
      const DecodedInstruction& first, const DecodedInstruction& second);

  virtual void ReportProblemRegisterList(
      uint32_t vaddr, ValidatorProblem problem,
      nacl_arm_dec::RegisterList registers);

  virtual void ReportProblemRegisterListInstructionPair(
      uint32_t vaddr, ValidatorProblem problem,
      ValidatorInstructionPairProblem pair_problem,
      nacl_arm_dec::RegisterList registers,
      const DecodedInstruction& first, const DecodedInstruction& second);

 protected:
  // Virtual called once the diagnostic message of ReportProblemDiagnostic,
  // or ReportProblemInternal, has been generated.
  virtual void ReportProblemMessage(nacl_arm_dec::Violation violation,
                                    uint32_t vaddr,
                                    const char* message) = 0;

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

  // Same but without virtual address.
  void ToText(char* buffer,
              size_t buffer_size,
              ValidatorProblem problem,
              ValidatorProblemMethod method,
              const ValidatorProblemUserData user_data);

  // Reports a problem in untrusted code. All public report problem functions
  // are automatically converted to a call to this function.
  //  vaddr - the virtual address where the problem occurred.  Note that this is
  //      probably not the address of memory that contains the offending
  //      instruction, since we allow CodeSegments to lie about their base
  //      addresses.
  //  problem - An enumerated type defining the type of problem found.
  //  method - The reporting method used to generate user data.
  //  user_data - An array of additional information about the instruction.
  //
  // Default implementation does nothing!
  virtual void ReportProblemInternal(uint32_t vaddr,
                                     ValidatorProblem problem,
                                     ValidatorProblemMethod method,
                                     ValidatorProblemUserData user_data);

  // Holds the values passed to the last call to ReportProblemInternal.
  uint32_t last_vaddr;
  ValidatorProblem last_problem;
  ValidatorProblemMethod last_method;
  ValidatorProblemUserData last_user_data;

  // Returns the number of elements in user_data, for the given method.
  // Used so that we can blindly copy/compare user data.
  static size_t UserDataSize(ValidatorProblemMethod method);

 private:
  // Define a buffer to generate error messages into.
  static const size_t kBufferSize = 256;
  char buffer[kBufferSize];

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

// Returns a printable name for the given register.
const char* RegisterName(const nacl_arm_dec::Register& r);

}  // namespace

#endif  // NATIVE_CLIENT_SOURCE_TRUSTED_VALIDATOR_ARM_PROBLEM_REPORTER_H_
