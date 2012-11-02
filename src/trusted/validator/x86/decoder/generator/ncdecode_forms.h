/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Set of predefined instruction forms (via procedure calls), providing
 * a more concise way of specifying opcodes.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NCDECODE_FORMS_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NCDECODE_FORMS_H__

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/trusted/validator/x86/decoder/ncopcode_desc.h"

struct NaClSymbolTable;

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
  UnaryUpdate, /* Dest := f(Dest) for some f. */
  /* The following are for categorizing operations with 2 or more operands. */
  Move,       /* Dest := f(Source) for some f. */
  O2Move,     /* Dest1, Dest2 = f(source) for some f. */
  Binary,     /* Dest := f(Dest, Source) for some f. */
  O2Binary,   /* Dest1,Dest2 = f(Dest2, Source) for some f. */
  Nary,       /* Dest := f(dest, source1, ..., sourcen) for some f. */
  O1Nary,     /* Dest := f(source1, ..., sourcen) for some f. */
  O3Nary,     /* Dest1,Dest2,Dest3 = f(source1, ..., sourcen) for some f. */
  Compare,    /* Sets flag using f(Dest, Source). The value of Dest is not
               * modified.
               */
  Exchange,   /* Dest := f(Dest, Source) for some f, and
               * Source := g(Dest, Source) for some g.
               */
  Push,       /* Implicit first (stack) argument is updated, and the
               * value of the Dest is not modified.
               */
  Pop,        /* Implicit first (stack) argument is updated, and
               * dest := f() for some f (i.e. f gets the value on
               * top of the stack).
               */
  Call,       /* Implicit ip first argument that is updated. Stack second
               * argument that is updated. Third argument is used.
               */
  SysCall,    /* Implicit ip first argument that is updated, Implicit register
               * second argument that is set.
               */
  SysJump,    /* First four arguments are set (eip, esp, cs, ss). Remaining
               * (if any) are used.
               */
  Return,     /* Implicit ip first argument that is set. Stack second
               * argument that is updated. Third argument, if given, is used.
               */
  SysRet,     /* Implicit ip first argument that is set. Implicit register
               * second argument that is used.
               */
  Jump,       /* Implicit first (IP) argument is updated to the
               * value of the Dest argument.
               */
  Uses,       /* All arguments are uses. */
  Sets,       /* All arguments are set. */
  Lea,        /* Address calculation, and hence, operand 2 is neither used
               * nor set.
               */
  Cpuid,      /* Sets all, uses starting with third. */
  Other,      /* No implicit set/use implications. */
} NaClInstCat;

/* Defines the maximum length of an opcode sequence descriptor (see
 * comment for typedef NaClOpcodeSeq). Note: two extra bytes have been added
 * for SL(x) and END_OPCODE_SEQ entries.
 */
#define NACL_OPCODE_SEQ_SIZE (NACL_MAX_OPCODE_BYTES + 2)

/* Models an opcode sequence. Used by NaClInInstructionSet to describe
 * an instruction implemented by a sequence of bytes. Macro SL(N) is used
 * to describe an additional value N, which appears in the modrm mod field.
 * Macro END_OPCODE_SEQ is an placeholder, ignore value, defining the end of the
 * opcode sequence.
 *
 * 0..256         => Opcode byte.
 * PR(N)          => prefix byte value N.
 * SL(N)          => /N
 * END_OPCODE_SEQ => Not part of prefix.
 */
typedef int16_t NaClOpcodeSeq[NACL_OPCODE_SEQ_SIZE];

/* Value denoting the end of an opcode sequence (descriptor). */
#define END_OPCODE_SEQ 512

/* Define value in modrm (i.e. /n in opcode sequence). */
#define SL(n) (-((n) + 1))

/* Define prefix value for opcode sequence. */
#define PR(n) SL(n) - END_OPCODE_SEQ

/* Model an instruction by its mnemonic and opcode sequence. */
typedef struct NaClNameOpcodeSeq {
  NaClMnemonic name;
  NaClOpcodeSeq opcode_seq;
} NaClNameOpcodeSeq;

