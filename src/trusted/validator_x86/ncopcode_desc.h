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
 * ncopcode_desc.h - Descriptors to model opcode operands.
 */
#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCOPCODE_DESC_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCOPCODE_DESC_H_

#include <stdio.h>
#include "native_client/src/trusted/validator_x86/ncdecode.h"
#include "native_client/src/shared/utils/types.h"

/* Defines the set of known (opcode) instructions. */
typedef enum {
  InstUndefined,  /* Special marker for problems parsing instructions. */
  InstAaa,
  InstAas,
  InstAdc,
  InstAdd,
  InstAnd,
  InstArpl,
  InstBound,
  InstCall,
  InstCmp,
  InstDaa,
  InstDas,
  InstDec,
  InstHlt,
  InstImul,
  InstInc,
  InstJb,
  InstJbe,
  InstJcxz,
  InstJg,
  InstJge,
  InstJl,
  InstJle,
  InstJmp,
  InstJnb,
  InstJnbe,
  InstJno,
  InstJnp,
  InstJns,
  InstJnz,
  InstJo,
  InstJp,
  InstJs,
  InstJz,
  InstLea,
  InstLeave,
  InstMov,
  InstMovsxd,
  InstNop,
  InstOr,
  InstPop,
  InstPopa,
  InstPopad,
  InstPush,
  InstPusha,
  InstPushad,
  InstRcl,
  InstRcr,
  InstRet,
  InstRol,
  InstRor,
  InstSar,
  InstSbb,
  InstShl,
  InstShr,
  InstSub,
  InstTest,
  InstXor,
  /* TODO(karl) Add missing instructions. */
  InstMnemonicEnumSize /* Special marker denoting size */
} InstMnemonic;

/* Returns the name of the corresponding (opcode) instruction, less
 * the "Inst" prefix.
 */
extern const char* InstMnemonicName(const InstMnemonic mnemonic);

/* Defines the set of possible instruction prefices. */
typedef enum {
  NoPrefix,
  Prefix0F,
  PrefixF20F,
  PrefixF30F,
  Prefix660F,
  Prefix0F0F,
  Prefix0F38,
  Prefix660F38,
  PrefixF20F38,
  Prefix0F3A,
  Prefix660F3A,
  /* Special marker denoting OpcodePrefix size. */
  OpcodePrefixEnumSize
} OpcodePrefix;

/* returns the name of an opcode prefix. */
extern const char* OpcodePrefixName(OpcodePrefix prefix);

/* Defines the maximum allowable bytes per x86 instruction. */
#define MAX_BYTES_PER_X86_INSTRUCTION 15

/* Defines instruction opcode modifier flags. */
/* Note: The following sufficies are used:
 *    b - 8 bits.
 *    w - 16 bits.
 *    v - 32 bits.
 *    o - 64 bits.
 */

/* Define set of possible instruction opcode flags that can apply
 * to an instruction.
 */
typedef enum {
  /* Indicates the use of a rex pefix that affects the operand size or
   * instruction semantics. Intel's Notation is REX.W. Only applies if
   * decoder is running in 64-bit mode.
   */
  OpcodeUsesRexW,

  /* Indicates that opcode has REX prefix and REX.R is set. */
  OpcodeHasRexR,

  /* Indicates that opcode has REX prefix and REX.R is not set. */
  OpcodeHasNoRexR,

  /* Defines instruction that uses opcode value 0-7 in the ModR/M.rm field
   * as an operand. Intel's notation is /digit. Note: This data will be
   * stored in the first operand, using a ModRmOpcode operand kind.
   */
  OpcodeInModRm,

  /* Defines instruction that the ModR/M byte contains a register operand and
   * an r/m operand. Intel's notation is /r
   */
  OpcodeUsesModRm,

  /* Defines the size of the immediate value that must follow the opcode.
   * Intel's notation is ib, iw, id, and io.
   */
  OpcodeHasImmed,
  OpcodeHasImmed_b,
  OpcodeHasImmed_w,
  OpcodeHasImmed_v,
  OpcodeHasImmed_o,

  /* In 32-bit mode, same as OpcodeHasImmed_v, In 64-bit mode, same as
   * OpcodeHasImmed_o.
   */
  OpcodeHasImmed_Addr,

  /* Defines a register code, from 0 through 7, added to the hexadecimal byte
   * associated with the instruction, based on the operand size.
   * Intel's notation is +rb. See Intel manual, table 3-1 for details.
   * Note: to compute value 0-7, see first operand, which should be OperandBase.
   */
  OpcodePlusR,

  /* Defines a number used in floating-point instructions when one of the
   * operands is ST(i) from the FPU register stack. The number i (which can
   * range from 0 to 7) is added to the hexidecimal byte given at the left
   * of the plus sign to form a single opcode byte. Intel's notation is +i.
   * Note: to compute value 0-7, see first operand, which sould be OperandBase.
   */
  OpcodePlusI,

  /* Defines that in 64-bit mode, REX prefix should appear if using a 64-bit
   * only register. Only applicable if running in 64-bit mode.
   */
  OpcodeRex,

  /* Indicates the REX prefix does not affect the legacy instruction in 64-bit
   * mode. Intel's notation is N.P.
   */
  OpcodeLegacy,

  /* Defines that the opcode can be prefixed with a lock prefix. */
  OpcodeLockable,

  /* Opcode only applies if running in 32-bit mode. */
  Opcode32Only,

  /* Opcode only applies if running in 64-bit mode. */
  Opcode64Only,

  /* Defines the expected size of the operand. Can be repeated.
   * The (possibly repeated) flags define the set possible operand
   * sizes that are allowed by the opcode.
   */
  OperandSize_b,
  OperandSize_w,
  OperandSize_v,
  OperandSize_o,

  /* Defines the expected size of addresses. Can be repeated.
   * The (possibly repeated) flags define the set of possible
   * address sizes that are allowed by the opcode.
   */
  AddressSize_w,
  AddressSize_v,
  AddressSize_o,

  /* Operand size defaults to size 64 in 64-bit mode. */
  OperandSizeDefaultIs64,

  /* Operand size must be 64 in 64-bit mode. */
  OperandSizeForce64,

  /* Must parse opcode prefix OF as part of instruction. */
  /* OpcodeOF, */

  /* Special marker denoting the number of opcode flags. */
  OpcodeFlagEnumSize,
} OpcodeFlagEnum;

