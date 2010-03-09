/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * ncdis.c - disassemble using NaCl decoder.
 * Mostly for testing.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/trusted/validator_x86/ncdecode.h"
#include "gen/native_client/src/trusted/validator_x86/ncdisasmtab.h"


/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

/* The following are conditionally added since they only have meaning
 * if we are processing 64-bit instructions.
 */
#if NACL_TARGET_SUBARCH == 64

/* Returns if the decoded instruction contains a REX prefix byte. */
static INLINE uint8_t HasRexPrefix(const struct NCDecoderState* mstate) {
  /* Note: we could check the prefix mask of the recognized instruction to
   * see if bit kPrefixREX is set. However, the field rexprefix is
   * initialized to zero (during decoding) and only set if an actual
   * REX prefix is found (which must be values between 0x40 and 0x4f)
   */
  return 0 != mstate->inst.rexprefix;
}

/* Returns true if REX.W is defined in the REX prefix byte. */
static INLINE uint8_t GetRexPrefixW(const struct NCDecoderState* mstate) {
  /* Note: the field rexprefix is non-zero only if a rexprefix was found. */
  return 0 != (mstate->inst.rexprefix & 0x8);
}

/* Returns true if REX.R is defined in the REX prefix byte. */
static INLINE uint8_t GetRexPrefixR(const struct NCDecoderState* mstate) {
  /* Note: the field rexprefix is non-zero only if a rexprefix was found. */
  return 0 != (mstate->inst.rexprefix & 0x4);
}

/* Returns true if REX.X is defined in the REX prefix byte. */
static INLINE uint8_t GetRexPrefixX(const struct NCDecoderState* mstate) {
  /* Note: the field rexprefix is non-zero only if a rexprefix was found. */
  return 0 != (mstate->inst.rexprefix & 0x2);
}

/* Returns true if REX.B is defined in the REX prefix byte. */
static INLINE uint8_t GetRexPrefixB(const struct NCDecoderState* mstate) {
  /* Note: the field rexprefix is non-zero only if a rexprefix was found. */
  return 0 != (mstate->inst.rexprefix & 0x1);
}
#endif

/* Returns the index into the general purpose registers based on the
 * value in the modrm.rm field of the instruction (taking into account
 * the REX prefix if it appears).
 */
static INLINE uint32_t state_modrm_reg(const struct NCDecoderState* mstate) {
  /* TODO(karl) This is no where near close to being correct. Currently,
   * It only models r/m64 entry for instructions in the Intel documentation,
   * which requires the W bit to be set (r/m32 entries do not use REX.w,
   * and a different set of registers).
   */
  uint32_t index = modrm_reg(mstate->inst.mrm);
#if NACL_TARGET_SUBARCH == 64
  if (GetRexPrefixW(mstate) && GetRexPrefixR(mstate)) {
    index += 8;
  }
#endif
  return index;
}

/* later this will make decoding x87 instructions a bit more concise. */
static const char** kDisasmX87Op[8] = {kDisasm87D8,
                                       kDisasm87D9,
                                       kDisasm87DA,
                                       kDisasm87DB,
                                       kDisasm87DC,
                                       kDisasm87DD,
                                       kDisasm87DE,
                                       kDisasm87DF};

const char** kDummyUsesToAvoidCompilerWarning[] = {kDisasm660F38Op,
                                                   kDisasmF20F38Op,
                                                   kDisasm660F3AOp};

/* disassembler stuff */
static const char* DisFmt(const struct NCDecoderState *mstate) {
  static const uint8_t kWAITOp = 0x9b;
  uint8_t *opbyte = &mstate->inst.maddr[mstate->inst.prefixbytes];
  uint8_t pm = mstate->inst.prefixmask;

  if (mstate->opinfo->insttype == NACLi_X87) {
    if (opbyte[0] != kWAITOp) {
      return kDisasmX87Op[opbyte[0]-0xd8][mstate->inst.mrm];
    }
  }
  if (mstate->opinfo->insttype == NACLi_FCMOV) {
    return kDisasmX87Op[opbyte[0]-0xd8][mstate->inst.mrm];
  }
  if (*opbyte != kTwoByteOpcodeByte1) return kDisasm1ByteOp[opbyte[0]];
  if (opbyte[1] == 0x0f) return kDisasm0F0FOp[opbyte[mstate->inst.length - 1]];
  if (opbyte[1] == 0x38) return kDisasm0F38Op[opbyte[2]];
  if (opbyte[1] == 0x3A) return kDisasm0F3AOp[opbyte[2]];
  if (! (pm & (kPrefixDATA16 | kPrefixREPNE | kPrefixREP))) {
    return kDisasm0FXXOp[opbyte[1]];
  }
  if (pm & kPrefixDATA16) return kDisasm660FXXOp[opbyte[1]];
  if (pm & kPrefixREPNE)  return kDisasmF20FXXOp[opbyte[1]];
  if (pm & kPrefixREP)    return kDisasmF30FXXOp[opbyte[1]];

  /* no update; should be invalid */
  return "internal error";
}

