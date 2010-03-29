/* Copyright (c) 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Set of predefined instruction forms (via procedure calls), providing
 * a more concise way of specifying opcodes.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCDECODE_FORMS_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCDECODE_FORMS_H__

#include "native_client/src/trusted/validator_x86/ncopcode_desc.h"

/* Defines the general category of instruction, and is used to define
 * set/use values for instructions. That is, most X86 instructions
 * have the form:
 *
 *     OP Dest, Source
 *
 * where OP is some mnemonic name, the first argument is the DEST (because
 * side effecting operations change this value), and Source is a second (use)
 * argument.
 *
 * Note: Unary operands assume form:
 *
 *     OP Dest
 *
 * Reading the text associated with each instruction, one should be able to
 * categorize (most) instructions, into one of the following:
 */
typedef enum NaClInstCat {
  /* The following are for categorizing operands with a single operand. */
  UnarySet,    /* The value of Dest is set to a predetermined value. */
  UnaryUse,    /* The value of Dest is not modified. */
  UnaryUpdate, /* Dest := f(Dest) for some f. */
  /* The following are for categorizing operations with 2 or more operands. */
  Move,       /* Dest := f(Source) for some f. */
  Binary,     /* Dest := f(Dest, Source) for some f. */
  Compare,    /* Sets flag using f(Dest, Source). The value of Dest is not
                 modified.
              */
  Exchange,   /* Dest := f(Dest, Source) for some f, and
                 Source := g(Dest, Source) for some g.
              */
} NaClInstCat;

/* Returns the operand flags for the destination argument of the instruction,
 * given the category of instruction.
 */
NaClOpFlags NaClGetDestFlags(NaClInstCat icat);

/* Returns the operand flags for the source argument of the instruction,
 * given the category of instruction.
 */
NaClOpFlags NaClGetSourceFlags(NaClInstCat icat);

/* Returns the operand flags for the operand with the given index (one-based)
 * of the instruction, given the category of the instruction.
 */
NaClOpFlags NaClGetIcatFlags(NaClInstCat icat, int operand_index);

/*
 * Operands are encoded using up to 3 characters. Each character defines
 * a property of the operand, as follows (if the sequence is less than 3
 * characters, trailing underscores are added to make it a 3 character
 * sequence):
 *
 * E - General purpose register or memory operand specified by the Modrm
 *     byte. Memory addresses can be computed from a segment register, SIB byte,
 *     and/or displacement.
 * G - General purpose register specified by the ModRM reg field.
 * I - Immediate Value.
 * M - A memory operand specified by the ModRm byte.
 * N _ (AMD uses PR) a 64-BIT MMX register specified by the ModRm r/m field.
 *     The ModRm mod field must be 0x3.
 * P - A 64-bit MMX register specified by the ModRm reg field.
 * Q - A 64-bit MMX register or memory operand specified by the ModRm Byte.
 *     Memory addresses can be computed from a segment register, SIB byte,
 *     and/or displacement.
 * U - (AMD uses VR) A 128-bit XMM register specified by the ModRm r/m field.
 *     The ModRm mod field must be 0x3.
 * V - A 128-bit XMM register specified by the ModRM reg field.
 * W - A 128-bit XMM register or memory operand specified by the ModRm Byte.
 *     Memory address can be computed from a segment register, SIB byte, and/or
 *     displacement.
 * b - A byte, irrespective of the effective operand size.
 * d - A doubleword (32 bits), irrespective of the effective operand size.
 * dq - A double-quadword (128 bits), irrespective to the effective operand
 *     size.
 * dQ (AMD uses d/q) A 32/64 bit value, depending on Rex.W.
 * pd - A 128-bit double-precision floating-point vector operand (packed
 *      double).
 * pi - A 64-bit MMX operand (packed integer).
 * ps - A 128-bit single-precision floating-point vector operand (packed
 *     single).
 * q - A quadword, irrespective of effective operand size.
 * sd - A scalar double-precision floating-point operand (scalar double).
 * ss - A scalar single-precision floating-point operand (scalar single).
 * v - A word, doubleword, or quadword, depending on the effective opreand size.
 *
 * Note: These character encodings come from Appendix A of the AMD manual.
 */

/* Generic macro for defining the name of an operand defining function,
 * based on the type it should recognize.
 */
#define DEF_OPERAND(XXX) Define ## XXX ## Operand

/* Generic macro defining an operand defining function, basede on
 * the type it should recognize.
 */
#define DECLARE_OPERAND(XXX) \
  void DEF_OPERAND(XXX)(NaClInstCat icat, int operand_index)

/* The following list are the current set of known (i.e. implemented
 * operand types.
 */

DECLARE_OPERAND(EdQ);

DECLARE_OPERAND(Edq);

DECLARE_OPERAND(Gd_);

DECLARE_OPERAND(Gdq);

DECLARE_OPERAND(I__);

DECLARE_OPERAND(Md_);

