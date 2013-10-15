/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * ncdecode_verbose.c - Print routines for validator that are
 * not to be loaded into sel_ldr.
 */

#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncdecode_verbose.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/shared/gio/gio.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncdecode.h"

#include "native_client/src/trusted/validator/x86/x86_insts_inl.c"

#if NACL_TARGET_SUBARCH == 64
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/gen/ncdisasmtab_64.h"
#else
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/gen/ncdisasmtab_32.h"
#endif

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

/* The following are conditionally added since they only have meaning
 * if we are processing 64-bit instructions.
 */
#if NACL_TARGET_SUBARCH == 64

/* Returns true if REX.W is defined in the REX prefix byte. */
static INLINE uint8_t GetRexPrefixW(const NCDecoderInst* dinst) {
  /* Note: the field rexprefix is non-zero only if a rexprefix was found. */
  return 0 != NaClRexW(dinst->inst.rexprefix);
}

/* Returns true if REX.R is defined in the REX prefix byte. */
static INLINE uint8_t GetRexPrefixR(const NCDecoderInst* dinst) {
  /* Note: the field rexprefix is non-zero only if a rexprefix was found. */
  return 0 != NaClRexR(dinst->inst.rexprefix);
}

#endif

/* Returns the index into the general purpose registers based on the
 * value in the modrm.rm field of the instruction (taking into account
 * the REX prefix if it appears).
 */
static INLINE uint32_t state_modrm_reg(const NCDecoderInst* dinst) {
  /* TODO(karl) This is no where near close to being correct. Currently,
   * It only models r/m64 entry for instructions in the Intel documentation,
   * which requires the W bit to be set (r/m32 entries do not use REX.w,
   * and a different set of registers).
   */
  uint32_t index = modrm_regInline(dinst->inst.mrm);
#if NACL_TARGET_SUBARCH == 64
  if (GetRexPrefixW(dinst) && GetRexPrefixR(dinst)) {
    index += 8;
  }
#endif
  return index;
}

/* Returns the index for the first instruction opcode byte. */
static INLINE int NCOpcodeOffset(const NCDecoderInst* dinst) {
  return dinst->inst.prefixbytes;
}

/* Returns the index for the modrm byte. */
static INLINE int NCMrmOffset(const NCDecoderInst* dinst) {
  return NCOpcodeOffset(dinst) + dinst->inst.num_opbytes;
}

/* Returns the index of the sib byte (if it has one). */
static INLINE int NCSibOffset(const NCDecoderInst* dinst) {
  /* Note: The sib byte follows the mrm byte. */
  return NCMrmOffset(dinst) + 1;
}

/* Returns the beginning index for the displacement (if it has one). */
static int NCDispOffset(const NCDecoderInst* dinst) {
  if (dinst->opinfo->hasmrmbyte) {
    if (dinst->inst.hassibbyte) {
      return NCSibOffset(dinst) + 1;
    } else {
      return NCMrmOffset(dinst) + 1;
    }
  } else {
    return NCOpcodeOffset(dinst) + dinst->inst.num_opbytes;
  }
}

