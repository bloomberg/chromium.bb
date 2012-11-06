/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Defines two byte opcodes beginning with OF. Taken from:
 * (1) Tables A-3 and A-4 of appendix "A2 - Opcode Encodings", in AMD
 *     document 24594-Rev.3.14-September 2007: "AMD64 Architecture
 *     Programmer's manual Volume 3: General-Purpose and System Instructions".
 * (2) Table A-4 of "appendix A.3", in Intel Document 253667-030US (March 2009):
 *     "Intel 64 and IA-32 Architectures Software Developer's Manual,
 *     Volume 2g: Instruction Set Reference, N-Z."
 *
 * Note: The SSE instructions that begin with 0F are not defined here. Look
 * at ncdecode_sse.c for the definitions of SSE instruction.
 *
 * Note: Invalid etries with f2 (REPNE) and f3 (REP) can be ommitted in
 * the tables, because the corresponding entry without that prefix will
 * be explicitly rejected (unless it is one of the special instructions
 * that allow such prefixes).
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_forms.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_tablegen.h"

static void NaCl3DNow0F0FInsts(struct NaClSymbolTable* st) {
  /* All 3DNOW instructions of form: 0f 0f [modrm] [sib] [displacement]
   *      imm8_opcode
   *
   * These instructions encode into "OP Pq, Qq", based on the value of
   * imm8_opcode. We decode these instructions in two steps. The first
   * step uses a OF0F instruction to read in the bytes of the instruction.
   * These bytes are then inspected, and is replaced by the corresponding
   * 3DNOW instruction in the OFOF prefix table. If no such entry is found,
   * The original match is left so that the bytes are marked as an
   * invalid 3dnow instruction.
   *
   * Note: 3DNow instructions are defined in document 21928G/0-March 2000:
   * "3DNow!(TM) Technology Manual".
   */
  NaClBegDef("     0f0f:     Invalid $Pq, $Qq, $Ib",
                                                   NACLi_INVALID,   st);
  NaClAddIFlags(NACL_IFLAG(Opcode0F0F));
  NaClEndDef(                                                         Other);
  NaClDefine("     0f0f..0c: Pi2fw $Pq, $Qq",      NACLi_E3DNOW,  st, Move);
  NaClDefine("     0f0f..0d: Pi2fd $Pq, $Qq",      NACLi_3DNOW,   st, Move);
  NaClDefine("     0f0f..1c: Pf2iw $Pq, $Qq",      NACLi_E3DNOW,  st, Move);
  NaClDefine("     0f0f..1d: Pf2id $Pq, $Qq",      NACLi_3DNOW,   st, Move);
  NaClDefine("     0f0f..8a: Pfnacc $Pq, $Qq",     NACLi_E3DNOW,  st, Binary);
  NaClDefine("     0f0f..8e: Pfpnacc $Pq, $Qq",    NACLi_E3DNOW,  st, Binary);
  NaClDefine("     0f0f..90: Pfcmpge $Pq, $Qq",    NACLi_3DNOW,   st, Binary);
  NaClDefine("     0f0f..94: Pfmin $Pq, $Qq",      NACLi_3DNOW,   st, Binary);
  NaClDefine("     0f0f..96: Pfrcp $Pq, $Qq",      NACLi_3DNOW,   st, Move);
  NaClDefine("     0f0f..97: Pfrsqrt $Pq, $Qq",    NACLi_3DNOW,   st, Move);
  NaClDefine("     0f0f..9a: Pfsub $Pq, $Qq",      NACLi_3DNOW,   st, Binary);
  NaClDefine("     0f0f..9e: Pfadd $Pq, $Qq",      NACLi_3DNOW,   st, Binary);
  NaClDefine("     0f0f..a0: Pfcmpgt $Pq, $Qq",    NACLi_3DNOW,   st, Binary);
  NaClDefine("     0f0f..a4: Pfmax $Pq, $Qq",      NACLi_3DNOW,   st, Binary);
  NaClDefine("     0f0f..a6: Pfrcpit1 $Pq, $Qq",   NACLi_3DNOW,   st, Binary);
  NaClDefine("     0f0f..a7: Pfrsqit1 $Pq, $Qq",   NACLi_3DNOW,   st, Binary);
  NaClDefine("     0f0f..aa: Pfsubr $Pq, $Qq",     NACLi_3DNOW,   st, Binary);
  NaClDefine("     0f0f..ae: Pfacc $Pq, $Qq",      NACLi_3DNOW,   st, Binary);
  NaClDefine("     0f0f..b0: Pfcmpeq $Pq, $Qq",    NACLi_3DNOW,   st, Binary);
  NaClDefine("     0f0f..b4: Pfmul $Pq, $Qq",      NACLi_3DNOW,   st, Binary);
  NaClDefine("     0f0f..b6: Pfrcpit2 $Pq, $Qq",   NACLi_3DNOW,   st, Binary);
  NaClDefine("     0f0f..b7: Pmulhrw $Pq, $Qq",    NACLi_3DNOW,   st, Binary);
  NaClDefine("     0f0f..bb: Pswapd $Pq, $Qq",     NACLi_E3DNOW,  st, Move);
  NaClDefine("     0f0f..bf: Pavgusb $Pq, $Qq",    NACLi_3DNOW,   st, Binary);
}

