/* native_client/src/trusted/validator/x86/ncval_seg_sfi/gen/ncbadprefixmask_64.h
 * THIS FILE IS AUTO-GENERATED. DO NOT EDIT.
 * Compiled for x86-64 bit mode.
 *
 * You must include ncdecode.h before this file.
 */

static uint32_t BadPrefixMask[kNaClInstTypeRange] = {
  0xffffffff, /* NACLi_UNDEFINED */
  0xffffffff, /* NACLi_ILLEGAL */
  0xffffffff, /* NACLi_INVALID */
  0xffffffff, /* NACLi_SYSTEM */
  0xffffffff, /* NACLi_NOP */
  0xffffffe7, /* NACLi_386 */
  0xfffffeef, /* NACLi_386L */
  0xffffff6f, /* NACLi_386R */
  0xffffff2f, /* NACLi_386RE */
  0xfffffbfe, /* NACLi_JMP8 */
  0xfffffbfe, /* NACLi_JMPZ */
  0xffffffff, /* NACLi_INDIRECT */
  0xffffffff, /* NACLi_OPINMRM */
  0xffffffff, /* NACLi_RETURN */
  0xffffffff, /* NACLi_LAHF */
  0xffffffff, /* NACLi_SFENCE_CLFLUSH */
  0xfffffeff, /* NACLi_CMPXCHG8B */
  0xffffffff, /* NACLi_CMPXCHG16B */
  0xffffffff, /* NACLi_CMOV */
  0xffffffff, /* NACLi_RDMSR */
  0xffffffff, /* NACLi_RDTSC */
  0xffffffff, /* NACLi_RDTSCP */
  0xffffffff, /* NACLi_SYSCALL */
  0xffffffff, /* NACLi_SYSENTER */
  0xffffffff, /* NACLi_X87 */
  0xffffffff, /* NACLi_X87_FSINCOS */
  0xffffffff, /* NACLi_MMX */
  0xffffffff, /* NACLi_MMXSSE2 */
  0xffffffff, /* NACLi_3DNOW */
  0xffffffff, /* NACLi_EMMX */
  0xffffffff, /* NACLi_E3DNOW */
  0xffffffff, /* NACLi_SSE */
  0xffffffff, /* NACLi_SSE2 */
  0xffffffff, /* NACLi_SSE2x */
  0xffffffff, /* NACLi_SSE3 */
  0xffffffff, /* NACLi_SSE4A */
  0xffffffff, /* NACLi_SSE41 */
  0xffffffff, /* NACLi_SSE42 */
  0xffffffff, /* NACLi_MOVBE */
  0xffffffff, /* NACLi_POPCNT */
  0xffffffff, /* NACLi_LZCNT */
  0xffffffff, /* NACLi_LONGMODE */
  0xffffffff, /* NACLi_SVM */
  0xffffffff, /* NACLi_SSSE3 */
  0xffffffff, /* NACLi_3BYTE */
  0xffffffff, /* NACLi_FCMOV */
  0xffffffff, /* NACLi_VMX */
  0xffffffff, /* NACLi_FXSAVE */
  0xffffffff, /* (null) */
};