static int ByteImmediate(const uint8_t* byte_array) {
  return (char) byte_array[0];
}

static int WordImmediate(const uint8_t* byte_array) {
  return (short) (byte_array[0] + (byte_array[1] << 8));
}

static int DwordImmediate(const uint8_t* byte_array) {
  return (byte_array[0] +
          (byte_array[1] << 8) +
          (byte_array[2] << 16) +
          (byte_array[3] << 24));
}

/* Defines the set of available general purpose registers. */
static const char* gp_regs[] = {
#if NACL_TARGET_SUBARCH == 64
  /* TODO(karl) - Depending on whether the instruction uses r/m32
   * of r/m64 forms (according to the Intel document), the list of
   * (16) registers is different. Currently, we are only handling
   * the r/m64 case.
   */
  "%rax", "%rcx", "%rdx", "%rbx", "%rsp", "%rbp", "%rsi", "%rdi",
  "%r8", "%r9", "%r10", "%r11", "%r12", "%r13", "%r14", "%r15"
#else
  "%eax", "%ecx", "%edx", "%ebx", "%esp", "%ebp", "%esi", "%edi"
#endif
};


static const char* mmx_regs[] = {
  "%mm0", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5", "%mm6", "%mm7"
};

static const char* xmm_regs[] = {
  "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7"
};

static const char* seg_regs[] = {
  "%es", "%cs", "%ss", "%ds", "%fs", "%gs"
};

static void SibPrint(const struct NCDecoderState *mstate,
                     uint32_t sib_offset,
                     FILE* fp) {
  uint8_t sib = mstate->inst.maddr[sib_offset];

  if (sib_ss(sib) == 0) {
    if (sib_base(sib) == 5) {
      const uint8_t* disp_addr = mstate->inst.maddr + sib_offset + 1;
      fprintf(fp, "[0x%x]", DwordImmediate(disp_addr));
    } else {
      /* Has a base register */
      if (sib_index(sib) == 4) {
        /* No index */
        fprintf(fp, "[%s]", gp_regs[sib_base(sib)]);
      } else {
        fprintf(fp, "[%s + %s]",
                gp_regs[sib_base(sib)],
                gp_regs[sib_index(sib)]);
      }
    }
  } else {
    if (sib_index(sib) == 4) {
      /* No index */
      fprintf(fp, "[%s]", gp_regs[sib_base(sib)]);
    } else {
      fprintf(fp, "[%s + %d * %s]",
              gp_regs[sib_base(sib)],
              1 << sib_ss(sib),
              gp_regs[sib_index(sib)]);
    }
  }
}


static void SegPrefixPrint(const struct NCDecoderState *mstate,
                           FILE* fp) {
  uint8_t pm = mstate->inst.prefixmask;
  if (pm & kPrefixSEGCS) {
    fprintf(fp, "cs:");
  } else if (pm & kPrefixSEGSS) {
    fprintf(fp, "ss:");
  } else if (pm & kPrefixSEGFS) {
    fprintf(fp, "fs:");
  } else if (pm & kPrefixSEGGS) {
    fprintf(fp, "gs:");
  }
}

/* Print out the register (from the list of register names),
 * that is referenced by the decoded instruction in the
 * decoder state. If is_gp_regs is true, the register names
 * correspond to general purpose register names. Otherwise,
 * they are some other set, such as MMX or XMM.
 */