/* Returns the print name for an OpcodeFlagEnum. */
extern const char* OpcodeFlagName(const OpcodeFlagEnum flag);

/* Defines integer to represent sets of possible opcode flags */
typedef uint32_t OpcodeFlags;

/* Converts an OpcodeFlagEnum to the corresponding bit in OpcodeFlags. */
#define InstFlag(x) (((OpcodeFlags) 1) << (x))

/* Defines possible kinds of operands an opcode can have. */
typedef enum {
  Unknown_Operand,   /* Unknown operand. */

  /* Note: The instruction decoder may count on the fact that the A_operand
   * values are contiguous, in the order specifed.
   */

  /* Models a far pointer. */
  A_Operand,

  /* A far pointer, consisting of a 16-bit selector, and
   * a 16 bit offset (operand size is 16). Intel's notation
   * is ptr16:16.
   */
  Aw_Operand,

  /* A far pointer, consisting of a 16-bit selector, and
   * a 32-bit offset (operand size is 32). Intel's notation
   * is ptr16:32.
   */
  Av_Operand,

  /* A far pointer, consisting of a 16-bit selector, and
   * a 64-bit offset (operand size is 64). Intel's notation
   * is ptr16:64.
   */
  Ao_Operand,

  /* Note: The instruction decoder may count on the fact that the E_operand
   * values are contiguous, in the order specified.
   */

  /* Models a general purpose register or memory operand.
   * Size is defined by the computed operand size.
   */
  E_Operand,

  /* Models an 8-bit general-purpose register or memory
   * operand. Intel's notation is r/m8.
   */
  Eb_Operand,

  /* Models a 16-bit general-purpose register or memory
   * operand. Intel's notation is r/m16
   */
  Ew_Operand,

  /* Models a 32-bit general-purpose register or memory
   * operand. Intel's notation is r/m32.
   */
  Ev_Operand,

  /* Models a 64-bit general-purpose register or memory
   * operand. Intel's notation is r/m64.
   */
  Eo_Operand,

  /* Note: The instruction decoder may count on the fact that the G_Operand
   * values are contiguous, in the order specified.
   */

  /* Models a general purpose register operand.
   * Size is defined by the computed operand size.
   */
  G_Operand,
  /* Models an 8-bit general purpose register operand.
   * Intel's notation is r8.
   */
  Gb_Operand,

  /* Models a 16-bit general purpose register operand.
   * Intel's notation is r16.
   */
  Gw_Operand,

  /* Models a 32-bit general-purpose register operand.
   * Intel's notation is r32
   */
  Gv_Operand,

  /* Models a 64 bit general-purpose register operand.
   * Intel's notation is r64.
   */
  Go_Operand,

  /* Models registers using the difference in the opcode base, from the opcode
   * value.
   */
  G_OpcodeBase,

  /* Note: The instruction decoder may count on the fact that the I_Operand
   * values are contiguous, in the order specified.
   */

  /* Model an immediate value. Size is defined by the
   * computed operand size.
   */
  I_Operand,

  /* An immediate 8-bit value. Intel's notation is imm8. */
  Ib_Operand,

  /* An immediate 16-bit value. Intel's notation is imm16. */
  Iw_Operand,

  /* An immediate 32-bit value. Intel's notation is imm32. */
  Iv_Operand,

  /* An immediate 64-bit value. Intel's notation is imm64. */
  Io_Operand,

  /* Note: The instruction decoder may count on the fact that the J_Operand
   * values are contiguous, in the order specified.
   */

  /* A relative address to branch to. Size is defined by
   * the computed operand size.
   */
  J_Operand,

  /* A relative address (range 128 before, 127 after) the
   * end of the instruction. Intel's notation is rel8.
   */
  Jb_Operand,

  /* A relative address that is 16 bits long (operand size).
   * Intel's notation is rel16
   */
  Jw_Operand,

  /* A relative address that is 32 bits long (operand size).
   * Intel's notation is rel32.
   */
  Jv_Operand,

  /* Note: The instruction decoder may count on the fact that the M_Operand
   * values are contiguous, in the order specified.
   */

  /* A 16, 32, or 64 bit operand in memory. ???
   * Intel's notation is m. Size is defined by the
   * computed operand size.
   */
  M_Operand,

  /* A 8-bit operand in memory pointerd to by the DS:(E)Si or
   * ES:(E)DI registers. In 64-bit mode, it is pointed to
   * by the RSI or RDI registers. Intel's notation is m8.
   */
  Mb_Operand,

  /* A 16-bit operand in memory pointed to by the DS:(E)SI
   * or ES:(E)DI registers. Intel's notation is m16.
   */
  Mw_Operand,

  /* A 32-bit operand in memory pointed to by the DS:(E)SI
   * or ES:(E)DI registers. Intel's notation is m32.
   */
  Mv_Operand,

  /* A 64-bit operand in memory. Intel's notation is m64. */
  Mo_Operand,

  /* A 128-bit operand in memory. Intel's notation is m128. */
  Mqo_Operand,

  /* A memory operand containing a far pointer composed of
   * a 16-bit selector and a 16 bit offset. Intel's notation
   * is m16:16.
   */
  Mpw_Operand,

  /* A memory operand containing a far pointer composed of
   * a 16 bit selector and a 32 bit offset. Intel's notation
   * is m16:32.
   */
  Mpv_Operand,

  /* A memory operand containing a far pointer composed of
   * a 16 bit selector and a 64 bit offset. Intel's notation
   * is m16:64.
   */
  Mpo_Operand,

  /* Note: The instruction decoder may count on the fact that the O_Operand
   * values are contiguous, in the order specified.
   */

  /* A memory offset. Size is defined by the computed operand size. */
  O_Operand,

  /* A memory 8-bit offset. Intel's notation is moffs8. */
  Ob_Operand,

  /* A memory 16-bit offset. Intel's notation is moffs16. */
  Ow_Operand,

  /* A memory 32-bit offset. Intel's notation is moffs32. */
  Ov_Operand,

  /* A memory 64-bit offset. Intel's notation is moffs64. */
  Oo_Operand,

  /* A segment register. The segment register bit assignments
   * are ES=0, CS=1, SS=2, DS=3, FS=4, GS=5. Intel's notation
   * is Sreg.
   */
  S_Operand,

  /* Note: The instruction decoder may count on the fact that the list
   * of register values are contiguous, in the order specified.
   */

  /* Unknown register - Used if actual register can't
   *be determined by the instruction decoder.
   */
  RegUnknown,
  RegAL,             /* 8-bit registers in 32-bit and 64-bit modes. */
  RegBL,
  RegCL,
  RegDL,

  RegAH,             /* Additional 8-bit regisister in 32-bit mode. */
  RegBH,
  RegCH,
  RegDH,

  RegDIL,            /* Additional 8-bit registers in 64-bit mode. */
  RegSIL,
  RegBPL,
  RegSPL,
  /* Note: Intel manual claims that r8l..r15l should be used. However, AMD
   * manual and (xed from Intel) uses r8b..r15b. Therefore we will use the
   * latter.
   */
  RegR8B,
  RegR9B,
  RegR10B,
  RegR11B,
  RegR12B,
  RegR13B,
  RegR14B,
  RegR15B,

  RegAX,             /* 16 bit registers in 32-bit and 64-bit modes. */
  RegBX,
  RegCX,
  RegDX,
  RegSI,
  RegDI,
  RegBP,
  RegSP,

  RegR8W,             /* 16 bit registers only in 64-bit mode. */
  RegR9W,
  RegR10W,
  RegR11W,
  RegR12W,
  RegR13W,
  RegR14W,
  RegR15W,


  RegEAX,             /* General 32-bit registers in 32-bit and 64-bit modes. */
  RegEBX,
  RegECX,
  RegEDX,
  RegESI,
  RegEDI,
  RegEBP,
  RegESP,

  RegR8D,             /* Additional 32-bit registers in 64-bit mode. */
  RegR9D,
  RegR10D,
  RegR11D,
  RegR12D,
  RegR13D,
  RegR14D,
  RegR15D,

  RegCS,              /* 16-bit segment registers in 32-bit and 64-bit modes. */
  RegDS,
  RegSS,
  RegES,
  RegFS,
  RegGS,


  RegEFLAGS,          /* Program status and control register in 32-bit mode. */
  RegRFLAGS,          /* Program status and control register in 64-bit mode. */

  RegEIP,             /* Instruction pointer in 32-bit mode. */
  RegRIP,             /* Instruction pointer in 64-bit mode. */


  RegRAX,             /* General purpose 64-bit registers in 64-bit mode. */
  RegRBX,
  RegRCX,
  RegRDX,
  RegRSI,
  RegRDI,
  RegRBP,
  RegRSP,
  RegR8,
  RegR9,
  RegR10,
  RegR11,
  RegR12,
  RegR13,
  RegR14,
  RegR15,

  /* Stack register, based on address size - ESP in 32-bits, RSP in 64-bits. */
  RegRESP,
  /* Use AX, EAX, or RAX, based on operand size. */
  RegREAX,
  /* Use EIP or RIP, based on address size - EIP in 32-bits, RIP in 64-bits. */
  RegREIP,
  /* Use EBP or RBP, based on 32/64 bit model. */
  RegREBP,

  /* One of the eight general purpose registers, less the stack pointer, based
   * on operand size.
   */
  RegGP7,

  /* The following only appears as the first (implicit) operand when the Opcode
   * has flag OpcodePlusR or OpcodePlusI . When defined, the operand defines the
   * amount to subtract from the opcode to get the opcode base.
   */
  OpcodeBaseMinus0,
  OpcodeBaseMinus1,
  OpcodeBaseMinus2,
  OpcodeBaseMinus3,
  OpcodeBaseMinus4,
  OpcodeBaseMinus5,
  OpcodeBaseMinus6,
  OpcodeBaseMinus7,

  /* The following only appears as the first (implicit) operand when the Opcode
   * has flag OpcodeInModRm. When defined, the operand defines the opcode value
   * inside the Mod/Rm byte.
   */
  Opcode0,
  Opcode1,
  Opcode2,
  Opcode3,
  Opcode4,
  Opcode5,
  Opcode6,
  Opcode7,

  /* Predefined constants. */
  Const_1,

  /* Special marker defining the end of the list. */
  OperandKindEnumSize
} OperandKind;