void NaClDef0FInsts(struct NaClSymbolTable* st) {
  int i;
  NaClDefPrefixInstMrmChoices(Prefix0F, 0x01, 1, 4);
  NaClDefPrefixInstMrmChoices(Prefix0F, 0x01, 3, 9);
  NaClDefPrefixInstMrmChoices_32_64(Prefix0F, 0x01, 7, 2, 4);
  NaClDefPrefixInstMrmChoices_32_64(Prefix0F, 0xc7, 1, 1, 2);
  NaClDefPrefixInstMrmChoices(Prefix0F, 0xae, 5, 8);
  NaClDefPrefixInstMrmChoices(Prefix0F, 0xae, 6, 8);
  NaClDefPrefixInstMrmChoices(Prefix0F, 0xae, 7, 9);

  NaClDefine("     0f00/0:   Sldt $Mw/Rv",         NACLi_SYSTEM,  st, Sets);
  NaClDefine("     0f00/1:   Str   $Mw/Rv",        NACLi_SYSTEM,  st, Sets);
  NaClDefine("     0f00/2:   Lldt $Ew",            NACLi_SYSTEM,  st, Uses);
  NaClDefine("     0f00/3:   Ltr $Ew",             NACLi_SYSTEM,  st, Uses);
  NaClDefine("     0f00/4:   Verr $Ew",            NACLi_SYSTEM,  st, Other);
  NaClDefine("     0f00/5:   Verw $Ew",            NACLi_SYSTEM,  st, Other);
  NaClDefIter("    0f00/@i:  Invalid", 6, 7,       NACLi_INVALID, st, Other);
  NaClDefine("     0f01/0:   Sgdt $Ms",            NACLi_SYSTEM,  st, Sets);
  NaClDefine("     0f01/1:   Sidt $Ms",            NACLi_SYSTEM,  st, Sets);
  NaClDefine("     0f01/1/0: Monitor %reax, %ecx, %edx",
                                                   NACLi_SYSTEM,  st, Uses);
  NaClDefine("     0f01/1/1: Mwait %eax, %ecx",    NACLi_SYSTEM,  st, Other);
  NaClDefine("     0f01/1:   Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("     0f01/2:   Lgdt $Ms",            NACLi_SYSTEM,  st, Uses);
  NaClDefine("     0f01/3:   Lidt $Ms",            NACLi_SYSTEM,  st, Uses);
  NaClDef_64("     0f01/3/0: Vmrun $rAXva",        NACLi_SVM,     st, Uses);
  NaClDefine("     0f01/3/1: Vmmcall",             NACLi_SVM,     st, Other);
  NaClDefine("     0f01/3/2: Vmload $rAXva",       NACLi_SVM,     st, Uses);
  NaClDefine("     0f01/3/3: Vmsave $rAXva",       NACLi_SVM,     st, Uses);
  NaClDefine("     0f01/3/4: Stgi",                NACLi_SVM,     st, Other);
  NaClDefine("     0f01/3/5: Clgi",                NACLi_SVM,     st, Other);
  NaClDefine("     0f01/3/6: Skinit {%eip}, %eax", NACLi_SVM,     st, Jump);
  NaClDefine("     0f01/3/7: Invlpga $rAXva, %ecx",
                                                   NACLi_SVM,     st, Uses);
  NaClDef_32("     0f01/3:   Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("     0f01/4:   Smsw $Mw/Rv",         NACLi_SYSTEM,  st, Sets);
  NaClDefine("     0f01/5:   Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("     0f01/6:   Lmsw $Ew",            NACLi_INVALID, st, Uses);
  NaClDefine("     0f01/7:   Invlpg $Mb",          NACLi_SYSTEM,  st, Uses);
  NaClDef_64("     0f01/7/0: Swapgs {%gs}",        NACLi_SYSTEM,  st, Sets);
  NaClDef_64("     0f01/7/1: Rdtscp {%rdx}, {%rax}, {%rcx}",
                                                   NACLi_RDTSCP,  st, Sets);
  NaClDefine("     0f01/7:   Invalid",             NACLi_INVALID, st, Other);
  /* Note: Xed appears to use Ev for second argument for Lar aand Lsl. */
  NaClDefine("     0f02:     Lar $Gv, $Ew",        NACLi_SYSTEM,  st, Other);
  NaClDefine("     0f03:     Lsl $Gv, $Ew",        NACLi_SYSTEM,  st, Other);
  NaClDefine("     0f04:     Invalid",             NACLi_INVALID, st, Other);
  NaClDef_64("     0f05:     Syscall {%rip}, {%rcx}",
                                                   NACLi_SYSCALL, st, SysCall);
  NaClDefine("     0f06:     Clts",                NACLi_SYSTEM,  st, Other);
  NaClDef_64("     0f07:     Sysret {%rip}, {%rcx}",
                                                   NACLi_SYSTEM,  st, SysRet);
  NaClDefine("     0f08:     Invd",                NACLi_SYSTEM,  st, Other);
  NaClDefine("     0f09:     Wbinvd",              NACLi_SYSTEM,  st, Other);
  NaClDefine("     0f0a:     Invalid",             NACLi_INVALID, st, Other);
  /* Note: ud2 with no prefix bytes is currently understood as a NOP sequence.
   * The definition here only applies to cases where prefix bytes are added.
   */
  NaClDefine("     0f0b:     Ud2",                 NACLi_386,     st, Other);
  NaClDefine("     0f0c:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("     0f0d/0:   Prefetch_exclusive $Mb",
                                                   NACLi_3DNOW,   st, Other);
  NaClDefine("     0f0d/1:   Prefetch_modified $Mb",
                                                   NACLi_3DNOW,   st, Other);
  NaClDefine("     0f0d/2:   Prefetch_reserved $Mb",
                                                   NACLi_3DNOW,   st, Other);
  NaClDefine("     0f0d/3:   Prefetch_modified $Mb",
                                                   NACLi_3DNOW,   st, Other);
  NaClDefine("     0f0d/4:   Prefetch_reserved $Mb",
                                                   NACLi_3DNOW,   st, Other);
  NaClDefine("     0f0d/5:   Prefetch_reserved $Mb",
                                                   NACLi_3DNOW,   st, Other);
  NaClDefine("     0f0d/6:   Prefetch_reserved $Mb",
                                                   NACLi_3DNOW,   st, Other);
  NaClDefine("     0f0d/7:   Prefetch_reserved $Mb",
                                                   NACLi_3DNOW,   st, Other);
  NaClDefine("     0f0e:     Femms",               NACLi_3DNOW,   st, Other);
  NaCl3DNow0F0FInsts(st);
  NaClDefIter("  f20f13+@i:  Invalid", 0, 4,       NACLi_INVALID, st, Other);
  NaClDefine("     0f18/0:   Prefetchnta $Mb",     NACLi_MMX,     st, Other);
  NaClDefine("     0f18/1:   Prefetcht0 $Mb",      NACLi_MMX,     st, Other);
  NaClDefine("     0f18/2:   Prefetcht1 $Mb",      NACLi_MMX,     st, Other);
  NaClDefine("     0f18/3:   Prefetcht2 $Mb",      NACLi_MMX,     st, Other);
  /* The following 4 are reserved for future prefetch instructions. */
  NaClDefIter("    0f18/@i:  Invalid", 4, 7,       NACLi_INVALID, st, Other);
  NaClDefine("     0f1f/0:   Nop",                 NACLi_386,     st, Other);
  NaClDefIter("    0f19+@i/r: Nop", 0, 6,          NACLi_386,     st, Other);
  NaClDefine("     0f20:     Mov $Rd/q, $Cd/q",    NACLi_SYSTEM,  st, Move);
  NaClDefine("     0f21:     Mov $Rd/q, $Dd/q",    NACLi_SYSTEM,  st, Move);
  NaClDefine("     0f22:     Mov $Cd/q, $Rd/q",    NACLi_SYSTEM,  st, Move);
  NaClDefine("     0f23:     Mov $Dd/q, $Rd/q",    NACLi_SYSTEM,  st, Move);
  NaClDefIter("    0f24+@i:  Invalid", 0, 3,       NACLi_INVALID, st, Other);
  NaClDefine("     0f30:     Wrmsr {%eax}, {%edx}, {%ecx}",
                                                   NACLi_RDMSR,   st, Uses);
  NaClDefine("     0f31:     Rdtsc {%eax}, {%edx}",
                                                   NACLi_RDTSC,   st, Sets);
  NaClDefine("     0f32:     Rdmsr {%eax}, {%edx}, {%ecx}",
                                                   NACLi_RDMSR,   st, O2Move);
  NaClDefine("     0f33:     Rdpmc {%eax}, {%edx}, {%ecx}",
                                                   NACLi_SYSTEM,  st, O2Move);
  NaClDefine("     0f34:     Sysenter {%eip}, {%esp}, {%cs}, {%ss}",
                                                   NACLi_SYSENTER,st, SysJump);
  NaClDefine("     0f35:     Sysexit {%eip}, {%esp}, {%cs}, {%ss}, "
                                 "{%edx}, {%ecx}", NACLi_SYSENTER,st, SysJump);
  NaClDefIter("    0f36+@i:  Invalid", 0, 9,       NACLi_INVALID, st, Other);
  NaClDefine("     0f40:     Cmovo $Gv, $Ev",      NACLi_CMOV,    st, Move);
  NaClDefine("     0f41:     Cmovno $Gv, $Ev",     NACLi_CMOV,    st, Move);
  NaClDefine("     0f42:     Cmovb $Gv, $Ev",      NACLi_CMOV,    st, Move);
  NaClDefine("     0f43:     Cmovnb $Gv, $Ev",     NACLi_CMOV,    st, Move);
  NaClDefine("     0f44:     Cmovz $Gv, $Ev",      NACLi_CMOV,    st, Move);
  NaClDefine("     0f45:     Cmovnz $Gv, $Ev",     NACLi_CMOV,    st, Move);
  NaClDefine("     0f46:     Cmovbe $Gv, $Ev",     NACLi_CMOV,    st, Move);
  NaClDefine("     0f47:     Cmovnbe $Gv, $Ev",    NACLi_CMOV,    st, Move);
  NaClDefine("     0f48:     Cmovs $Gv, $Ev",      NACLi_CMOV,    st, Move);
  NaClDefine("     0f49:     Cmovns $Gv, $Ev",     NACLi_CMOV,    st, Move);
  NaClDefine("     0f4a:     Cmovp $Gv, $Ev",      NACLi_CMOV,    st, Move);
  NaClDefine("     0f4b:     Cmovnp $Gv, $Ev",     NACLi_CMOV,    st, Move);
  NaClDefine("     0f4c:     Cmovl $Gv, $Ev",      NACLi_CMOV,    st, Move);
  NaClDefine("     0f4d:     Cmovnl $Gv, $Ev",     NACLi_CMOV,    st, Move);
  NaClDefine("     0f4e:     Cmovle $Gv, $Ev",     NACLi_CMOV,    st, Move);
  NaClDefine("     0f4f:     Cmovnle $Gv, $Ev",    NACLi_CMOV,    st, Move);
  /* Note: We special case the 66 prefix on direct conditional jumps, by
   * explicitly disallowing 16-bit direct jumps. This is done partly because
   * some versions (within x86-64) are not supported in such cases. However,
   * NaCl also doesn't want to allow 16-bit direct jumps.
   */
  NaClDefine("     0f80:     Jo {%@ip}, $Jzd",     NACLi_386,     st, Jump);
  NaClDefine("     0f81:     Jno {%@ip}, $Jzd",    NACLi_386,     st, Jump);
  NaClDefine("     0f82:     Jb {%@ip}, $Jzd",     NACLi_386,     st, Jump);
  NaClDefine("     0f83:     Jnb {%@ip}, $Jzd",    NACLi_386,     st, Jump);
  NaClDefine("     0f84:     Jz {%@ip}, $Jzd",     NACLi_386,     st, Jump);
  NaClDefine("     0f85:     Jnz {%@ip}, $Jzd",    NACLi_386,     st, Jump);
  NaClDefine("     0f86:     Jbe {%@ip}, $Jzd",    NACLi_386,     st, Jump);
  NaClDefine("     0f87:     Jnbe {%@ip}, $Jzd",   NACLi_386,     st, Jump);
  NaClDefine("     0f88:     Js {%@ip}, $Jzd",     NACLi_386,     st, Jump);
  NaClDefine("     0f89:     Jns {%@ip}, $Jzd",    NACLi_386,     st, Jump);
  NaClDefine("     0f8a:     Jp {%@ip}, $Jzd",     NACLi_386,     st, Jump);
  NaClDefine("     0f8b:     Jnp {%@ip}, $Jzd",    NACLi_386,     st, Jump);
  NaClDefine("     0f8c:     Jl {%@ip}, $Jzd",     NACLi_386,     st, Jump);
  NaClDefine("     0f8d:     Jnl {%@ip}, $Jzd",    NACLi_386,     st, Jump);
  NaClDefine("     0f8e:     Jle {%@ip}, $Jzd",    NACLi_386,     st, Jump);
  NaClDefine("     0f8f:     Jnle {%@ip}, $Jzd",   NACLi_386,     st, Jump);
  NaClDefine("     0f90:     Seto $Eb",            NACLi_386,     st, Sets);
  NaClDefine("     0f91:     Setno $Eb",           NACLi_386,     st, Sets);
  NaClDefine("     0f92:     Setb $Eb",            NACLi_386,     st, Sets);
  NaClDefine("     0f93:     Setnb $Eb",           NACLi_386,     st, Sets);
  NaClDefine("     0f94:     Setz $Eb",            NACLi_386,     st, Sets);
  NaClDefine("     0f95:     Setnz $Eb",           NACLi_386,     st, Sets);
  NaClDefine("     0f96:     Setbe $Eb",           NACLi_386,     st, Sets);
  NaClDefine("     0f97:     Setnbe $Eb",          NACLi_386,     st, Sets);
  NaClDefine("     0f98:     Sets $Eb",            NACLi_386,     st, Sets);
  NaClDefine("     0f99:     Setns $Eb",           NACLi_386,     st, Sets);
  NaClDefine("     0f9a:     Setp $Eb",            NACLi_386,     st, Sets);
  NaClDefine("     0f9b:     Setnp $Eb",           NACLi_386,     st, Sets);
  NaClDefine("     0f9c:     Setl $Eb",            NACLi_386,     st, Sets);
  NaClDefine("     0f9d:     Setnl $Eb",           NACLi_386,     st, Sets);
  NaClDefine("     0f9e:     Setle $Eb",           NACLi_386,     st, Sets);
  NaClDefine("     0f9f:     Setnle $Eb",          NACLi_386,     st, Sets);
  NaClDefine("     0fa0:     Push {%@sp}, %fs",    NACLi_386,     st, Push);
  NaClDefine("     0fa1:     Pop {%@sp}, %fs",     NACLi_386,     st, Pop);
  NaClDefine("     0fa2:     Cpuid {%ebx}, {%edx}, {%eax}, {%ecx}",
                                                   NACLi_386,     st, Cpuid);
  NaClDefine("     0fa3:     Bt $Ev, $Gv",         NACLi_386,     st, Compare);
  NaClDefine("     0fa4:     Shld $Ev, $Gv, $Ib",  NACLi_386,     st, Binary);
  NaClDefine("     0fa5:     Shld $Ev, $Gv, %cl",  NACLi_386,     st, Binary);
  NaClDefIter("    0fa6+@i:  Invalid", 0, 1,       NACLi_INVALID, st, Other);
  NaClDefine("     0fa8:     Push {%@sp}, %gs",    NACLi_386,     st, Push);
  NaClDefine("     0fa9:     Pop {%@sp}, %gs",     NACLi_386,     st, Pop);
  NaClDefine("     0faa:     Rsm",                 NACLi_SYSTEM,  st, Other);
  NaClDefine("     0fab:     Bts $Ev, $Gv",        NACLi_386,     st, Binary);
  NaClDefine("     0fac:     Shrd $Ev, $Gv, $Ib",  NACLi_386,     st, Nary);
  NaClDefine("     0fad:     Shrd $Ev, $Gv, %cl",  NACLi_386,     st, Nary);

  NaClDefine("     0fae/0:   Fxsave $Mf",          NACLi_FXSAVE,  st, Sets);
  NaClDefine("     0fae/1:   Fxrstor $Mf",         NACLi_FXSAVE,  st, Uses);
  NaClDefine("     0fae/2:   Ldmxcsr $Md",         NACLi_SSE,     st, Uses);
  NaClDefine("     0fae/3:   Stmxcsr $Md",         NACLi_SSE,     st, Sets);
  NaClDefine("     0fae/4:   Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("     0fae/5/0: Lfence",              NACLi_SSE2,    st, Other);
  NaClDefine("     0fae/6/0: Mfence",              NACLi_SSE2,    st, Other);
  NaClDefine("     0fae/7/0: Sfence",              NACLi_SFENCE_CLFLUSH,
                                                                  st, Other);
  NaClDefIter("    0fae/5/@i: Invalid", 1, 7,      NACLi_INVALID, st, Other);
  NaClDefIter("    0fae/6/@i: Invalid", 1, 7,      NACLi_INVALID, st, Other);
  NaClDefIter("    0fae/7/@i: Invalid", 1, 7,      NACLi_INVALID, st, Other);
  NaClDefine("     0fae/7:   Clflush $Mb",         NACLi_SFENCE_CLFLUSH,
                                                                  st, Uses);

  NaClDefine("   f20fae:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("   660fae:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("     0faf:     Imul $Gv, $Ev",       NACLi_386,     st, Binary);
  NaClDefine("     0fb0:     Cmpxchg {%al}, $Eb, $Gb",
                                                   NACLi_386,     st, Exchange);
  NaClDefine("     0fb1:     Cmpxchg {$rAXv}, $Ev, $Gv",
                                                   NACLi_386,     st, Exchange);
  NaClDefine("     0fb2:     Lss $SGz, $Mp",       NACLi_386,     st, Lea);
  NaClDefine("     0fb3:     Btr $Ev, $Gv",        NACLi_386,     st, Binary);
  NaClDefine("     0fb4:     Lfs $SGz, $Mp",       NACLi_386,     st, Lea);
  NaClDefine("     0fb5:     Lgs $SGz, $Mp",       NACLi_386,     st, Lea);
  NaClDefine("     0fb6:     Movzx $Gv, $Eb",      NACLi_386,     st, Move);
  NaClDefine("     0fb7:     Movzx $Gv, $Ew",      NACLi_386,     st, Move);

  NaClDefine("     0fb8:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("     0fb9/r:   Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("     0fba/4:   Bt $Ev, $Ib",         NACLi_386,     st, Compare);
  NaClDefine("     0fba/5:   Bts $Ev, $Ib",        NACLi_386,     st, Binary);
  NaClDefine("     0fba/6:   Btr $Ev, $Ib",        NACLi_386,     st, Binary);
  NaClDefine("     0fba/7:   Btc $Ev, $Ib",        NACLi_386,     st, Binary);
  NaClDefine("     0fba/r:   Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("     0fbb:     Btc $Ev, $Gv",        NACLi_386,     st, Binary);
  NaClDefine("     0fbc:     Bsf $Gv, $Ev",        NACLi_386,     st, Move);
  NaClDefine("     0fbd:     Bsr $Gv, $Ev",        NACLi_386,     st, Move);
  NaClDefine("     0fbe:     Movsx $Gv, $Eb",      NACLi_386,     st, Move);
  NaClDefine("     0fbf:     Movsx $Gv, $Ew",      NACLi_386,     st, Move);
  NaClDefine("     0fc0:     Xadd $Eb, $Gb",       NACLi_386,     st, Exchange);
  NaClDefine("     0fc1:     Xadd $Ev, $Gv",       NACLi_386,     st, Exchange);
  NaClBegDef("     0fc7/1:   Cmpxchg8b {%edx}, {%eax}, $Mq",
                                                   NACLi_CMPXCHG8B, st);
  NaClAddIFlags(NACL_IFLAG(OperandSize_v));
  NaClEndDef(                                                         Exchange);
  NaClBegD64("     0fc7/1:   Cmpxchg16b {%rdx}, {%eax}, $Mdq",
                                                   NACLi_CMPXCHG16B, st);
  NaClAddIFlags(NACL_IFLAG(OperandSize_o));
  NaClEndDef(                                                         Exchange);
  NaClDefine("     0fc7/r:   Invalid",             NACLi_INVALID, st, Other);
  for (i = 0; i <= 7; ++i) {
    NaClDefPrefixInstChoices(Prefix0F, 0xc8 + i, 2);
  }
  NaClDefReg("     0fc8+@reg:  Bswap $r8vd", 0, 7, NACLi_386,     st,
                                                                  UnaryUpdate);
  NaClDefReg("     0fc8+@reg:  Bswap $r8vq", 0, 7,  NACLi_386,    st,
                                                                  UnaryUpdate);
}