/* Returns true iff the current instruction has one of the given mnemonic names,
 * or is defined by one of the name and opcode sequences. Note: It is safe to
 * pass NULL for names or name_and_opcode_seq, if the corresponding size
 * parameter is zero.
 */
Bool NaClInInstructionSet(const NaClMnemonic* names,
                          size_t names_size,
                          const NaClNameOpcodeSeq* name_and_opcode_seq,
                          size_t name_and_opcode_seq_size);

/* Model of a an operand processing function. */
typedef void (*NaClDefOperand)(void);

/***************************************************************************
 * This section is the new API for defining instructions. It uses a string,
 * describing the instruction to model. In addition, a symbol table is passed
 * in to define possible substitutions.
 *
 * The string defining the instruction is called an "opcode description string".
 *
 *    Examples: The following are some examples of opcode description strings.
 *
 *   "06: Push {%@sp}, %es" - Defines (opcode 06) that pushes register es
 *   "07: Pop  {%@sp}, %es" - Defines (opcode 07) that pops into register es.
 *   "69/r: Imul $Gv, $Ev, $Iz" - Defines (opcode 69) a signed multiply.
 *   "0fba/7: Btc $Ev, $Ib" - Defines (opcode 0f ba, with opcode extension
 *            7 in the modrm mod field) a bit test and complement.
 *   "90+@i: Xchg $r8v, $rAX" - Defines (opcode 90+i) exchange register/memory
 *            with register.
 *
 * A (symbol table) substitution is defined as follows:
 *
 *    (1) It begins with the character '@';
 *    (2) Its name is an alphanumeric sequence; and
 *    (3) The name is terminated by a character in the charset ' :+/{}'.
 *
 * The general form of an opcode description string is a sequence of
 * hex values defining the opcode prefix, and the opcode byte. This
 * sequence of values must be terminated with a colon (:). No spaces
 * are allowed in this sequence.
 *
 * If the instruction uses the modrm byte, a '/r' must immediately follow
 * the sequence of hex values (and must appear before the colon).
 *
 * If the instruction is continued in the modrm mod field (i.e. a value 0..7),
 * the characters /N (where N is in 0..7) must immediately follow the sequence
 * of hex values (and must appear before the colon).
 *
 * If the instruction encodes a register value as part of the opcode byte,
 * the value of the register defined is the string '+R' (where R is in 0..7),
 * and must immediately follow the sequence of hex values (and must appear
 * before the colon).

 * Note: If the instruction uses an operand print form that uses the modrm
 * value (such as $E or $G), then it is not necessary to add the
 * /r suffix to the sequence of hex values.
 *
 * After the colon, the mnemonic name of the instruction must appear. An
 * arbitrary number of spaces can appear between the colon, and the mnemonic
 * name. The mnemonic name is then followed by zero or more operands.
 * Each operand can be separated by an arbitrary sequence of spaces and/or
 * commas.
 *
 * Each operand specifies a register and/or memory address. An operand
 * may not contain spaces.
 *
 * If the operand is implicit (i.e. should not appear when printing a
 * decoded instruction), it should be enclosed in curly braces. In general,
 * we put implicit operands first, but there are no rules defining where an
 * implicit operand may appear.
 *
 * A register begins with the character '%', and is followed by its name.
 * Register names are case insensitive. Legal values are any operand kind
 * defined in ncopcode_operand_kind.enum that begins with the text 'Reg'.
 *
 * A print form begins with the character '$", and is followed by a name.
 * Print forms are, in general, defined by Appendex section A.1 - Opcode-Syntax
 * Notation in AMD document 24594-Rev.3.14-September 2007, "AMD64 Architecture
 * Programmer's manual Volume 3: General-Purpose and System Instructions".
 * Exceptions are made for descriptions used in that appendex, but are
 * not documented in this section. For clarity, the rules are explicitly
 * defined as follows: A print form consists of a FORM, followed by
 * a SIZE specification.
 *
 * Valid FORMs are:
 *   A - Far pointer is encoded in the instruction.
 *   C - Control register specified by the ModRM reg field.
 *   D - Debug register specified by the ModRM reg field.
 *   E - General purpose register or memory operand specified by the ModRm
 *       byte. Memory addresses can be computed from a segment register,
 *       SIB byte, and/or displacement.
 *   F - rFLAGS register.
 *   G - General purpose register specified by the ModRm reg field.
 *   I - Immediate value.
 *   J - The instruction includes a relative offset that is added to the rIP
 *       register.
 *   M - A memory operand specified by the ModRM byte.
 *   O - The offset of an operand is encoded in the instruction. There is no
 *       ModRm byte in the instruction. Complex addressing using the SIB byte
 *       cannot be done.
 *   P - 64-bit MMX register specified by the ModRM reg field.
 *   PR - 64 bit MMX register specified by the ModRM r/m field. The ModRM mod
 *       field must be 11b.
 *   Q - 64 bit MMX register or memory operand specified by the ModRM byte.
 *       Memory addresses can be computed from a segment register, SIB byte,
 *       and/or displacement.
 *   R - General purpose register specified by the ModRM r/m field. The ModRM
 *       mod field must be 11b.
 *   S - Segment register specified by the ModRM reg field.
 *   U - The R/M field of the ModR/M byte selects a 128-bit XMM register.
 *   V - 128-bit XMM register specified by the ModRM reg field.
 *   VR - 128-bit XMM register specified by the ModRM r/m field. The ModRM mod
 *       field must be 11b.
 *   W - 128 Xmm register or memory operand specified by the ModRm Byte. Memory
 *       addresses can be computed from a segment register, SIB byte, and/or
 *       displacement.
 *   X - A memory operand addressed by the DS.rSI registers. Used in string
 *       instructions.
 *   Y - A memory operand addressed by the ES.rDI registers. Used in string
 *       instructions.
 *   Z - A memory operand addressed by the DS.rDI registers. Used in maskmov
 *       instructions.
 *   r8 - The 8 registers rAX, rCX, rDX, rBX, rSP, rBP, rSI, rDI, and the
 *        optional registers r8-r15 if REX.b is set, based on the register value
 *        embedded in the opcode.
 *   SG - segment address defined by a G expression and the segment register in
 *        the corresponding mnemonic (lds, les, lfs, lgs, lss).
 *   rAX - The register AX, EAX, or RAX, depending on SIZE.
 *   rBP - The register BP, EBP, or RBP, depending on SIZE.
 *   rBX - The register BX, EBX, or RBX, depending on SIZE.
 *   rCX - The register CX, ECX, or RCX, depending on SIZE.
 *   rDI - The register DI, EDI, or RDI, depending on SIZE.
 *   rDX - The register DX, EDX, or RDX, depending on SIZE.
 *   rSI - The register SI, ESI, or RSI, depending on SIZE.
 *   rSP - The register SP, ESP, or RSP, depending on SIZE.
 *
 * Note: r8 is not in the manual cited above. It has been added to deal with
 * instructions with an embedded register in the opcode. In such cases, this
 * value allows a single defining call to be used (within a for loop),
 * rather than writing eight separate rules (one for each possible register
 * value).
 *
 * Note: SG is also not in the manual cited above. It has been added to deal
 * with the instructions lds, les, lfs, lgs, and lss, which generate a
 * segment address from a General purpose register specified in the ModRm reg
 * field.
 *
 * Note: Z is also not in the manual cited above. It has been added to deal with
 * the implicit argument of maskmov instructions.
 *
 * Valid SIZEs are:
 *   a - Two 16-bit or 32-bit memory operands, depending on the effective
 *       operand size. Used in the BOUND instruction.
 *   b - A byte, irrespective of the effective operand size.
 *   d - A doubleword (32-bits), irrespective of the effective operand size.
 *   dq - A double-quadword (128 bits), irrespective of the effective operand
 *       size.
 *   p - A 32-bit or 48-bit far pointer, depending on the effective operand
 *       size.
 *   pd - A 128-bit double-precision floating point vector operand (packed
 *       double).
 *   pi - A 64-bit MMX operand (packed integer).
 *   ps - A 128-bit single precision floating point vector operand (packed
 *        single).
 *   q - A quadword, irrespective of the effective operand size.
 *   s - A 6-byte or 10-byte pseudo-descriptor.
 *   sd - A scalar double-precision floating point operand (scalar double).
 *   si - A scalar doubleword (32-bit) integer operand (scalar integer).
 *   ss - A scalar single-precision floating-point operand (scalar single).
 *   w - A word, irrespective of the effective operand size.
 *   v - A word, doubleword, or quadword, depending on the effective operand
 *       size.
 *   va - A word, doubleword, or quadword, depending on the effective address
 *       size.
 *   vw - A word only when the effective operand size matches.
 *   vd - A doubleword only when the effective operand size matches.
 *   vq - A quadword only when the effective operand size matches.
 *   w - A word, irrespective of the effective operand size.
 *   z - A word if the effective operand size is 16 bits, or a doubleword
 *       if the effective operand size is 32 or 64 bits.
 *   zw - A word only when the effective operand size matches.
 *   zd - A doubleword only when the effective operand size is 32 or 64 bits.
 *   f - A memory access (of small size, i.e. less than 100 bytes),
 *       irrespective of the operand size (as modified by the prefix 66,
         and the Rex.w prefix). Should only be used with $M arguments.
 *       When this size modifier $Mf is used (unlike $M which allows
 *       prefix 66), prefix 66 is illegal.
 *       Note: When $Mf is used, the (small) size differences are not
 *       important for the validator. Hence, it doesn't matter if we are
 *       more accurate.
 *
 * Note: vw, vd, vq, zw, and zd are not in the manual cited above. However,
 * they have been added so that sub-variants of an v/z instruction (not
 * specified in the manual) can be specified.
 *
 * In addition, this code adds the following special print forms:
 *    One - The literal constant 1.
 *
 * Note: The AMD manual uses some slash notations (such as d/q) which isn't
 * explicitly defined. In general, we allow such notation as specified in
 * the AMD manual. Depending on the use, it can mean any of the following:
 *   (1) In 32-bit mode, d is used. In 64-bit mode, q is used.
 *   (2) only 32-bit or 64-bit values are allowed.
 * In addition, when the nmemonic name changes based on which value is chosen
 * in d/q, we use d/q/d to denote the 32-bit case, and d/q/q to denote the
 * 64 bit case.
 *
 * Because some instructions may need to add flags and/or additional operands
 * outside the string context, instructions are modeled using a pair of calls
 * (i.e. a Begin and End form). The Begin form starts defining the instruction,
 * and the End form completes and installs the modeled instruction. Any
 * additional model changes for the instruction being defined should
 * appear between these call pairs.
 *
 * For instructions not needing to do special touchups, a simpler Define form
 * exists that simply dispatches calls to the corresponding Begin and End forms.
 ***************************************************************************/