/* Returns the print name for the corresponding OperandKindEnum. */
extern const char* OperandKindName(const OperandKind kind);

/* Define a set of possible opcode operand flags that can apply to an
 * operand.
 */
typedef enum {
  /* Operand is used */
  OpUse,
  /* Operand is set  */
  OpSet,
  /* Operand is an address (as in LEA). */
  OpAddress,
  /* Operand is implicit (rather than explicit) */
  OpImplicit,
  /* Operand is an extension of the opcode, rather than a (real) operand.
   * This should be defined on the first operand for opcodes with values
   * OpcodeInModRm, OpcodePlusR, and OpcodePlusI.
   */
  OperandExtendsOpcode,
  /* When jump address, the jump is near (rather than far). */
  OperandNear,
  /* When jump address, the jump is relative (rather than absolute. */
  OperandRelative,
  /* Operand zero-extends 32-bit register results to 64-bits. */
  OperandZeroExtends_v,
  /* Special marker denoting the number of operand flags. */
  OperandFlagEnumSize
} OperandFlagEnum;

/* Returns the print name for the corresponding OperandFlagEnum. */
const char* OperandFlagName(const OperandFlagEnum flag);

/* Defines integer to represent sets of possible operand flags. */
typedef uint32_t OperandFlags;

/* Converts an OperandFlagEnum to the corresponding bit in OperandFlags. */
#define OpFlag(x) (((OperandFlags) 1) << x)

/* Metadata about an instruction operand. */
typedef struct Operand {
  /* The kind of the operand (i.e. kind of data modeled by the operand).*/
  OperandKind kind;
  /* Flags defining additional facts about the operand. */
  OperandFlags flags;
} Operand;


/* Maximum number of operands in an x86 instruction (implicit and explicit). */
#define MAX_NUM_OPERANDS 4

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