static void RegMemPrint(const struct NCDecoderState *mstate,
                        const char* reg_names[],
                        const uint8_t is_gp_regs,
                        FILE* fp) {
  uint32_t sib_offset =
      mstate->inst.prefixbytes +
      1 +
      mstate->inst.hasopbyte2 +
      mstate->inst.hasopbyte3 +
      1;
  const uint8_t* disp_addr = mstate->inst.maddr +
                             sib_offset +
                             mstate->inst.hassibbyte;

  DEBUG( printf(
             "reg mem print: sib_offset = %d, disp_addr = %p, mrm.mod = %02x\n",
             sib_offset, disp_addr, modrm_mod(mstate->inst.mrm)) );
  switch (modrm_mod(mstate->inst.mrm)) {
    case 0:
     SegPrefixPrint(mstate, fp);
      if (4 == modrm_rm(mstate->inst.mrm)) {
        SibPrint(mstate, sib_offset, fp);
      } else if (5 == modrm_rm(mstate->inst.mrm)) {
        fprintf(fp, "[0x%x]", DwordImmediate(disp_addr));
      } else {
        fprintf(fp, "[%s]", gp_regs[modrm_rm(mstate->inst.mrm)]);
      }
      break;
    case 1: {
        SegPrefixPrint(mstate, fp);
        fprintf(fp, "0x%x", ByteImmediate(disp_addr));
        if (4 == modrm_rm(mstate->inst.mrm)) {
          SibPrint(mstate, sib_offset, fp);
        } else {
          fprintf(fp, "[%s]", gp_regs[modrm_rm(mstate->inst.mrm)]);
        }
      }
      break;
    case 2: {
        SegPrefixPrint(mstate, fp);
        fprintf(fp, "0x%x", DwordImmediate(disp_addr));
        if (4 == modrm_rm(mstate->inst.mrm)) {
          SibPrint(mstate, sib_offset, fp);
        } else {
          fprintf(fp, "[%s]", gp_regs[modrm_rm(mstate->inst.mrm)]);
        }
      }
      break;
    case 3:
      if (is_gp_regs) {
        fprintf(fp, "%s", reg_names[state_modrm_reg(mstate)]);
      } else {
        fprintf(fp, "%s", reg_names[modrm_rm(mstate->inst.mrm)]);
      }
      break;
  }
}

/* Parses the group name in token. Returns NOGROUP(0) if unable to parse. */
static NaClMRMGroups ParseGroupName(const char* token) {
  if (!strncmp(token, "group", 5)) {
    /* Matched group name prefix, recognize last two characters explicitly.*/
    const char* suffix = token + 5;
    switch (strlen(suffix)) {
      case 1:
        switch (suffix[0]) {
          case '1':
            return GROUP1;
          case '2':
            return GROUP2;
          case '3':
            return GROUP3;
          case '4':
            return GROUP4;
          case '5':
            return GROUP5;
          case '6':
            return GROUP6;
          case '7':
            return GROUP7;
          case '8':
            return GROUP8;
          case '9':
            return GROUP9;
          case 'p':
            return GROUPP;
          default:
            break;
        }
        break;
      case 2:
        if (suffix[0] == '1') {
          switch(suffix[1]) {
            case '0':
              return GROUP10;
            case '1':
              return GROUP11;
            case '2':
              return GROUP12;
            case '3':
              return GROUP13;
            case '4':
              return GROUP14;
            case '5':
              return GROUP15;
            case '6':
              return GROUP16;
            case '7':
              return GROUP17;
            case 'a':
              return GROUP1A;
            default:
              break;
          }
        }
        break;
      default:
        break;
    }
  }
  return 0;
}