/* Defines target machine.*/
typedef enum {
  T32,   /* 32 only. */
  T64,   /* 64 only. */
  Tall,  /* both 32 and 64. */
} NaClTargetPlatform;

/* Defines the beginning of the modeling of a platform instruction.
 * Parameters are:
 *   platform - The platform(s) the instruction applies to.
 *   desc - the opcode description string.
 *   insttype - The category of the instruction (defines the effects of CPUID).
 *   st - The symbol table to use while defining the instruction.
 */
void NaClBegDefPlatform(NaClTargetPlatform platform,
                        const char* desc, NaClInstType insttype,
                        struct NaClSymbolTable* st);

/* Defines the beginning of the modeling of both a x86-32 and x86-64
 * instruction.
 * Parameters are:
 *   desc - the opcode description string.
 *   insttype - The category of the instruction (defines the effects of CPUID).
 *   st - The symbol table to use while defining the instruction.
 */
void NaClBegDef(const char* desc, NaClInstType insttype,
                struct NaClSymbolTable* st);

/* Defines the beginning of the modeling of a x86-32 instruction without
 * an equivalent x86-64 version.
 * Parameters are:
 *   desc - the opcode description string.
 *   insttype - The category of the instruction (defines the effects of CPUID).
 *   st - The symbol table to use while defining the instruction.
 */
