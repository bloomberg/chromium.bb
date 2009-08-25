/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Defines the internal API to modeled ARM instructions.
 */

#ifndef NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_ARM_INST_MODELING_H__
#define NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_ARM_INST_MODELING_H__

#include "native_client/src/trusted/validator_arm/arm_insts.h"

EXTERN_C_BEGIN

/* Define information used to model instructions. */
typedef struct ModeledOpInfo {
  const char* name;            /* Printable name for the operation. */
  ArmInstKind inst_kind;       /* The kind of instruction. */
  const char* inst_access;     /* Name of the ArmAccessMode* to use */
  ArmInstType inst_type;       /* Type of instruction */
  Bool arm_safe;               /* true iff allowed in native client */
  const char* describe_format; /* Print format for disassembled text. */
  int num_bytes;               /* number of bytes int the instruction. */
  const char* check_fcn;       /* Name of the function to call to check
                                * additional properties (or "NULL" if no
                                * such function.
                                */
  InstValues expected_values;  /* Extracted values based on corresponding masks
                                * for the type of instruction.
                                */
} ModeledOpInfo;

/* Name possible types of access modes. List of names must match the
 * extern constant ArmAccessMode's defined in arm_insts.h"
 */
#define kArmUndefinedAccessModeName "kArmUndefinedAccessMode"
#define kArmDataImmediateName "kArmDataImmediate"
#define kArmDataLslImmediateName "kArmDataLslImmediate"
#define kArmDataLsrImmediateName "kArmDataLsrImmediate"
#define kArmDataAsrImmediateName "kArmDataAsrImmediate"
#define kArmDataRorImmediateName "kArmDataRorImmediate"
#define kArmDataRrxName "kArmDataRrx"
#define kArmDataRegisterName "kArmDataRegister"
#define kArmDataLslRegisterName "kArmDataLslRegister"
#define kArmDataLsrRegisterName "kArmDataLsrRegister"
#define kArmDataAsrRegisterName "kArmDataAsrRegister"
#define kArmDataRorRegisterName "kArmDataRorRegister"
#define kArmLsImmediateOffsetName "kArmLsImmediateOffset"
#define kArmLsRegisterOffsetName "kArmLsRegisterOffset"
#define kArmLsScaledRegisterLslOffsetName "kArmLsScaledRegisterLslOffset"
#define kArmLsScaledRegisterLsrOffsetName "kArmLsScaledRegisterLsrOffset"
#define kArmLsScaledRegisterAsrOffsetName "kArmLsScaledRegisterAsrOffset"
#define kArmLsScaledRegisterRorOffsetName "kArmLsScaledRegisterRorOffset"
#define kArmLsScaledRegisterRrxOffsetName "kArmLsScaledRegisterRrxOffset"
#define kArmLsImmediatePreIndexedName "kArmLsImmediatePreIndexed"
#define kArmLsRegisterPreIndexedName "kArmLsRegisterPreIndexed"
#define kArmLsScaledRegisterLslPreIndexedName \
  "kArmLsScaledRegisterLslPreIndexed"
#define kArmLsScaledRegisterLsrPreIndexedName \
  "kArmLsScaledRegisterLsrPreIndexed"
#define kArmLsScaledRegisterAsrPreIndexedName \
  "kArmLsScaledRegisterAsrPreIndexed"
#define kArmLsScaledRegisterRorPreIndexedName \
  "kArmLsScaledRegisterRorPreIndexed"
#define kArmLsScaledRegisterRrxPreIndexedName \
  "kArmLsScaledRegisterRrxPreIndexed"
#define kArmLsImmediatePostIndexedName "kArmLsImmediatePostIndexed"
#define kArmLsRegisterPostIndexedName "kArmLsRegisterPostIndexed"
#define kArmLsScaledRegisterLslPostIndexedName \
  "kArmLsScaledRegisterLslPostIndexed"
