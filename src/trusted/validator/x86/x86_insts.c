/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/validator/x86/x86_insts.h"

static const char *kNaClInstTypeString[kNaClInstTypeRange] = {
  "NACLi_UNDEFINED",
  "NACLi_ILLEGAL",
  "NACLi_INVALID",
  "NACLi_SYSTEM",
  "NACLi_NOP",
  "NACLi_UD2",
  "NACLi_386",
  "NACLi_386L",
  "NACLi_386R",
  "NACLi_386RE",
  "NACLi_JMP8",
  "NACLi_JMPZ",
  "NACLi_INDIRECT",
  "NACLi_OPINMRM",
  "NACLi_RETURN",
  "NACLi_SFENCE_CLFLUSH",
  "NACLi_CMPXCHG8B",
  "NACLi_CMPXCHG16B",
  "NACLi_CMOV",
  "NACLi_RDMSR",
  "NACLi_RDTSC",
  "NACLi_RDTSCP",
  "NACLi_SYSCALL",
  "NACLi_SYSENTER",
  "NACLi_X87",
  "NACLi_MMX",
  "NACLi_MMXSSE2",
  "NACLi_3DNOW",
  "NACLi_EMMX",
  "NACLi_E3DNOW",
  "NACLi_SSE",
  "NACLi_SSE2",
  "NACLi_SSE2x",
  "NACLi_SSE3",
  "NACLi_SSE4A",
  "NACLi_SSE41",
  "NACLi_SSE42",
  "NACLi_MOVBE",
  "NACLi_POPCNT",
  "NACLi_LZCNT",
  "NACLi_LONGMODE",
  "NACLi_SVM",
  "NACLi_SSSE3",
  "NACLi_3BYTE",
  "NACLi_FCMOV",
  "NACLi_VMX",
  "NACLi_FXSAVE",
};

const char* NaClInstTypeString(NaClInstType typ) {
  return kNaClInstTypeString[typ];
}

/* Defines the range of possible (initial) opcodes for x87 instructions. */
const uint8_t kFirstX87Opcode = 0xd8;
const uint8_t kLastX87Opcode = 0xdf;

/* Defines the opcode for the WAIT instruction.
 * Note: WAIT is an x87 instruction but not in the coproc opcode space.
 */
const uint8_t kWAITOp = 0x9b;

const uint8_t kTwoByteOpcodeByte1 = 0x0f;
const uint8_t k3DNowOpcodeByte2 = 0x0f;
const int kMaxPrefixBytes = 4;

/* Accessors for the ModRm byte. */
uint8_t modrm_mod(uint8_t modrm) { return ((modrm >> 6) & 0x03);}
uint8_t modrm_rm(uint8_t modrm) { return (modrm & 0x07); }
uint8_t modrm_reg(uint8_t modrm) { return ((modrm >> 3) & 0x07); }
uint8_t modrm_opcode(uint8_t modrm) { return modrm_reg(modrm); }

/* Accessors for the Sib byte. */
uint8_t sib_ss(uint8_t sib) { return ((sib >> 6) & 0x03); }
uint8_t sib_index(uint8_t sib) { return ((sib >> 3) & 0x07); }
uint8_t sib_base(uint8_t sib) { return (sib & 0x07); }


/* Defines the corresponding mask for the bits of the the Rex prefix. */
#define NaClRexWMask 0x8
#define NaClRexRMask 0x4
#define NaClRexXMask 0x2
#define NaClRexBMask 0x1

/* Defines the range of rex prefix values. */
const uint8_t NaClRexMin = 0x40;
const uint8_t NaClRexMax = 0x4F;

uint8_t NaClRexW(uint8_t prefix) {
  return NaClHasBit(prefix, NaClRexWMask);
}

uint8_t NaClRexR(uint8_t prefix) {
  return NaClHasBit(prefix, NaClRexRMask);
}

uint8_t NaClRexX(uint8_t prefix) {
  return NaClHasBit(prefix, NaClRexXMask);
}

uint8_t NaClRexB(uint8_t prefix) {
  return NaClHasBit(prefix, NaClRexBMask);
}