static void InstFormat(const char* format,
                       const struct NCDecoderState *mstate,
                       FILE* fp) {
  char token_buf[128];
  char* fmt = token_buf;
  int pos = 0;

  strncpy(token_buf, format, sizeof(token_buf));

  while (1) {
    char* token = strtok(fmt, " ,\n");
    DEBUG( printf("\ntoken = '%s'\n", token) );
    if (NULL == token) {
      break;
    }
    if (pos > 1) {
      fprintf(fp, ", ");
    } else if (pos > 0) {
      fprintf(fp, " ");
    }
    if ('$' == token[0]) {
      NaClMRMGroups group = ParseGroupName(token+1);
      if (NOGROUP != group) {
        int mrm = modrm_reg(mstate->inst.mrm);
        const char* opname = kDisasmModRMOp[group][mrm];
        DEBUG( printf("case: group %d, opname = %s\n", group, opname) );
        fprintf(fp, "%s", opname);
      } else {
        /* Tokens starting with a $ but not $group need formatting */
        DEBUG( printf("case: $ and not group\n") );
        switch (token[1]) {
          case 'A':
            fprintf(fp, "$A");
            break;
          case 'C':
            fprintf(fp, "%%cr%d", modrm_reg(mstate->inst.mrm));
            break;
          case 'D':
            fprintf(fp, "%%dr%d", modrm_reg(mstate->inst.mrm));
            break;
          case 'E':
          case 'M': /* mod should never be 3 for 'M' */
            /* TODO(sehr): byte and word accesses */
            RegMemPrint(mstate, gp_regs, 1, fp);
            break;
          case 'F':
            fprintf(fp, "eflags");
            break;
          case 'G':
            fprintf(fp, "%s", gp_regs[modrm_reg(mstate->inst.mrm)]);
            break;
          case 'I': {
              const uint8_t* imm_addr = mstate->inst.maddr +
                  mstate->inst.prefixbytes +
                  1 +
                  mstate->inst.hasopbyte2 +
                  mstate->inst.hasopbyte3 +
                  mstate->opinfo->hasmrmbyte +
                  mstate->inst.hassibbyte +
                  mstate->inst.dispbytes;
              if ('b' == token[2]) {
                fprintf(fp, "0x%x", ByteImmediate(imm_addr));
              } else if ('w' == token[2]) {
                fprintf(fp, "0x%x", WordImmediate(imm_addr));
              } else {
                fprintf(fp, "0x%x", DwordImmediate(imm_addr));
              }
            }
            break;
          case 'J': {
              const uint8_t* imm_addr = mstate->inst.maddr +
                  mstate->inst.prefixbytes +
                  1 +
                  mstate->inst.hasopbyte2 +
                  mstate->inst.hasopbyte3 +
                  mstate->opinfo->hasmrmbyte +
                  mstate->inst.hassibbyte +
                  mstate->inst.dispbytes;
              if ('b' == token[2]) {
                fprintf(fp, "0x%"NACL_PRIxPcAddress,
                        mstate->inst.vaddr + mstate->inst.length +
                        ByteImmediate(imm_addr));
              } else {
                fprintf(fp, "0x%"NACL_PRIxPcAddress,
                        mstate->inst.vaddr + mstate->inst.length +
                        DwordImmediate(imm_addr));
              }
            }
            break;
          case 'O': {
              const uint8_t* imm_addr = mstate->inst.maddr +
                  mstate->inst.prefixbytes +
                  1 +
                  mstate->inst.hasopbyte2 +
                  mstate->inst.hasopbyte3;
              fprintf(fp, "[0x%x]", DwordImmediate(imm_addr));
            }
            break;
          case 'P':
            if ('R' == token[2]) {
              fprintf(fp, "%%mm%d", modrm_rm(mstate->inst.mrm));
            } else {
              fprintf(fp, "%%mm%d", modrm_reg(mstate->inst.mrm));
            }
            break;
          case 'Q':
            RegMemPrint(mstate, mmx_regs, 0, fp);
            break;
          case 'R':
            fprintf(fp, "%s", gp_regs[modrm_rm(mstate->inst.mrm)]);
            break;
          case 'S':
            fprintf(fp, "%s", seg_regs[modrm_reg(mstate->inst.mrm)]);
            break;
          case 'V':
            if ('R' == token[2]) {
              fprintf(fp, "%%xmm%d", modrm_rm(mstate->inst.mrm));
            } else {
              fprintf(fp, "%%xmm%d", modrm_reg(mstate->inst.mrm));
            }
            break;
          case 'W':
            RegMemPrint(mstate, xmm_regs, 0, fp);
            break;
          case 'X':
            fprintf(fp, "ds:[esi]");
            break;
          case 'Y':
            fprintf(fp, "es:[edi]");
            break;
          default:
            fprintf(fp, "token('%s')", token);
            break;
        }
      }
    } else {
      /* Print the token as is */
      fprintf(fp, "%s", token);
    }
    fmt = NULL;
    ++pos;
  }
}


void PrintInst(const struct NCDecoderState *mstate, FILE* fp) {
  int i;
  DEBUG( printf("use format: %s\n", DisFmt(mstate)) );
  fprintf(fp, " %"NACL_PRIxPcAddress":\t%02x", mstate->inst.vaddr,
          mstate->inst.maddr[0]);
  for (i = 1; i < mstate->inst.length; i++) {
    fprintf(fp, " %02x", mstate->inst.maddr[i]);
  }
  for (i = mstate->inst.length; i < 7; i++) fprintf(fp, "   ");
  fprintf(fp, "\t");
  InstFormat(DisFmt(mstate), mstate, fp);
  fprintf(fp, "\n");
}


void PrintInstStdout(const struct NCDecoderState *mstate) {
  PrintInst(mstate, stdout);
}