#define kArmLsScaledRegisterLsrPostIndexedName \
  "kArmLsScaledRegisterLsrPostIndexed"
#define kArmLsScaledRegisterAsrPostIndexedName \
  "kArmLsScaledRegisterAsrPostIndexed"
#define kArmLsScaledRegisterRorPostIndexedName \
  "kArmLsScaledRegisterRorPostIndexed"
#define kArmLsScaledRegisterRrxPostIndexedName \
  "kArmLsScaledRegisterRrxPostIndexed"
#define kArmMlsImmediateOffsetName "kArmMlsImmediateOffset"
#define kArmMlsRegisterOffsetName "kArmMlsRegisterOffset"
#define kArmMlsImmediatePreIndexedName "kArmMlsImmediatePreIndexed"
#define kArmMlsRegisterPreIndexedName "kArmMlsRegisterPreIndexed"
#define kArmMlsImmediatePostIndexedName "kArmMlsImmediatePostIndexed"
#define kArmMlsRegisterPostIndexedName "kArmMlsRegisterPostIndexed"
#define kArmLscImmediateOffsetName "kArmLscImmediateOffset"
#define kArmLscImmediatePreIndexedName "kArmLscImmediatePreIndexed"
#define kArmLscImmediatePostIndexedName "kArmLscImmediatePostIndexed"
#define kArmLscUnindexedName "kArmLscUnindexed"

/* Define names of possible check fucntions. */
#define NULLName "NULL"
#define Args_1_4_NotEqualName "Args_1_4_NotEqual"
#define Args_1_4_NotEqualName "Args_1_4_NotEqual"
#define Args_3_4_NotBothZeroName "Args_3_4_NotBothZero"
#define Arg1NotInLoadStoreRegistersName "Arg1NotInLoadStoreRegisters"
#define R15NotInLoadStoreRegistersName "R15NotInLoadStoreRegisters"
#define R15InLoadStoreRegistersName "R15InLoadStoreRegisters"

/* Models a list of known instructions. */
typedef struct ModeledArmInstructionSet {
  /* The number of known instructions */
  int size;
  /* The information for the i-th instruction in the instruction set. */
  const ModeledOpInfo* instructions[MAX_ARM_INSTRUCTION_SET_SIZE];
} ModeledArmInstructionSet;

/*
 * Special marker denoting the end of the list of known instructions.
 */
#define END_OPINFO_LIST { "???", ARM_UNKNOWN_INST,  \
    kArmUndefinedAccessModeName, ARM_INST_TYPE_SIZE, 0, \
    "?error?",  ARM_WORD_LENGTH, NULLName, \
    { NOT_USED, NOT_USED, NOT_USED, NOT_USED, NOT_USED, NOT_USED, \
      NOT_USED, NOT_USED, NOT_USED }}

/*
 * Define the instruction set. Not built until BuildArmInstructionSet called.
 */
extern ModeledArmInstructionSet modeled_arm_instruction_set;

/*
 * Add the given instruction to the set of known instructions.
 */
void AddInstruction(const ModeledOpInfo* instruction);

/*
 * Construct a new copy of the given OpInfo.
 */
ModeledOpInfo* CopyOfOpInfo(const ModeledOpInfo* op);

/*
 * Strip out flags that define which subsets (of all posible ARM instructions)
 * should be built. Return a new argc value and update argv to match the
 * returned argc.
 */
int GrokArmBuildFlags(int argc, const char *argv[]);

/*
 * Take the patterns (specified above), and build the corresponding
 * set of known ARM instructions.
 *
 * Note: Can be called multiple times. Subsequent calls do nothing.
 */
void BuildArmInstructionSet();

/*
 * Prints text describing the data modeling the given indexed instruction
 * to stdout. Returns true if the bxouffer doesn't overflow.
 */
Bool PrintIndexedInstruction(int index);

EXTERN_C_END

#endif  // NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_ARM_INST_MODELING_H__