void NaClBegD32(const char* desc, NaClInstType insttype,
                struct NaClSymbolTable* st);

/* Defines the beginning of the modeling of a x86-64 instruction without
 * an equivalent x86-32 version.
 * Parameters are:
 *   desc - the opcode description string.
 *   insttype - The category of the instruction (defines the effects of CPUID).
 *   st - The symbol table to use while defining the instruction.
 */
void NaClBegD64(const char* desc, NaClInstType insttype,
                struct NaClSymbolTable* st);

/* Defines the end of the modeling of an instruction. Must be paired with
 * a call to NaClBegDef, NaClBegD32, or NaClBegD64.
 * Parameters are:
 *   icat - The set/use categorization for the instruction being defined.
 */
void NaClEndDef(NaClInstCat icat);

/* Defines a platform instruction, using dispatching
 * calls to NaClBegDefPlatform and NaClEndDef.
 * Parameters are:
 *   platform - the platform(s) the instruction applies to.
 *   desc - the opcode description string.
 *   insttype - The category of the instruction (defines the effects of CPUID).
 *   st - The symbol table to use while defining the instruction.
 *   icat - The set/use categorization for the instruction being defined.
 */
void NaClDefinePlatform(NaClTargetPlatform platform,
                        const char* desc, NaClInstType insttype,
                        struct NaClSymbolTable* st, NaClInstCat cat);