/* Returns the beginning index for the immediate value. */
static INLINE int NCImmedOffset(const NCDecoderInst* dinst) {
  return NCDispOffset(dinst) + dinst->inst.dispbytes;
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
static const char* DisFmt(const NCDecoderInst *dinst) {
  NCInstBytesPtr opbyte;
  uint8_t opbyte0;
  uint8_t opbyte1;
  uint8_t pm = dinst->inst.opcode_prefixmask;
  NCInstBytesPtrInitInc(&opbyte, &dinst->inst_bytes, NCOpcodeOffset(dinst));
  opbyte0 = NCInstBytesByte(&opbyte, 0);

  if (dinst->opinfo->insttype == NACLi_X87 ||
      dinst->opinfo->insttype == NACLi_X87_FSINCOS) {
    if (opbyte0 != kWAITOp) {
      return kDisasmX87Op[opbyte0 - kFirstX87Opcode][dinst->inst.mrm];
    }
  }
  if (dinst->opinfo->insttype == NACLi_FCMOV) {
    return kDisasmX87Op[opbyte0 - kFirstX87Opcode][dinst->inst.mrm];
  }
  if (dinst->opinfo->insttype == NACLi_NOP) return "nop";
  if (opbyte0 != kTwoByteOpcodeByte1) return kDisasm1ByteOp[opbyte0];
  opbyte1 = NCInstBytesByte(&opbyte, 1);
  if (opbyte1 == 0x0f) {
    return kDisasm0F0FOp[
        NCInstBytesByte(&opbyte, dinst->inst.bytes.length - 1)];
  }
  if (opbyte1 == 0x38) {
    return kDisasm0F38Op[NCInstBytesByte(&opbyte, 2)];
  }
  if (opbyte1 == 0x3A) {
    return kDisasm0F3AOp[NCInstBytesByte(&opbyte, 2)];
  }
  if (! (pm & (kPrefixDATA16 | kPrefixREPNE | kPrefixREP))) {
    return kDisasm0FXXOp[opbyte1];
  }
  if (pm & kPrefixDATA16) return kDisasm660FXXOp[opbyte1];
  if (pm & kPrefixREPNE)  return kDisasmF20FXXOp[opbyte1];
  if (pm & kPrefixREP)    return kDisasmF30FXXOp[opbyte1];

  /* no update; should be invalid */
  return "internal error";
}

/* Returns the (sign extended) 32-bit integer immediate value. */
static int32_t ImmedValue32(const NCDecoderInst* dinst) {
  NCInstBytesPtr addr;
  NCInstBytesPtrInitInc(&addr, &dinst->inst_bytes,
                        NCImmedOffset(dinst));
  return NCInstBytesInt32(&addr, dinst->inst.immbytes);
}

/* Returns the (sign extended) 64-bit integer immediate value. */
static int64_t ImmedValue64(const NCDecoderInst* dinst) {
  NCInstBytesPtr addr;
  NCInstBytesPtrInitInc(&addr, &dinst->inst_bytes,
                        NCImmedOffset(dinst));
  return NCInstBytesInt64(&addr, dinst->inst.immbytes);
}

/* Returns the (sign extended) 32-bit integer displacement value. */
static int32_t DispValue32(const NCDecoderInst* dinst) {
  NCInstBytesPtr addr;
  NCInstBytesPtrInitInc(&addr, &dinst->inst_bytes,
                        NCDispOffset(dinst));
  return NCInstBytesInt32(&addr, dinst->inst.dispbytes);
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

/* Print out the sib byte of the parsed instruction in the given decoder state
 * to the given file.
 */
static void SibPrint(const NCDecoderInst* dinst, struct Gio* fp) {
  uint8_t sib = NCInstBytesByte(&dinst->inst_bytes, NCSibOffset(dinst));

  if (!dinst->inst.hassibbyte) {
    /* This should not happen. */
    gprintf(fp, "?");
  } else if (sib_ss(sib) == 0) {
    if (sib_base(sib) == 5) {
      /* This code is JUST WRONG!. However, I am not fixing since
       * the decoder printer is broken in so many other ways!. See tables
       * 2-1 and 2-2 of "Intel 64 and IA-32 Architectures Software Developer's
       * Manual" for details of what should be done (there are
       * 3 cases, depending on the value of the mod field of the modrm
       * byte).
       * Note: While the changes here are not correct, they at least make
       * sure that we process bytes that are actually in the instruction.
       * That is, the code doesn't accidentally walk off the end of the
       * parsed instruction.
       */
      gprintf(fp, "[0x%x]", DispValue32(dinst));
    } else {
      /* Has a base register */
      if (sib_index(sib) == 4) {
        /* No index */
        gprintf(fp, "[%s]", gp_regs[sib_base(sib)]);
      } else {
        gprintf(fp, "[%s + %s]",
                gp_regs[sib_base(sib)],
                gp_regs[sib_index(sib)]);
      }
    }
  } else {
    if (sib_index(sib) == 4) {
      /* No index */
      gprintf(fp, "[%s]", gp_regs[sib_base(sib)]);
    } else {
      gprintf(fp, "[%s + %d * %s]",
              gp_regs[sib_base(sib)],
              1 << sib_ss(sib),
              gp_regs[sib_index(sib)]);
    }
  }
}

static void SegPrefixPrint(const NCDecoderInst *dinst, struct Gio* fp) {
  uint8_t pm = dinst->inst.prefixmask;
  if (pm & kPrefixSEGCS) {
    gprintf(fp, "cs:");
  } else if (pm & kPrefixSEGSS) {
    gprintf(fp, "ss:");
  } else if (pm & kPrefixSEGFS) {
    gprintf(fp, "fs:");
  } else if (pm & kPrefixSEGGS) {
    gprintf(fp, "gs:");
  }
}

/* Append that we don't bother to translate the instruction argument,
 * since it is NaCl illegal. Used to handle cases where we don't implement
 * 16-bit modrm effective addresses.
 */
static INLINE void NaClIllegalOp(struct Gio* fp) {
  gprintf(fp, "*NaClIllegal*");
}

/* Print out the register (from the list of register names),
 * that is referenced by the decoded instruction in the
 * decoder state. If is_gp_regs is true, the register names
 * correspond to general purpose register names. Otherwise,
 * they are some other set, such as MMX or XMM.
 */
static void RegMemPrint(const NCDecoderInst *dinst,
                        const char* reg_names[],
                        const uint8_t is_gp_regs,
                        struct Gio* fp) {
  DEBUG( printf(
             "reg mem print: sib_offset = %d, "
             "disp_offset = %d, mrm.mod = %02x\n",
             NCSibOffset(dinst), (int) NCDispOffset(dinst),
             modrm_modInline(dinst->inst.mrm)) );
  switch (modrm_modInline(dinst->inst.mrm)) {
    case 0:
      SegPrefixPrint(dinst, fp);
      if (NaClHasBit(dinst->inst.prefixmask, kPrefixADDR16)) {
        NaClIllegalOp(fp);
      } else {
        if (4 == modrm_rmInline(dinst->inst.mrm)) {
          SibPrint(dinst, fp);
        } else if (5 == modrm_rmInline(dinst->inst.mrm)) {
          gprintf(fp, "[0x%x]", DispValue32(dinst));
        } else {
          gprintf(fp, "[%s]", gp_regs[modrm_rmInline(dinst->inst.mrm)]);
        }
      }
      break;
    case 1:
      SegPrefixPrint(dinst, fp);
      if (NaClHasBit(dinst->inst.prefixmask, kPrefixADDR16)) {
        NaClIllegalOp(fp);
      } else {
        gprintf(fp, "0x%x", DispValue32(dinst));
        if (4 == modrm_rmInline(dinst->inst.mrm)) {
          SibPrint(dinst, fp);
        } else {
          gprintf(fp, "[%s]", gp_regs[modrm_rmInline(dinst->inst.mrm)]);
        }
      }
      break;
    case 2:
      SegPrefixPrint(dinst, fp);
      if (NaClHasBit(dinst->inst.prefixmask, kPrefixADDR16)) {
        NaClIllegalOp(fp);
      } else {
        gprintf(fp, "0x%x", DispValue32(dinst));
        if (4 == modrm_rmInline(dinst->inst.mrm)) {
          SibPrint(dinst, fp);
        } else {
          gprintf(fp, "[%s]", gp_regs[modrm_rmInline(dinst->inst.mrm)]);
        }
      }
      break;
    case 3:
      if (is_gp_regs) {
        gprintf(fp, "%s", reg_names[state_modrm_reg(dinst)]);
      } else {
        gprintf(fp, "%s", reg_names[modrm_rmInline(dinst->inst.mrm)]);
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
                       const NCDecoderInst *dinst,
                       struct Gio* fp) {
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
      gprintf(fp, ", ");
    } else if (pos > 0) {
      gprintf(fp, " ");
    }
    if ('$' == token[0]) {
      NaClMRMGroups group = ParseGroupName(token+1);
      if (NOGROUP != group) {
        int mrm = modrm_regInline(dinst->inst.mrm);
        const char* opname = kDisasmModRMOp[group][mrm];
        DEBUG( printf("case: group %d, opname = %s\n", group, opname) );
        gprintf(fp, "%s", opname);
      } else {
        /* Tokens starting with a $ but not $group need formatting */
        DEBUG( printf("case: $ and not group\n") );
        switch (token[1]) {
          case 'A':
            gprintf(fp, "$A");
            break;
          case 'C':
            gprintf(fp, "%%cr%d", modrm_regInline(dinst->inst.mrm));
            break;
          case 'D':
            gprintf(fp, "%%dr%d", modrm_regInline(dinst->inst.mrm));
            break;
          case 'E':
          case 'M': /* mod should never be 3 for 'M' */
            /* TODO(sehr): byte and word accesses */
            RegMemPrint(dinst, gp_regs, 1, fp);
            break;
          case 'F':
            gprintf(fp, "eflags");
            break;
          case 'G':
            gprintf(fp, "%s", gp_regs[modrm_regInline(dinst->inst.mrm)]);
            break;
          case 'I':
            gprintf(fp, "0x%"NACL_PRIx64, ImmedValue64(dinst));
            break;
          case 'J':
            gprintf(fp, "0x%"NACL_PRIxNaClPcAddress,
                    NCPrintableInstructionAddress(dinst)
                    + dinst->inst.bytes.length
                    + ImmedValue32(dinst));
            break;
          case 'O':
            gprintf(fp, "[0x%"NACL_PRIx64"]", ImmedValue64(dinst));
            break;
          case 'P':
            if ('R' == token[2]) {
              gprintf(fp, "%%mm%d", modrm_rmInline(dinst->inst.mrm));
            } else {
              gprintf(fp, "%%mm%d", modrm_regInline(dinst->inst.mrm));
            }
            break;
          case 'Q':
            RegMemPrint(dinst, mmx_regs, 0, fp);
            break;
          case 'R':
            gprintf(fp, "%s", gp_regs[modrm_rmInline(dinst->inst.mrm)]);
            break;
          case 'S':
            gprintf(fp, "%s", seg_regs[modrm_regInline(dinst->inst.mrm)]);
            break;
          case 'V':
            if ('R' == token[2]) {
              gprintf(fp, "%%xmm%d", modrm_rmInline(dinst->inst.mrm));
            } else {
              gprintf(fp, "%%xmm%d", modrm_regInline(dinst->inst.mrm));
            }
            break;
          case 'W':
            RegMemPrint(dinst, xmm_regs, 0, fp);
            break;
          case 'X':
            gprintf(fp, "ds:[esi]");
            break;
          case 'Y':
            gprintf(fp, "es:[edi]");
            break;
          default:
            gprintf(fp, "token('%s')", token);
            break;
        }
      }
    } else {
      /* Print the token as is */
      gprintf(fp, "%s", token);
    }
    fmt = NULL;
    ++pos;
  }
}

static void NCPrintInstTextUsingFormat(const char* format,
                                       const NCDecoderInst *dinst,
                                       struct Gio* fp) {
  InstFormat(format, dinst, fp);
  gprintf(fp, "\n");
}

void NCPrintInstWithoutHex(const NCDecoderInst *dinst, struct Gio* fp) {
  DEBUG( printf("use format: %s\n", DisFmt(dinst)) );
  NCPrintInstTextUsingFormat(DisFmt(dinst), dinst, fp);
}

void NCPrintInstWithHex(const NCDecoderInst *dinst, struct Gio *fp) {
  int i;
  DEBUG( printf("use format: %s\n", DisFmt(dinst)) );
  gprintf(fp, " %"NACL_PRIxNaClPcAddress":\t%02x",
          NCPrintableInstructionAddress(dinst),
          NCInstBytesByte(&dinst->inst_bytes, 0));
  for (i = 1; i < dinst->inst.bytes.length; i++) {
    gprintf(fp, " %02x", NCInstBytesByte(&dinst->inst_bytes, i));
  }
  for (i = dinst->inst.bytes.length; i < 7; i++) gprintf(fp, "   ");
  gprintf(fp, "\t");
  NCPrintInstWithoutHex(dinst, fp);
}

static Bool PrintInstLogGio(const NCDecoderInst *dinst) {
  NCPrintInstWithHex(dinst, NaClLogGetGio());
  return TRUE;
}

/* Defines a buffer size big enough to hold an instruction. */
#define MAX_INST_TEXT_SIZE 256

/* Defines an instruction print function. */
typedef void (*inst_print_fn)(const struct NCDecoderInst* inst,
                              struct Gio *file);

/* Print out the given instruction, using the given instruction print function,
 * and return the printed text as a (malloc allocated) string.
 */
static char* InstToStringConvert(const NCDecoderInst* dinst,
                                 inst_print_fn print_fn) {
  /* Print to a memory buffer, and then duplicate. */
  struct GioMemoryFile filemem;
  struct Gio *file = (struct Gio*) &filemem;
  char buffer[MAX_INST_TEXT_SIZE];
  char* result;

  /* Note: Be sure to leave an extra byte to add the null character to
   * the end of the string.
   */
  GioMemoryFileCtor(&filemem, buffer, MAX_INST_TEXT_SIZE - 1);
  print_fn(dinst, file);
  buffer[filemem.curpos < MAX_INST_TEXT_SIZE
         ? filemem.curpos : MAX_INST_TEXT_SIZE] ='\0';
  result = strdup(buffer);
  GioMemoryFileDtor(file);
  return result;
}

char* NCInstWithHexToString(const NCDecoderInst* dinst) {
  return InstToStringConvert(dinst, NCPrintInstWithHex);
}

char* NCInstWithoutHexToString(const NCDecoderInst* dinst) {
  return InstToStringConvert(dinst, NCPrintInstWithoutHex);
}

void NCDecodeSegment(uint8_t* mbase, NaClPcAddress vbase,
                     NaClMemorySize size) {
  NCDecoderInst inst;
  NCDecoderState dstate;
  NCDecoderStateConstruct(&dstate, mbase, vbase, size, &inst, 1);
  NCDecoderStateSetErrorReporter(&dstate, &kNCVerboseErrorReporter);
  /* TODO(karl): Fix this so that we don't need to override the
   * action function.
   */
  dstate.action_fn = PrintInstLogGio;
  NCDecoderStateDecode(&dstate);
  NCDecoderStateDestruct(&dstate);
}

static void NCVerboseErrorPrintInst(NaClErrorReporter* self,
                                    NCDecoderInst* dinst) {
  PrintInstLogGio(dinst);
}

NaClErrorReporter kNCVerboseErrorReporter = {
  NCDecoderInstErrorReporter,
  NaClVerboseErrorPrintf,
  NaClVerboseErrorPrintfV,
  (NaClPrintInst) NCVerboseErrorPrintInst
};