DECLARE_OPERAND(Mdq);

DECLARE_OPERAND(Mpd);

DECLARE_OPERAND(Mps);

DECLARE_OPERAND(Mq_);

DECLARE_OPERAND(Nq_);

DECLARE_OPERAND(Pd_);

DECLARE_OPERAND(Pdq);

DECLARE_OPERAND(PdQ);

DECLARE_OPERAND(Ppi);

DECLARE_OPERAND(Pq_);

DECLARE_OPERAND(Qd_);

DECLARE_OPERAND(Qpi);

DECLARE_OPERAND(Qq_);

DECLARE_OPERAND(Udq);

DECLARE_OPERAND(Upd);

DECLARE_OPERAND(Ups);

DECLARE_OPERAND(Uq_);

DECLARE_OPERAND(Vdq);

DECLARE_OPERAND(VdQ);

DECLARE_OPERAND(Vpd);

DECLARE_OPERAND(Vps);

DECLARE_OPERAND(Vq_);

DECLARE_OPERAND(Vsd);

DECLARE_OPERAND(Vss);

DECLARE_OPERAND(Wdq);

DECLARE_OPERAND(Wpd);

DECLARE_OPERAND(Wps);

DECLARE_OPERAND(Wq_);

DECLARE_OPERAND(Wsd);

DECLARE_OPERAND(Wss);

/* Generic macro to define the name of an opcode with no type arguments.
 * Note: Size is intentionally set to be the same as for
 * uses of macro DEF_BINST and DEF_OINST, so that all
 * opcode definitions have the same width.
 */
#define DEF_NULL_OPRDS_INST NaClDefNullOprdsInst

/* Generic routine to define an opcode with no type arguments. */
void DEF_NULL_OPRDS_INST(NaClInstType itype, uint8_t opbyte,
                         NaClInstPrefix prefix, NaClMnemonic inst);

/* Generic macro to define the name of a unary instruction with one type
 * argument, and use the modrm byte to decode the argument.
 */
#define DEF_UNARY_INST(XXX) NaClDef ## XXX ## Inst

/* Declares a unary instruction function whose argument is described
 * by a (3) character sequence type name. Asumes the instruction
 * uses the modrm byte to decode the argument.
 *
 * NOTE: We use macros to define function headers so that type checking
 * can happen on arguments to the corresponding defining function.
 */
#define DECLARE_UNARY_INST(XXX) \
  void DEF_UNARY_INST(XXX)(NaClInstType itype, uint8_t opbyte, \
                           NaClInstPrefix prefix, NaClMnemonic inst,    \
                           NaClInstCat icat)

/* Generic macro to define the name of a unary instruction with two type
 * arguments, and used the modrm field of the modrm byt to refine
 * the opcode being defined.
 */
#define DEF_USUBO_INST(XXX) NaClDef ## XXX ## SubInst

/* Declares a unary instruction function whose argument is
 * decribed by a (3) character sequence type name. Assumes
 * the the modrm field of the modrm byte is used to refine the
 * opcode being defined.
 *
 * NOTE: We use macros to define function headers so that type checking
 * can happen on arguments to the corresponding defining function.
 */
#define DECLARE_UNARY_OINST(XXX) \
  void DEF_USUBO_INST(XXX)(NaClInstType itype, uint8_t opbyte, \
                           NaClInstPrefix prefix,              \
                           NaClOpKind modrm_opcode,            \
                           NaClMnemonic inst,                  \
                           NaClInstCat icat)

DECLARE_UNARY_OINST(Mb_);

/* Generic macro to define the name of an opcode with two type arguments,
 * and use the modrm byte to decode at least one of these arguments.
 */
#define DEF_BINST(XXX, YYY) NaClDef ## XXX ## YYY ## Inst

/* Declares a binary instruction function whose arguments are described
 * by (3) character sequence type names. Assumes the
 * instruction uses the modrm byte to decode at least one of the arguments.
 *
 * NOTE: We use macros to define function headers so that type checking
 * can happen on arguments to the corresponding defining function.
 */
#define DECLARE_BINARY_INST(XXX, YYY) \
  void DEF_BINST(XXX, YYY)(NaClInstType itype, uint8_t opbyte, \
                           NaClInstPrefix prefix, NaClMnemonic inst,     \
                           NaClInstCat icat)

/* The set of binary instructions, with typed arguments, that are recognized. */

DECLARE_BINARY_INST(Edq, Pd_);

DECLARE_BINARY_INST(Edq, Pdq);

DECLARE_BINARY_INST(EdQ, PdQ);

DECLARE_BINARY_INST(Edq, Vdq);

DECLARE_BINARY_INST(EdQ, VdQ);

DECLARE_BINARY_INST(Ev_, Gv_);

DECLARE_BINARY_INST(Gd_, Ups);

DECLARE_BINARY_INST(Gdq, Wsd);

DECLARE_BINARY_INST(Gdq, Wss);