/* Defines both a x86-32 and x86-64 instruction, using dispatching
 * calls to NaClBegDef and NaClEndDef.
 * Parameters are:
 *   desc - the opcode description string.
 *   insttype - The category of the instruction (defines the effects of CPUID).
 *   st - The symbol table to use while defining the instruction.
 *   icat - The set/use categorization for the instruction being defined.
 */
void NaClDefine(const char* desc, NaClInstType insttype,
                struct NaClSymbolTable* st, NaClInstCat cat);

/* Defines a x86-32 instruction without an equivalent x86-64 version, using
 * dispatching calls to NaClBegD32 and NaClEndDef.
 * Parameters are:
 *   desc - the opcode description string.
 *   insttype - The category of the instruction (defines the effects of CPUID).
 *   st - The symbol table to use while defining the instruction.
 *   icat - The set/use categorization for the instruction being defined.
 */
void NaClDef_32(const char* desc, NaClInstType insttype,
                struct NaClSymbolTable* st, NaClInstCat cat);

/* Defines a x86-64 instruction without an equivalent x86-32 version, using
 * dispatching calls to NaClBegD32 and NaClEndDef.
 * Parameters are:
 *   desc - the opcode description string.
 *   insttype - The category of the instruction (defines the effects of CPUID).
 *   st - The symbol table to use while defining the instruction.
 *   icat - The set/use categorization for the instruction being defined.
 */
void NaClDef_64(const char* desc, NaClInstType insttype,
                struct NaClSymbolTable* st, NaClInstCat cat);


/* Defines a set of instructions, for all values of min <= i <= max (bound
 * in a local symbol table), using calls to NaClDefine on the remaining
 * arguments. In addition, opcodes of the form "xx+@i:", within the description
 * string are automatically added to generate the opcode value xx+i.
 * In addition, the value of min and max must be between 0 and 7.
 * Parameters are:
 *   desc - the opcode description string.
 *   min - The starting value to iterate i on.
 *   max - The ending value to iterate i on.
 *   insttype - The category of the instruction (defines the effects of CPUID).
 *   st - The symbol table to use while defining the instruction.
 *   icat - The set/use categorization for the instruction being defined.
 */
void NaClDefIter(const char* desc, int min, int max,
                 NaClInstType insttype, struct NaClSymbolTable* st,
                 NaClInstCat cat);

/* Defines a set of instructions, for all values of min <= reg <= max (bound
 * in a local symbol), using calls to NaClDefine on the remaining arguments.
 * In addition, the value of min and max must be between 0 and 255. Typically
 * used to generate register values that are part of the opcode.
 * Parameters are:
 *   desc - the opcode description string.
 *   min - The starting value to iterate reg on.
 *   max - The ending value to iterate reg on.
 *   insttype - The category of the instruction (defines the effects of CPUID).
 *   st - The symbol table to use while defining the instruction.
 *   icat - The set/use categorization for the instruction being defined.
 */
void NaClDefReg(const char* desc, int min, int max,
                NaClInstType insttype, struct NaClSymbolTable* st,
                NaClInstCat cat);


#endif /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NCDECODE_FORMS_H__ */
