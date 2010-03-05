/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * ncopcode_desc.h - Descriptors to model opcode operands.
 */
#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCOPCODE_DESC_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCOPCODE_DESC_H_

#include <stdio.h>
#include "native_client/src/trusted/validator_x86/ncdecode.h"
#include "native_client/src/shared/utils/types.h"

/* Define enumerated types. */
#include "gen/native_client/src/trusted/validator_x86/ncopcode_prefix.h"
#include "gen/native_client/src/trusted/validator_x86/ncopcode_insts.h"
#include "gen/native_client/src/trusted/validator_x86/ncopcode_opcode_flags.h"
#include "gen/native_client/src/trusted/validator_x86/ncopcode_operand_kind.h"
#include "gen/native_client/src/trusted/validator_x86/ncopcode_operand_flag.h"

/* Defines the maximum allowable bytes per x86 instruction. */
#define MAX_BYTES_PER_X86_INSTRUCTION 15

/* Defines integer to represent sets of possible opcode flags */
typedef uint64_t OpcodeFlags;

/* Converts an OpcodeFlagEnum to the corresponding bit in OpcodeFlags. */
#define InstFlag(x) (((OpcodeFlags) 1) << (x))

/* Models the empty set of opcode flags. */
#define EmptyInstFlags ((OpcodeFlags) 0)

/* Defines integer to represent sets of possible operand flags. */
typedef uint32_t OperandFlags;

/* Converts an OperandFlagEnum to the corresponding bit in OperandFlags. */
#define OpFlag(x) (((OperandFlags) 1) << x)

/* Models the empty set of operand flags. */
#define EmptyOpFlags ((OperandFlags) 0)

/* Metadata about an instruction operand. */
typedef struct Operand {
  /* The kind of the operand (i.e. kind of data modeled by the operand).*/
  OperandKind kind;
  /* Flags defining additional facts about the operand. */
  OperandFlags flags;
} Operand;


/* Maximum number of operands in an x86 instruction (implicit and explicit). */
#define MAX_NUM_OPERANDS 6

/* Maxmimum number of opcode bytes per instruction. */
#define MAX_OPCODE_BYTES 3

/* Metadata about an instruction, defining a pattern. Note: Since the same
 * sequence of opcode bytes may define more than one pattern (depending on
 * other bytes in the parsed instruction), the patterns are
 * modeled using a singly linked list.
 */
typedef struct Opcode {
  /* The number of opcode bytes in the instruction. */
  uint8_t num_opcode_bytes;
  /* The actual opcode bytes. */
  uint8_t opcode[MAX_OPCODE_BYTES];
  /* The (last) byte value representing the (opcode) instruction. */
  /* uint8_t opcode; */
  /* Defines the origin of this instruction. */
  NaClInstType insttype;
  /* Flags defining additional facts about the instruction. */
  OpcodeFlags flags;
  /* The instruction that this instruction implements. */
  InstMnemonic name;
  /* The number of operands modeled for this instruction. */
  uint8_t num_operands;
  /* The corresponding models of the operands. */
  Operand operands[MAX_NUM_OPERANDS];
  /* Pointer to the next pattern to try and match for the
   * given sequence of opcode bytes.
   */
  struct Opcode* next_rule;
} Opcode;

/* Implements trie nodes for selecting instructions that must match
 * a specific sequence of bytes. Used to handle NOP cases.
 */
typedef struct NcOpcodeNode {
  Opcode* matching_opcode;
  struct NcOpcodeNode* succs[256];
} NcOpcodeNode;

/* Returns the number of logical operands an Opcode has. That is,
 * returns field num_operands unless the first operand is
 * a special encoding that extends the opcode.
 */
uint8_t NcGetOpcodeNumberOperands(Opcode* opcode);

/* Returns the indexed logical operand for the opcode. That is,
 * returns the index-th operand unless the first operand is
 * a special encoding that extends the opcode. In the latter
 * case, the (index+1)-th operand is returned.
 */
Operand* NcGetOpcodeOperand(Opcode* opcode, uint8_t index);

/* Print out the given operand structure to the given file. */
void PrintOperand(FILE* f, Operand* operand);

/* Print out the given opcode structure to the given file. However, always
 * print the value NULL for next_rule, even if the value is non-null. This
 * function should be used to print out an individual opcode (instruction)
 * pattern.
 */
void PrintOpcode(FILE* f,  Opcode* opcode);

/* Prints out the given opcode structure to the given file. If index >= 0,
 * print out a comment, with the value of index, before the printed opcode
 * structure. Lookahead is used to convert the next_rule pointer into
 * a symbolic reference using the name "g_Opcodes", plus the index and
 * the lookahead.
 */
void PrintOpcodeTablegen(FILE* f, int index, Opcode* opcode, int lookahead);

/* Prints out the given opcode structure to the given file. If index >= 0,
 * print out a comment, with the value of index, before the printed opcode
 * structure. Lookahead is used to convert the next_rule pointer into
 * a symbolic reference using the name "g_Opcodes", plus the index defined by
 * the lookahead. Argument as_array_element is true if the element is
 * assumed to be in an array static initializer. If argument simplify is
 * true, then the element is for documentation purposes only (as a single
 * element), and simplify the output to only contain (user-readable)
 * useful information.
 */
void PrintOpcodeTableDriver(FILE* f, Bool as_array_element, Bool simplify,
                            int index, Opcode* opcode, int lookahead);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCOPCODE_DESC_H_ */
