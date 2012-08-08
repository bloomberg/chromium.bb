/* Copyright (c) 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * ncdecodex87.c - Handles x87 instructions.
 *
 * Note: These instructions are derived from tabler A-10 in Appendx
 * section A.2.7 - x87 Encodings in AMD document 24594-Rev.3.14-September
 * 2007, "AMD64 Architecture Programmer's manual Volume 3: General-Purpose
 * and System Instructions".
 *
 * Note: This modeling code doesn't handle several aspects of floating point
 * operations. This includes:
 *
 * 1) Doesn't model condition flags.
 * 2) Doesn't model floating point stack adjustments.
 * 3) Doesn't model all differences in size of pointed to memory ($Mf is used
 *    in such cases).
 *
 * Note: %st0 and %st1 have been inserted and made explicit, when necessary
 * to match the disassembler xed.
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_forms.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_tablegen.h"

void NaClDefX87Insts(struct NaClSymbolTable* st) {
  /* See Table A-10 in AMD manual for source of definitions.
   * Note: Added register ST0 when used in instructions, but
   * not explicitly listed in table A-10. This information
   * was derived from xed tests.
   */

  /* Define D8 x87 instructions. */

  NaClDefine("d8/0: Fadd %st0, $Md", NACLi_X87, st, Binary);
  NaClDefine("d8/1: Fmul %st0, $Md", NACLi_X87, st, Binary);
  NaClDefine("d8/2: Fcom %st0, $Md", NACLi_X87, st, Compare);
  NaClDefine("d8/3: Fcomp %st0, $Md", NACLi_X87, st, Compare);
  NaClDefine("d8/4: Fsub %st0, $Md", NACLi_X87, st, Binary);
  NaClDefine("d8/5: Fsubr %st0, $Md", NACLi_X87, st, Binary);
  NaClDefine("d8/6: Fdiv %st0, $Md", NACLi_X87, st, Binary);
  NaClDefine("d8/7: Fdivr %st0, $Md", NACLi_X87, st, Binary);
  NaClDefIter("d8C0+@i: Fadd %st0, %st@i", 0, 7, NACLi_X87, st, Binary);
  NaClDefIter("d8C8+@i: Fmul %st0, %st@i", 0, 7, NACLi_X87, st, Binary);
  NaClDefIter("d8d0+@i: Fcom %st0, %st@i", 0, 7, NACLi_X87, st, Compare);
  NaClDefIter("d8d8+@i: Fcomp %st0, %st@i", 0, 7, NACLi_X87, st, Compare);
  NaClDefIter("d8e0+@i: Fsub %st0, %st@i", 0, 7, NACLi_X87, st, Binary);
  NaClDefIter("d8e8+@i: Fsubr %st0, %st@i", 0, 7, NACLi_X87, st, Binary);
  NaClDefIter("d8f0+@i: Fdiv %st0, %st@i", 0, 7, NACLi_X87, st, Binary);
  NaClDefIter("d8f8+@i: Fdivr %st0, %st@i", 0, 7, NACLi_X87, st, Binary);

  /* Define D9 x87 instructions. */

  NaClDefine("d9/0: Fld %st0, $Md", NACLi_X87, st, Move);
  NaClDefine("d9/1: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("d9/2: Fst $Md, %st0", NACLi_X87, st, Move);
  NaClDefine("d9/3: Fstp $Md, %st0", NACLi_X87, st, Move);
  NaClDefine("d9/4: Fldenv $Mf", NACLi_X87, st, Uses);
  NaClDefine("d9/5: Fldcw $Mw", NACLi_X87, st, Uses);
  NaClDefine("d9/6: Fnstenv $Mf", NACLi_X87, st, UnarySet);
  NaClDefine("d9/7: Fnstcw $Mw", NACLi_X87, st, UnarySet);
  NaClDefIter("d9c0+@i: Fld %st0, %st@i", 0, 7, NACLi_X87, st, Move);
  NaClDefIter("d9c8+@i: Fxch %st0, %st@i", 0, 7, NACLi_X87, st, Exchange);
  NaClDefine("d9d0: Fnop", NACLi_X87, st, Other);
  NaClDefIter("d9d0+@i: Invalid", 1, 7, NACLi_INVALID, st, Other);
  NaClDefIter("d9d8+@i: Invalid", 0, 7, NACLi_INVALID, st, Other);
  NaClDefine("d9e0: Fchs %st0", NACLi_X87, st, UnaryUpdate);
  NaClDefine("d9e1: Fabs %st0", NACLi_X87, st, UnaryUpdate);
  NaClDefIter("d9e0+@i: Invalid", 2, 3, NACLi_INVALID, st, Other);
  NaClDefine("d9e4: Ftst %st0", NACLi_X87, st, Uses);
  NaClDefine("d9e5: Fxam %st0", NACLi_X87, st, Uses);
  NaClDefIter("d9e0+@i: Invalid", 6, 7, NACLi_INVALID, st, Other);
  NaClDefine("d9e8: Fld1 %st0", NACLi_X87, st, UnaryUpdate);
  NaClDefine("d9e9: Fldl2t %st0", NACLi_X87, st, UnaryUpdate);
  NaClDefine("d9ea: Fldl2e %st0", NACLi_X87, st, UnaryUpdate);
  NaClDefine("d9eb: Fldpi %st0", NACLi_X87, st, UnaryUpdate);
  NaClDefine("d9ec: Fldlg2 %st0", NACLi_X87, st, UnaryUpdate);
  NaClDefine("d9ed: Fldln2 %st0", NACLi_X87, st, UnaryUpdate);
  NaClDefine("d9ee: Fldz %st0", NACLi_X87, st, UnaryUpdate);
  NaClDefine("d9ef: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("d9f0: F2xm1 %st0", NACLi_X87, st, UnaryUpdate);
  NaClDefine("d9f1: Fyl2x %st0 %st1", NACLi_X87, st, Binary);
  NaClDefine("d9f2: Fptan %st0 %st1", NACLi_X87, st, Move);
  NaClDefine("d9f3: Fpatan %st0, %st1", NACLi_X87, st, Binary);
  NaClDefine("d9f4: Fxtract %st0, %st1", NACLi_X87, st, Move);
  NaClDefine("d9f5: Fprem1 %st0, %st1", NACLi_X87, st, Binary);
  NaClDefine("d9f6: Fdecstp", NACLi_X87, st, Other);
  NaClDefine("d9f7: Fincstp", NACLi_X87, st, Other);
  NaClDefine("d9f8: Fprem %st0, %st1", NACLi_X87, st, Binary);
  NaClDefine("d9f9: Fyl2xp1 %st0, %st1", NACLi_X87, st, Binary);
  NaClDefine("d9fa: Fsqrt %st0", NACLi_X87, st, UnaryUpdate);
  /* NaClDefine("d9fb: Fsincos %st0, %st1", NACLi_X87, st, Move); */
  NaClDefine("d9fb: Fsincos %st0, %st1", NACLi_X87_FSINCOS, st, Move);
  NaClDefine("d9fc: Frndint %st0", NACLi_X87, st, UnaryUpdate);
  NaClDefine("d9fd: Fscale %st0, %st1", NACLi_X87, st, Binary);
  NaClDefine("d9fe: Fsin %st0", NACLi_X87, st, UnaryUpdate);
  NaClDefine("d9ff: Fcos %st0", NACLi_X87, st, UnaryUpdate);

  /* Define DA x87 instructions. */

  NaClDefine("da/0: Fiadd %st0, $Md", NACLi_X87, st, Binary);
  NaClDefine("da/1: Fimul %st0, $Md", NACLi_X87, st, Binary);
  NaClDefine("da/2: Ficom %st0, $Md", NACLi_X87, st, Binary);
  NaClDefine("da/3: Ficomp %st0, $Md", NACLi_X87, st, Binary);
  NaClDefine("da/4: Fisub %st0, $Md", NACLi_X87, st, Binary);
  NaClDefine("da/5: Fisubr %st0, $Md", NACLi_X87, st, Binary);
  NaClDefine("da/6: Fidiv %st0, $Md", NACLi_X87, st, Binary);
  NaClDefine("da/7: Fidivr %st0, $Md", NACLi_X87, st, Binary);
  NaClDefIter("dac0+@i: Fcmovb %st0, %st@i", 0, 7, NACLi_X87, st, Binary);
  NaClDefIter("dac8+@i: Fcmove %st0, %st@i", 0, 7, NACLi_X87, st, Binary);
  NaClDefIter("dad0+@i: Fcmovbe %st0, %st@i", 0, 7, NACLi_X87, st, Binary);
  NaClDefIter("dad8+@i: Fcmovu %st0, %st@i", 0, 7, NACLi_X87, st, Binary);
  NaClDefIter("dae0+@i: Invalid", 0, 7, NACLi_INVALID, st, Other);
  NaClDefine("dae8: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("dae9: Fucompp %st0, %st1", NACLi_X87, st, Compare);
  NaClDefIter("dae8+@i: Invalid", 2, 7, NACLi_INVALID, st, Other);
  NaClDefIter("daf0+@i: Invalid", 0, 7, NACLi_INVALID, st, Other);
  NaClDefIter("daf8+@i: Invalid", 0, 7, NACLi_INVALID, st, Other);

  /* Define DB x87 instructions. */

  NaClDefine("db/0: Fild %st0, $Md", NACLi_X87, st, Move);
  NaClDefine("db/1: Fisttp $Md, %st0", NACLi_X87, st, Move);
  NaClDefine("db/2: Fist $Md, %st0", NACLi_X87, st, Move);
  NaClDefine("db/3: Fistp $Md, %st0", NACLi_X87, st, Move);
  NaClDefine("db/4: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("db/5: Fld %st0, $Mf", NACLi_X87, st, Move);
  NaClDefine("db/6: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("db/7: Fstp $Mf, %st0", NACLi_X87, st, Move);
  NaClDefIter("dbc0+@i: Fcmovnb %st0, %st@i", 0, 7, NACLi_X87, st, Binary);
  NaClDefIter("dbc8+@i: Fcmovne %st0, %st@i", 0, 7, NACLi_X87, st, Binary);
  NaClDefIter("dbd0+@i: Fcmovnbe %st0, %st@i", 0, 7, NACLi_X87, st, Binary);
  NaClDefIter("dbd8+@i: Fcmovnu %st0, %st@i", 0, 7, NACLi_X87, st, Binary);
  NaClDefIter("dbe0+@i: Invalid", 0, 1, NACLi_INVALID, st, Other);
  NaClDefine("dbe2: Fnclex", NACLi_X87, st, Other);
  NaClDefine("dbe3: Fninit", NACLi_X87, st, Other);
  NaClDefIter("dbe0+@i: Invalid", 4, 7, NACLi_INVALID, st, Other);
  NaClDefIter("dbe8+@i: Fucomi %st0, %st@i", 0, 7, NACLi_X87, st, Compare);
  NaClDefIter("dbf0+@i: Fcomi %st0, %st@i", 0, 7, NACLi_X87, st, Compare);
  NaClDefIter("dbf8+@i: Invalid", NACLi_INVALID, 0, 7, st, Other);

  /* Define DC x87 instructions. */

  NaClDefine("dc/0: Fadd %st0, $Mq", NACLi_X87, st, Binary);
  NaClDefine("dc/1: Fmul %st0, $Mq", NACLi_X87, st , Binary);
  NaClDefine("dc/2: Fcom %st0, $Mq", NACLi_X87, st, Compare);
  NaClDefine("dc/3: Fcomp %st0, $Mq", NACLi_X87, st, Compare);
  NaClDefine("dc/4: Fsub %st0, $Mq", NACLi_X87, st, Binary);
  NaClDefine("dc/5: Fsubr %st0, $Mq", NACLi_X87, st, Binary);
  NaClDefine("dc/6: Fdiv %st0, $Mq", NACLi_X87, st, Binary);
  NaClDefine("dc/7: Fdivr %st0, $Mq", NACLi_X87, st, Binary);
  NaClDefIter("dcc0+@i: Fadd %st@i, %st0", 0, 7, NACLi_X87, st, Binary);
  NaClDefIter("dcc8+@i: Fmul %st@i, %st0", 0, 7, NACLi_X87, st, Binary);
  NaClDefIter("dcd0+@i: Invalid", 0, 7, NACLi_INVALID, st, Other);
  NaClDefIter("dcd8+@i: Invalid", 0, 7, NACLi_INVALID, st, Other);
  NaClDefIter("dce0+@i: Fsubr %st@i, %st0", 0, 7, NACLi_X87, st, Binary);
  NaClDefIter("dce8+@i: Fsub %st@i, %st0", 0, 7, NACLi_X87, st, Binary);
  NaClDefIter("dcf0+@i: Fdivr %st@i, %st0", 0, 7, NACLi_X87, st, Binary);
  NaClDefIter("dcf8+@i: Fdiv %st@i, %st0", 0, 7, NACLi_X87, st, Binary);

  /* Define DD x87 instructions. */

  NaClDefine("dd/0: Fld %st0, $Mq", NACLi_X87, st, Move);
  NaClDefine("dd/1: Fisttp $Mq, %st0", NACLi_X87, st, Move);
  NaClDefine("dd/2: Fst $Mq, %st0", NACLi_X87, st, Move);
  NaClDefine("dd/3: Fstp $Mq, %st0", NACLi_X87, st, Move);
  NaClDefine("dd/4: Frstor $Mf", NACLi_X87, st, Uses);
  NaClDefine("dd/5: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("dd/6: Fnsave $Mf", NACLi_X87, st, UnarySet);
  NaClDefine("dd/7: Fnstsw $Mw", NACLi_X87, st, UnarySet);
  NaClDefIter("ddc0+@i: Ffree %st@i", 0, 7, NACLi_X87, st, Other);
  NaClDefIter("ddc8+@i: Invalid", 0, 7, NACLi_INVALID, st, Other);
  NaClDefIter("ddd0+@i: Fst %st@i, %st0", 0, 7, NACLi_X87, st, Move);
  NaClDefIter("ddd8+@i: Fstp %st@i %st0", 0, 7, NACLi_X87, st, Move);
  NaClDefIter("dde0+@i: Fucom %st0, %st@i", 0, 7, NACLi_X87, st , Compare);
  NaClDefIter("dde8+@i: Fucomp %st0, %st@i", 0, 7, NACLi_X87, st, Compare);
  NaClDefIter("ddf0+@i: Invalid", 0, 7, NACLi_INVALID, st, Other);
  NaClDefIter("ddf8+@i: Invalid", 0, 7, NACLi_INVALID, st, Other);


  /* Define DE x87 instructions. */

  NaClDefine("de/0: Fiadd %st0, $Mw", NACLi_X87, st, Binary);
  NaClDefine("de/1: Fimul %st0, $Mw", NACLi_X87, st, Binary);
  NaClDefine("de/2: Ficom %st0, $Mw", NACLi_X87, st, Compare);
  NaClDefine("de/3: Ficomp %st0, $Mw", NACLi_X87, st, Compare);
  NaClDefine("de/4: Fisub %st0, $Mw", NACLi_X87, st, Binary);
  NaClDefine("de/5: Fisubr %st0, $Mw", NACLi_X87, st, Binary);
  NaClDefine("de/6: Fidiv %st0, $Mw", NACLi_X87, st, Binary);
  NaClDefine("de/7: Fidivr %st0, $Mw", NACLi_X87, st, Binary);
  NaClDefIter("dec0+@i: Faddp %st@i, %st0", 0, 7, NACLi_X87, st, Binary);
  NaClDefIter("dec8+@i: Fmulp %st@i, %st0", 0, 7, NACLi_X87, st, Binary);
  NaClDefIter("ded0+@i: Invalid", 0, 7, NACLi_INVALID, st, Other);
  NaClDefine("ded8: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("ded9: Fcompp %st0, %st1", NACLi_X87, st, Compare);
  NaClDefIter("ded8+@i: Invalid", 2, 7, NACLi_INVALID, st, Other);
  NaClDefIter("dee0+@i: Fsubrp %st@i, %st0", 0, 7, NACLi_X87, st, Binary);
  NaClDefIter("dee8+@i: Fsubp %st@i, %st0", 0, 7, NACLi_X87, st, Binary);
  NaClDefIter("def0+@i: Fdivrp %st@i, %st0", 0, 7, NACLi_X87, st, Binary);
  NaClDefIter("def8+@i: Fdivp %st@i, %st0", 0, 7, NACLi_X87, st, Binary);

  /* Define DF x87 instructions. */
  NaClDefine("df/0: Fild %st0, $Mw", NACLi_X87, st , Move);
  NaClDefine("df/1: Fisttp $Mw, %st0", NACLi_X87, st, Move);
  NaClDefine("df/2: Fist $Mw, %st0", NACLi_X87, st, Move);
  NaClDefine("df/3: Fistp $Mw, %st0", NACLi_X87, st, Move);
  NaClDefine("df/4: Fbld %st0, $Mf", NACLi_X87, st, Move);
  NaClDefine("df/5: Fild %st0, $Mf", NACLi_X87, st, Move);
  NaClDefine("df/6: Fbstp $Mf, %st0", NACLi_X87, st , Move);
  NaClDefine("df/7: Fistp $Mf, %st0", NACLi_X87, st, Move);
  NaClDefIter("dfc0+@i: Invalid", 0, 7, NACLi_X87, st, Other);
  NaClDefIter("dfc8+@i: Invalid", 0, 7, NACLi_X87, st, Other);
  NaClDefIter("dfd0+@i: Invalid", 0, 7, NACLi_X87, st, Other);
  NaClDefIter("dfd8+@i: Invalid", 0, 7, NACLi_X87, st, Other);
  NaClDefine("dfe0: Fnstsw %ax", NACLi_X87, st, UnarySet);
  NaClDefIter("dfe0+@i: Invalid", 1, 7, NACLi_INVALID, st, Other);
  NaClDefIter("dfe8+@i: Fucomip %st0, %st@i", 0, 7, NACLi_X87, st, Compare);
  NaClDefIter("dff0+@i: Fcomip %st0, %st@i", 0, 7, NACLi_X87, st, Compare);
  NaClDefIter("dff8+@i: Invalid", 0, 7, NACLi_INVALID, st, Other);

  /* todo(karl) What about "9b db e2 Fclex" ? */
  /* todo(karl) What about "9b db e3 finit" ? */
  /* todo(karl) What about "9b d9 /6 Fstenv" ? */
  /* todo(karl) What about "9b d9 /7 Fstcw" ? */
  /* todo(karl) What about "9b dd /6 Fsave" ? */
  /* todo(karl) What about "9b dd /7 Fstsw" ? */
  /* todo(karl) What about "9b df e0 Fstsw" ? */
}