DECLARE_BINARY_INST(Gd_, Nq_);

DECLARE_BINARY_INST(Gd_, Udq);

DECLARE_BINARY_INST(Gd_, Upd);

DECLARE_BINARY_INST(Md_, Vss);

DECLARE_BINARY_INST(Mdq, Vdq);

DECLARE_BINARY_INST(Mdq, Vpd);

DECLARE_BINARY_INST(Mdq, Vps);

DECLARE_BINARY_INST(Mpd, Vpd);

DECLARE_BINARY_INST(Mps, Vps);

DECLARE_BINARY_INST(Mq_, Pq_);

DECLARE_BINARY_INST(Mq_, Vps);

DECLARE_BINARY_INST(Mq_, Vq_);

DECLARE_BINARY_INST(Mq_, Vsd);

DECLARE_BINARY_INST(Pq_, EdQ);

DECLARE_BINARY_INST(Pq_, Nq_);

DECLARE_BINARY_INST(Pq_, Qd_);

DECLARE_BINARY_INST(Pq_, Qq_);

DECLARE_BINARY_INST(Ppi, Wpd);

DECLARE_BINARY_INST(Ppi, Wps);

DECLARE_BINARY_INST(Pq_, Uq_);

DECLARE_BINARY_INST(Pq_, Wpd);

DECLARE_BINARY_INST(Pq_, Wps);

DECLARE_BINARY_INST(Qq_, Pq_);

DECLARE_BINARY_INST(Vdq, Edq);

DECLARE_BINARY_INST(Vdq, EdQ);

DECLARE_BINARY_INST(Vdq, Mdq);

DECLARE_BINARY_INST(Vdq, Udq);

DECLARE_BINARY_INST(Vdq, Uq_);

DECLARE_BINARY_INST(Vdq, Wdq);

DECLARE_BINARY_INST(Vdq, Wps);

DECLARE_BINARY_INST(Vdq, Wq_);

DECLARE_BINARY_INST(Vpd, Qpi);

DECLARE_BINARY_INST(Vpd, Qq_);

DECLARE_BINARY_INST(Vpd, Wdq);

DECLARE_BINARY_INST(Vpd, Wpd);

DECLARE_BINARY_INST(Vpd, Wq_);

DECLARE_BINARY_INST(Vpd, Wsd);

DECLARE_BINARY_INST(Vps, Mq_);

DECLARE_BINARY_INST(Vps, Qpi);

DECLARE_BINARY_INST(Vps, Qq_);

DECLARE_BINARY_INST(Vps, Wpd);

DECLARE_BINARY_INST(Vps, Wps);

DECLARE_BINARY_INST(Vps, Wq_);

DECLARE_BINARY_INST(Vq_, Wdq);

DECLARE_BINARY_INST(Vsd, Edq);

DECLARE_BINARY_INST(Vsd, Mq_);

DECLARE_BINARY_INST(Vsd, Wsd);

DECLARE_BINARY_INST(Vsd, Wss);

DECLARE_BINARY_INST(Vss, Edq);

DECLARE_BINARY_INST(Vss, Wsd);

DECLARE_BINARY_INST(Vss, Wss);

DECLARE_BINARY_INST(Vq_, Mq_);

DECLARE_BINARY_INST(Vq_, Wq_);

DECLARE_BINARY_INST(Wdq, Vdq);

DECLARE_BINARY_INST(Wpd, Vpd);

DECLARE_BINARY_INST(Wps, Vps);

DECLARE_BINARY_INST(Wq_, Vq_);

DECLARE_BINARY_INST(Wsd, Vsd);

DECLARE_BINARY_INST(Wss, Vss);

/* Generic macro to define the name of a binary instruction with two type
 * arguments, and uses the modrm field of the modrm byte to refine
 * the opcode being defined.
 */
#define DEF_OINST(XXX, YYY) NaClDef ## XXX ## YYY ## SubInst

/* Declares a binary instruction function whose arguments are
 * decribed by (3) character sequence type names. Assumes
 * the the modrm field of the modrm byte is used to refine the
 * opcode being defined.
 *
 * NOTE: We use macros to define function headers so that type checking
 * can happen on arguments to the corresponding defining function.
 */
#define DECLARE_BINARY_OINST(XXX, YYY) \
  void DEF_OINST(XXX, YYY)(NaClInstType itype, uint8_t opbyte, \
                           NaClInstPrefix prefix, \
                           NaClOpKind modrm_opcode, \
                           NaClMnemonic inst, \
                           NaClInstCat icat)

/* The set of binary functions (with opcode refinement in the modrm byte),
 * with typed aruments, that are recognized.
 */

DECLARE_BINARY_OINST(Ev_, Ib_);

DECLARE_BINARY_OINST(Nq_, I__);

DECLARE_BINARY_OINST(Udq, I__);

DECLARE_BINARY_OINST(Vdq, I__);

#endif /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCDECODE_FORMS_H__ */
