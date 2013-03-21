/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/validator_arm/problem_reporter.h"

#include <assert.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "native_client/src/include/portability_io.h"

namespace nacl_arm_val {

// Holds the number of data elements associated with each method.
static const size_t kUserDataSize[] = {
  1,  // kReportProblemSafety,
  0,  // kReportProblem,
  1,  // kReportProblemAddress,
  5,  // kReportProblemInstructionPair,
  1,  // kReportProblemRegister,
  6,  // kReportProblemRegisterInstructionPair,
  1,  // kReportProblemRegisterList,
  6,  // kReportProblemRegisterListInstructionPair
};

ProblemReporter::ProblemReporter()
    : last_vaddr(0),
      last_problem(kProblemOtherDiagnosticMessage),
      last_method(kReportProblem) {
  for (size_t i = 0; i < kValidatorProblemUserDataSize; ++i) {
    last_user_data[i] = 0;
  }
}

void ProblemReporter::ReportProblemDiagnostic(nacl_arm_dec::Violation violation,
                                              uint32_t vaddr,
                                              const char* format, ...) {
  // Start by adding address of diagnostic.
  va_list args;
  va_start(args, format);
  VSNPRINTF(buffer, kBufferSize, format, args);
  va_end(args);
  ReportProblemMessage(violation, vaddr, buffer);
}

size_t ProblemReporter::UserDataSize(ValidatorProblemMethod method) {
  size_t index = static_cast<size_t>(method);
  return (index < NACL_ARRAY_SIZE(kUserDataSize)) ? kUserDataSize[index] : 0;
}

void ProblemReporter::ReportProblemInternal(
    uint32_t vaddr,
    ValidatorProblem problem,
    ValidatorProblemMethod method,
    ValidatorProblemUserData user_data) {
  last_vaddr = vaddr;
  last_problem = problem;
  last_method = method;

  size_t data_size = UserDataSize(method);
  for (size_t i = 0; i < data_size; ++i) {
    last_user_data[i] = user_data[i];
  }

  // Before returning, be sure unused fields in user data are set to zero.
  // This way, we don't need to fill in each ReportProblem... method.
  for (size_t i = data_size; i < kValidatorProblemUserDataSize; ++i) {
    last_user_data[i] = 0;
  }

  // Convert to text and communicate generated message.
  ToText(buffer, kBufferSize, problem, method, user_data);
  // TODO(kschimpf) We really should be adding a more specific violation,
  // but requires more plumbing changes than currently can be handled.
  ReportProblemMessage(nacl_arm_dec::OTHER_VIOLATION, vaddr, buffer);
}

void ProblemReporter::ReportProblemSafety(nacl_arm_dec::Violation violation,
                                          const DecodedInstruction& inst) {
  uint32_t addr = inst.addr();
  switch (static_cast<int>(violation)) {
    case nacl_arm_dec::UNINITIALIZED_VIOLATION:
    default:
      ReportProblemDiagnostic(
          violation, addr,
          "Unknown error occurred decoding this instruction.");
      break;
    case nacl_arm_dec::UNKNOWN_VIOLATION:
      ReportProblemDiagnostic(
          violation, addr,
          "The value assigned to registers by this instruction is unknown.");
      break;
    case nacl_arm_dec::UNDEFINED_VIOLATION:
      ReportProblemDiagnostic(
          violation, addr,
          "Instruction is undefined according to the ARMv7"
          " ISA specifications.");
      break;
    case nacl_arm_dec::NOT_IMPLEMENTED_VIOLATION:
      // This instruction is not recognized by the decoder functions.
      ReportProblemDiagnostic(
          violation, addr,
          "Instruction not understood by Native Client.");
      break;
    case nacl_arm_dec::UNPREDICTABLE_VIOLATION:
      ReportProblemDiagnostic(
          violation, addr,
          "Instruction has unpredictable effects at runtime.");
      break;
    case nacl_arm_dec::DEPRECATED_VIOLATION:
      ReportProblemDiagnostic(
          violation, addr,
          "Instruction is deprecated in ARMv7.");
      break;
    case nacl_arm_dec::FORBIDDEN_VIOLATION:
      ReportProblemDiagnostic(
          violation, addr,
          "Instruction not allowed by Native Client.");
      break;
    case nacl_arm_dec::FORBIDDEN_OPERANDS_VIOLATION:
      ReportProblemDiagnostic(
          violation, addr,
          "Instruction has operand(s) forbidden by Native Client.");
      break;
    case nacl_arm_dec::DECODER_ERROR_VIOLATION:
      ReportProblemDiagnostic(
          violation, addr,
          "Instruction decoded incorrectly by NativeClient.");
      break;
  };
}

void ProblemReporter::ReportProblem(uint32_t vaddr, ValidatorProblem problem) {
  ValidatorProblemUserData user_data = {
  };
  NACL_COMPILE_TIME_ASSERT(NACL_ARRAY_SIZE(user_data) <=
                           kValidatorProblemUserDataSize);
  ReportProblemInternal(vaddr, problem, kReportProblem, user_data);
}

void ProblemReporter::ReportProblemAddress(
    uint32_t vaddr, ValidatorProblem problem, uint32_t problem_vaddr) {
  ValidatorProblemUserData user_data = {
    problem_vaddr
  };
  NACL_COMPILE_TIME_ASSERT(NACL_ARRAY_SIZE(user_data) <=
                           kValidatorProblemUserDataSize);
  ReportProblemInternal(vaddr, problem, kReportProblemAddress, user_data);
}

void ProblemReporter::ReportProblemInstructionPair(
    uint32_t vaddr, ValidatorProblem problem,
    ValidatorInstructionPairProblem pair_problem,
    const DecodedInstruction& first, const DecodedInstruction& second) {
  ValidatorProblemUserData user_data = {
    static_cast<uint32_t>(pair_problem),
    first.addr(),
    first.inst().Bits(),
    second.addr(),
    second.inst().Bits()
  };
  NACL_COMPILE_TIME_ASSERT(NACL_ARRAY_SIZE(user_data) <=
                           kValidatorProblemUserDataSize);
  ReportProblemInternal(vaddr, problem,
                        kReportProblemInstructionPair, user_data);
}

void ProblemReporter::ReportProblemRegister(
    uint32_t vaddr, ValidatorProblem problem,
    nacl_arm_dec::Register reg) {
  ValidatorProblemUserData user_data = {
    reg.number()
  };
  NACL_COMPILE_TIME_ASSERT(NACL_ARRAY_SIZE(user_data) <=
                           kValidatorProblemUserDataSize);
  ReportProblemInternal(vaddr, problem, kReportProblemRegister, user_data);
}

void ProblemReporter::ReportProblemRegisterInstructionPair(
    uint32_t vaddr, ValidatorProblem problem,
    ValidatorInstructionPairProblem pair_problem,
    nacl_arm_dec::Register reg,
    const DecodedInstruction& first, const DecodedInstruction& second) {
  ValidatorProblemUserData user_data = {
    static_cast<uint32_t>(pair_problem),
    reg.number(),
    first.addr(),
    first.inst().Bits(),
    second.addr(),
    second.inst().Bits()
  };
  NACL_COMPILE_TIME_ASSERT(NACL_ARRAY_SIZE(user_data) <=
                           kValidatorProblemUserDataSize);
  ReportProblemInternal(vaddr, problem,
                        kReportProblemRegisterInstructionPair, user_data);
}

void ProblemReporter::ReportProblemRegisterList(
    uint32_t vaddr, ValidatorProblem problem,
    nacl_arm_dec::RegisterList registers) {
  ValidatorProblemUserData user_data = {
    registers.bits()
  };
  NACL_COMPILE_TIME_ASSERT(NACL_ARRAY_SIZE(user_data) <=
                           kValidatorProblemUserDataSize);
  ReportProblemInternal(vaddr, problem,
                        kReportProblemRegisterList, user_data);
}

void ProblemReporter::ReportProblemRegisterListInstructionPair(
    uint32_t vaddr, ValidatorProblem problem,
    ValidatorInstructionPairProblem pair_problem,
    nacl_arm_dec::RegisterList registers,
    const DecodedInstruction& first, const DecodedInstruction& second) {
  ValidatorProblemUserData user_data = {
    static_cast<uint32_t>(pair_problem),
    registers.bits(),
    first.addr(),
    first.inst().Bits(),
    second.addr(),
    second.inst().Bits()
  };
  NACL_COMPILE_TIME_ASSERT(NACL_ARRAY_SIZE(user_data) <=
                           kValidatorProblemUserDataSize);
  ReportProblemInternal(vaddr, problem,
                        kReportProblemRegisterListInstructionPair, user_data);
}

void ProblemReporter::ExtractProblemAddress(
    const ValidatorProblemUserData user_data,
    uint32_t* problem_vaddr) {
  if (problem_vaddr != NULL)
    *problem_vaddr = user_data[0];
}

// Given the internalized (integer) representation of
// ValidatorInstructionPairProblem, convert it back.
static ValidatorInstructionPairProblem Int2InstPairProblem(uint32_t index) {
  static ValidatorInstructionPairProblem Int2InstPairProblemMap[] = {
    kNoSpecificPairProblem,
    kFirstSetsConditionFlags,
    kConditionsOnPairNotSafe,
    kEqConditionalOn,
    kTstMemDisallowed,
    kPairCrossesBundle,
  };
  return (index < NACL_ARRAY_SIZE(Int2InstPairProblemMap))
      ? Int2InstPairProblemMap[index] : kNoSpecificPairProblem;
}

void ProblemReporter::ExtractProblemInstructionPair(
    const ValidatorProblemUserData user_data,
    ValidatorInstructionPairProblem* pair_problem,
    uint32_t* first_address,
    nacl_arm_dec::Instruction* first,
    uint32_t* second_address,
    nacl_arm_dec::Instruction* second) {
  if (pair_problem != NULL)
    *pair_problem = Int2InstPairProblem(user_data[0]);
  if (first_address != NULL)
    *first_address = user_data[1];
  if (first != NULL)
    first->Copy(nacl_arm_dec::Instruction(user_data[2]));
  if (second_address != NULL)
    *second_address = user_data[3];
  if (second != NULL)
    second->Copy(nacl_arm_dec::Instruction(user_data[4]));
}

// Converts the internal (integer) representation of a register
// (stored in user_data) back to the corresponding register.
static nacl_arm_dec::Register Int2Register(uint32_t index) {
  if (index < 16) {
    return nacl_arm_dec::Register(index);
  } else {
    return nacl_arm_dec::Register::None();
  }
}

void ProblemReporter::ExtractProblemRegister(
    const ValidatorProblemUserData user_data,
    nacl_arm_dec::Register* reg) {
  if (reg != NULL)
    reg->Copy(Int2Register(user_data[0]));
}

void ProblemReporter::ExtractProblemRegisterInstructionPair(
    const ValidatorProblemUserData user_data,
    ValidatorInstructionPairProblem* pair_problem,
    nacl_arm_dec::Register* reg,
    uint32_t* first_address,
    nacl_arm_dec::Instruction* first,
    uint32_t* second_address,
    nacl_arm_dec::Instruction* second) {
  if (pair_problem != NULL)
    *pair_problem = Int2InstPairProblem(user_data[0]);
  if (reg != NULL)
    reg->Copy(Int2Register(user_data[1]));
  if (first_address != NULL)
    *first_address = user_data[2];
  if (first != NULL)
    first->Copy(nacl_arm_dec::Instruction(user_data[3]));
  if (second_address != NULL)
    *second_address = user_data[4];
  if (second != NULL)
    second->Copy(nacl_arm_dec::Instruction(user_data[5]));
}

void ProblemReporter::ExtractProblemRegisterList(
    const ValidatorProblemUserData user_data,
    nacl_arm_dec::RegisterList* registers) {
  if (registers != NULL)
    registers->Copy(nacl_arm_dec::RegisterList(user_data[0]));
}

void ProblemReporter::ExtractProblemRegisterListInstructionPair(
    const ValidatorProblemUserData user_data,
    ValidatorInstructionPairProblem* pair_problem,
    nacl_arm_dec::RegisterList* registers,
    uint32_t* first_address,
    nacl_arm_dec::Instruction* first,
    uint32_t* second_address,
    nacl_arm_dec::Instruction* second) {
  if (pair_problem != NULL)
    *pair_problem = Int2InstPairProblem(user_data[0]);
  if (registers != NULL)
    registers->Copy(nacl_arm_dec::RegisterList(user_data[1]));
  if (first_address != NULL)
    *first_address = user_data[2];
  if (first != NULL)
    first->Copy(nacl_arm_dec::Instruction(user_data[3]));
  if (second_address != NULL)
    *second_address = user_data[4];
  if (second != NULL)
    second->Copy(nacl_arm_dec::Instruction(user_data[5]));
}

// Error message to print for each type of ValidatorProblem. See
// member function Render (below) for an explanation of $X directives.
// TODO(karl): Add information from the collected
// ValidatorInstructionPairProblem (for instruction pairs) to the
// error messages printed.
static const char* ValidatorProblemFormatDirective[kValidatorProblemSize] = {
  // kProblemUnsafe - An instruction is unsafe -- more information in
  // the SafetyLevel.
  "*** UNKNOWN PROBLEM FOUND ***",  // SHOULD NOT HAPPEN ANYMORE.
  // kProblemBranchSplitsPattern - A branch would break a
  // pseudo-operation pattern.
  "Instruction branches into middle of 2-instruction pattern at $a.",
  // kProblemBranchInvalidDest - A branch targets an invalid code
  // address (out of segment).
  "Instruction branches to invalid address $a.",
  // kProblemUnsafeLoadStore - An load/store uses an unsafe
  // (non-masked) base address.
  "Load/store base $r is not properly masked$R.",
  // kProblemIllegalPcLoadStore - A load/store on PC that doesn't
  // increment with a valid immediate address.
  "Native Client only allows updates on PC of the form 'PC + immediate'.",
  // kProblemUnsafeBranch - A branch uses an unsafe (non-masked)
  // destination address.
  "Destination branch on $r is not properly masked$R.",
  // kProblemUnsafeDataWrite - An instruction updates a data-address
  // register (e.g. SP) without masking.
  "Updating $r without masking in following instruction$R.",
  // kProblemReadOnlyRegister - An instruction updates a read-only
  // register (e.g. r9).
  "Updates read-only register(s): $r.",
  // kProblemPatternCrossesBundle - A pseudo-op pattern crosses a
  // bundle boundary.
  "Instruction pair$p crossses bundle boundary.",
  // kProblemMisalignedCall - A linking branch instruction is not in
  // the last bundle slot.
  "Call not last instruction in instruction bundle.",
  // kProblemConstructionFailed.
  "Construction of the SfiValidator failed because its arguments were invalid.",
  // kProblemIllegalUseOfThreadPointer.
  "Use of thread pointer $r not legal outside of load thread pointer "
  "instruction(s)",
};

// Error message to append to ValidatorProblemFormatDirective text (i.e. $R),
// if there is a corresponding instruction pair problem.
static const char* kValidatorInstructionPairProblem[] = {
  // kNoSpecificPairProblem - No specific known reason for instruction pair
  // failing.
  "",
  // kFirstSetsConditionFlags - First instruction sets conditions flags, and
  // hence, can't guarantee that the next instruction will always be executed.
  ", because instruction $F sets APSR condition flags",
  // kConditionsOnPairNotSafe - Conditions on instructions don't guarantee
  // that instructions will run atomically.
  ", because the conditions $c on $p don't guarantee atomicity",
  // kEqConditionalOn - Second is dependent on eq being set by first
  // instruction.
  ", because $S is not conditional on EQ",
  // kTstMemDisallowed - TST+LDR or TST+STR was used, but it's disallowed
  // on this CPU.
  ", because $p instruction pair is disallowed on this CPU",
  // kPairCrosssesBundle - Instruction pair crosses bundle boundary.
  ", because instruction pair$p crosses bundle boundary",
};

// Returns a printable name for the given register.
const char* RegisterName(const nacl_arm_dec::Register& r) {
  static const char* Name[16] = {
    "r0",
    "r1",
    "r2",
    "r3",
    "r4",
    "r5",
    "r6",
    "r7",
    "r8",
    "r9",
    "sl",
    "fp",
    "ip",
    "sp",
    "lr",
    "pc"
  };
  uint32_t n = r.number();
  return (n < NACL_ARRAY_SIZE(Name)) ? Name[n] : "r?";
}

// Advances the buffer count positions, to a maximum of buffer_size
// elements.  Updates both buffer and buffer_size to point the the new
// end of buffer.
static void RenderAdvance(
    int count,
    char** buffer,
    size_t* buffer_size) {
  if (count < 0) {
    // Try to fix end point, and then stop.
    if (*buffer_size > 0) {
      (*buffer)[0] = '\0';
    }
    *buffer_size = 0;
  } else {
    (*buffer) += count;
    (*buffer_size) -= count;
  }
}

// Adds a character to the end of the given buffer of the given buffer
// size.
static void RenderChar(char ch,
                       char** buffer,
                       size_t* buffer_size) {
  if (*buffer_size > 0) {
    RenderAdvance(SNPRINTF(*buffer, *buffer_size, "%c", ch),
                  buffer, buffer_size);
  }
}

// Adds a text string to the end of the given buffer of the given
// buffer size.
static void RenderText(const char* text,
                       char** buffer,
                       size_t* buffer_size) {
  if (*buffer_size > 0) {
    RenderAdvance(SNPRINTF(*buffer, *buffer_size, "%s", text),
                  buffer, buffer_size);
  }
}

// Adds text describing the given instruction address to the end of the
// given buffer of the given buffer size.
static void RenderInstAddress(uint32_t address,
                              char** buffer,
                              size_t* buffer_size) {
  if (*buffer_size > 0) {
    RenderAdvance(SNPRINTF(*buffer, *buffer_size, "%08"NACL_PRIx32, address),
                  buffer, buffer_size);
  }
}

// Adds text describing the two instruction addresses to the end of
// the given buffer of the given buffer size.
static void RenderInstAddressPair(uint32_t first_address,
                                  uint32_t second_address,
                                  char** buffer,
                                  size_t* buffer_size) {
  RenderText(" [", buffer, buffer_size);
  RenderInstAddress(first_address, buffer, buffer_size);
  RenderText(", ", buffer, buffer_size);
  RenderInstAddress(second_address, buffer, buffer_size);
  RenderText("]", buffer, buffer_size);
}

// Adds the conditions associated with the given two instructions, to
// the end of the given buffer of the given buffer size.
static void RenderInstCondPair(nacl_arm_dec::Instruction first_inst,
                               nacl_arm_dec::Instruction second_inst,
                               char** buffer,
                               size_t* buffer_size) {
  if (*buffer_size > 0) {
    RenderAdvance(
        SNPRINTF(*buffer, *buffer_size,
                 " (%s, %s)",
                 nacl_arm_dec::Instruction::ToString(
                     first_inst.GetCondition()),
                 nacl_arm_dec::Instruction::ToString(
                     second_inst.GetCondition())),
        buffer, buffer_size);
  }
}

// Adds text describing the given register to the end of the given buffer of
// the given size.
static void RenderRegister(nacl_arm_dec::Register reg,
                               char** buffer,
                               size_t* buffer_size) {
  RenderText(RegisterName(reg), buffer, buffer_size);
}

// Adds text describing the given register list, to the end of the given buffer
// of the given size.
// TODO(jfb) This assumes all problematic RegisterList are GPRs, not FPRs.
static void RenderRegisterList(nacl_arm_dec::RegisterList registers,
                               char** buffer,
                               size_t* buffer_size) {
  uint32_t regs_count = registers.numGPRs();
  if (regs_count == 0) {
    return;
  }
  bool is_first = true;
  for (uint32_t i = 0; i < 16; ++i) {
    nacl_arm_dec::Register r(i);
    if (!registers.Contains(r)) {
      continue;
    }
    if (is_first) {
      if (regs_count > 1) {
        RenderText("(", buffer, buffer_size);
      }
      is_first = false;
    } else {
      RenderText(", ", buffer, buffer_size);
    }
    RenderRegister(r, buffer, buffer_size);
  }
  if (regs_count > 1) {
    RenderText(")", buffer, buffer_size);
  }
}

// Renders a text string using the given format. Does the following
// substitutions:
//    $a - Print the address associated with the problem.
//    $p - Print the adresses of the two instructions being reported about.
//    $c - Print the conditions that aren't provably correct.
//    $r - Print the problem register/register list associated with the problem.
//    $F - Print the address of the first instruction.
//    $R - Print the reason (if known) the instruction pair was rejected.
//    $S - Print the address of the second instruction.
//    $$ - Print $.
void ProblemReporter::Render(char** buffer,
                              size_t* buffer_size,
                              const char* format,
                              ValidatorProblem problem,
                              ValidatorProblemMethod method,
                              const ValidatorProblemUserData user_data) {
  while (*format && (*buffer_size > 0)) {
    if (*format == '$') {
      format++;
      switch (*format) {
        case '\0':
          return;
        case 'a':  // Print instruction address associated with problem.
          if (method == kReportProblemAddress) {
            uint32_t address = 0;
            ExtractProblemAddress(user_data, &address);
            RenderInstAddress(address, buffer, buffer_size);
          } else {
            RenderText("unknown address", buffer, buffer_size);
          }
          break;
        case 'p':  // Print out instruction addresses associated with
                   // problem instruciton pair.
          if (method == kReportProblemInstructionPair) {
            uint32_t first_address;
            uint32_t second_address;
            ExtractProblemInstructionPair(
                user_data, NULL, &first_address, NULL, &second_address, NULL);
            RenderInstAddressPair(first_address, second_address,
                                  buffer, buffer_size);
          } else if (method == kReportProblemRegisterInstructionPair) {
            uint32_t first_address;
            uint32_t second_address;
            ExtractProblemRegisterInstructionPair(
                user_data, NULL, NULL, &first_address, NULL,
                &second_address, NULL);
            RenderInstAddressPair(first_address, second_address,
                                  buffer, buffer_size);
          } else if (
              method == kReportProblemRegisterListInstructionPair) {
            uint32_t first_address;
            uint32_t second_address;
            ExtractProblemRegisterListInstructionPair(
                user_data, NULL, NULL, &first_address, NULL,
                &second_address, NULL);
            RenderInstAddressPair(first_address, second_address,
                                  buffer, buffer_size);
          }  // else don't print anything.
          break;
        case 'c':  // Print out conditions associated with
                   // problem instruction pair.
          if (method == kReportProblemInstructionPair) {
            nacl_arm_dec::Instruction first_inst;
            nacl_arm_dec::Instruction second_inst;
            ExtractProblemInstructionPair(user_data, NULL,
                                          NULL, &first_inst,
                                          NULL, &second_inst);
            RenderInstCondPair(first_inst, second_inst,
                               buffer, buffer_size);
          } else if (method == kReportProblemRegisterInstructionPair) {
            nacl_arm_dec::Instruction first_inst;
            nacl_arm_dec::Instruction second_inst;
            ExtractProblemRegisterInstructionPair(user_data, NULL, NULL,
                                                  NULL, &first_inst,
                                                  NULL, &second_inst);
            RenderInstCondPair(first_inst, second_inst,
                               buffer, buffer_size);
          } else if (
              method == kReportProblemRegisterListInstructionPair) {
            nacl_arm_dec::Instruction first_inst;
            nacl_arm_dec::Instruction second_inst;
            ExtractProblemRegisterListInstructionPair(
                user_data, NULL, NULL, NULL, &first_inst, NULL, &second_inst);
            RenderInstCondPair(first_inst, second_inst,
                               buffer, buffer_size);
          }  // else don't print anything.
          break;
        case 'r':  // Print out register(s) associated with reported problem.
          if (method == kReportProblemRegister) {
            nacl_arm_dec::Register reg;
            ExtractProblemRegister(user_data, &reg);
            RenderRegister(reg, buffer, buffer_size);
          } else if (method == kReportProblemRegisterInstructionPair) {
            nacl_arm_dec::Register reg;
            ExtractProblemRegisterInstructionPair(
                user_data, NULL, &reg, NULL, NULL, NULL, NULL);
            RenderRegister(reg, buffer, buffer_size);
          } else if (method == kReportProblemRegisterList) {
            nacl_arm_dec::RegisterList registers;
            ExtractProblemRegisterList(user_data, &registers);
            RenderRegisterList(registers, buffer, buffer_size);
          } else if (method == kReportProblemRegisterListInstructionPair) {
            nacl_arm_dec::RegisterList registers;
            ExtractProblemRegisterListInstructionPair(
                user_data, NULL, &registers, NULL, NULL, NULL, NULL);
            RenderRegisterList(registers, buffer, buffer_size);
          } else {
            RenderText("register", buffer, buffer_size);
          }
          break;
        case 'F':
          {
            uint32_t first_address;
            if (method == kReportProblemInstructionPair) {
              ExtractProblemInstructionPair(user_data, NULL,
                                            &first_address, NULL, NULL, NULL);
            } else if (method == kReportProblemRegisterInstructionPair) {
              ExtractProblemRegisterInstructionPair(
                  user_data, NULL, NULL, &first_address, NULL, NULL, NULL);
            } else if (
                method == kReportProblemRegisterListInstructionPair) {
              ExtractProblemRegisterListInstructionPair(
                  user_data, NULL, NULL, &first_address, NULL, NULL, NULL);
            }  else {
              // else don't print anything.
              break;
            }
            RenderInstAddress(first_address, buffer, buffer_size);
          }
          break;
        case 'R':
          {
            ValidatorInstructionPairProblem pair_problem;
            if (method == kReportProblemInstructionPair) {
              ExtractProblemInstructionPair(user_data, &pair_problem,
                                            NULL, NULL, NULL, NULL);
            } else if (method == kReportProblemRegisterInstructionPair) {
              ExtractProblemRegisterInstructionPair(
                  user_data, &pair_problem, NULL, NULL, NULL, NULL, NULL);
            } else if (
                method == kReportProblemRegisterListInstructionPair) {
              ExtractProblemRegisterListInstructionPair(
                  user_data, &pair_problem, NULL, NULL, NULL, NULL, NULL);
            } else {
              // else don't print anything.
              break;
            }
            Render(buffer, buffer_size,
                   kValidatorInstructionPairProblem[pair_problem],
                   problem, method, user_data);
          }
          break;
        case 'S':
          {
            uint32_t second_address;
            if (method == kReportProblemInstructionPair) {
              ExtractProblemInstructionPair(user_data, NULL,
                                            NULL, NULL, &second_address, NULL);
            } else if (method == kReportProblemRegisterInstructionPair) {
              ExtractProblemRegisterInstructionPair(
                  user_data, NULL, NULL, NULL, NULL, &second_address, NULL);
            } else if (
                method == kReportProblemRegisterListInstructionPair) {
              ExtractProblemRegisterListInstructionPair(
                  user_data, NULL, NULL, NULL, NULL, &second_address, NULL);
            }  else {
              // else don't print anything.
              break;
            }
            RenderInstAddress(second_address, buffer, buffer_size);
          }
          break;
        default:  // Not directive, print out character.
          RenderChar(*format, buffer, buffer_size);
          break;
      }
    } else {
      // Not in directive, print out character.
      RenderChar(*format, buffer, buffer_size);
    }
    format++;
  }
}

void ProblemReporter::ToText(char* buffer,
                             size_t buffer_size,
                             uint32_t vaddr,
                             ValidatorProblem problem,
                             ValidatorProblemMethod method,
                             const ValidatorProblemUserData user_data) {
  assert(buffer_size > 0);
  buffer[0] = '\0';
  assert(problem < kValidatorProblemSize);
  RenderAdvance(SNPRINTF(buffer, buffer_size, "%8"NACL_PRIx32, vaddr),
                &buffer, &buffer_size);
  RenderText(": ", &buffer, &buffer_size);
  Render(&buffer, &buffer_size,
         ValidatorProblemFormatDirective[problem],
         problem, method, user_data);
}

void ProblemReporter::ToText(char* buffer,
                             size_t buffer_size,
                             ValidatorProblem problem,
                             ValidatorProblemMethod method,
                             const ValidatorProblemUserData user_data) {
  assert(buffer_size > 0);
  buffer[0] = '\0';
  assert(problem < kValidatorProblemSize);
  Render(&buffer, &buffer_size,
         ValidatorProblemFormatDirective[problem],
         problem, method, user_data);
}

}  // namespace nacl_arm_val
