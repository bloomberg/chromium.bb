/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* enuminsts.c
 * exhaustive instruction enumeration test for x86 Native Client decoder.
 */

/* TODO(karl) - Fix the calls to the decoder for x86-32 to use the same decoder
 * as the x86-32 validator, and document how to properly test the
 * x86-32 decoder.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "xed-interface.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/utils/flags.h"
#include "native_client/src/trusted/validator/x86/decoder/nc_inst_state.h"
#include "native_client/src/trusted/validator_x86/ncenuminsts.h"
#include "native_client/src/trusted/validator/x86/decoder/ncop_exps.h"
#include "native_client/src/trusted/validator/x86/decoder/ncopcode_desc.h"

#define kDebug 0
#define kBufferSize 1024
static int gVerbose = 0;
static int gSilent = 0;
static int gSkipRepeatReports = 0;
static int gCheckOperands = 0;
/* Count of errors that have a high certainty of being exploitable. */
static int gSawLethalError = 0;
/* Defines the assumed text address for the test instruction */
const int kTextAddress = 0x1000;
/* Defines if only opcode bytes should be printed. */
static Bool gPrintOpcodeBytesOnly = FALSE;
/* Defines if we should just print out the translations of nacl instructions. */
static int gPrintNaClInsts = 0;
/* Defines if we should just print out the translations of xed instructions. */
static int gPrintXedInsts = 0;
/* If non-negative, defines the prefix to test. */
static int gPrefix = -1;
/* If non-negative, defines the opcode to test. */
static int gOpcode = -1;
/* If true, check if nacl instruction is NACL legal. */
static Bool gNaClLegal = FALSE;
/* If true, check if nacl instruction is also implemented in xed. */
static Bool gXedImplemented = FALSE;
/* If true, don't bother to do operand compares for nop's. */
static Bool gNoCompareNops = FALSE;
/* If true, only skip contiguous errors. */
static Bool gSkipContiguous = FALSE;

static const char* target_machine = "x86-"
#if NACL_TARGET_SUBARCH == 64
    "64"
#else
    "32"
#endif
    ;

static char *gArgv0 = "argv0";
static void Usage() {
  fprintf(stderr, "usage: %s [options] [hexbytes]\n", gArgv0);
  fprintf(stderr, "    Compare Xed and NaCl %s instruction decoders\n",
          target_machine);
  fprintf(stderr, "    With no argument, enumerate all %s instructions\n",
          target_machine);
  fprintf(stderr, "    --checkoperands: enables operand comparison (slow)\n");
  fprintf(stderr, "    --nacllegal: only compare NaCl legal instructions\n");
  fprintf(stderr, "    --xedimplemented: only compare NaCl instruction that "
          "are also implemented in xed\n");
  fprintf(stderr,
          "    --naclprint: prints out set of enumerated nacl instructions\n");
  fprintf(stderr, "    --prefix=XX: only process given prefix\n");
  fprintf(stderr, "    --opcode=XX: only process given opcode for "
          "each prefix\n");
  fprintf(stderr, "    --opcode_bytes: Enumerate opcode bytes found by "
          "NaCl disassembler\n");
  fprintf(stderr, "    --nops: Don't operand compare nops.\n");
  fprintf(stderr, "    --skipcontiguous: Only skip contiguous errors\n");
  fprintf(stderr, "    --xedprint: prints out set of enumerated xed "
          "instructions\n");
  exit(1);
}

static void InternalError(const char *why) {
  if (!gPrintOpcodeBytesOnly) {
    fprintf(stderr, "%s: Internal Error: %s\n", gArgv0, why);
  }
  gSawLethalError = 1;
}

/* The following are a set of simple ASCII string processing routines
 * used to parse/transform Xed and NaCl disassembler output.
 */
/* If string s begins with string prefix, return a pointer to the
 * first byte after the prefix. Else return s.
 * PRECONDITION: plen == strlen(prefix)
 */
static inline char *SkipPrefix(char *s, const char *prefix, int plen) {
  if (strncmp(s, prefix, plen) == 0) {
    return &s[plen + 1];
  }
  return s;
}

/* Return a pointer to s with leading spaces removed.
 */
static inline char *strip(char *s) {
  while (*s == ' ') s++;
  return s;
}

/* Updates the string by removing trailing spaces/newlines. */
static inline void rstrip(char *s) {
  int slen = strlen(s);
  while (slen > 0) {
    --slen;
    if (!isspace(s[slen])) return;
    s[slen] = '\0';
  }
}

/* Find substring ss in string s. Returns a pointer to the substring
 * on success, NULL on failure.
 * PRECONDITION: sslen == strlen(ss)
 */
static char *strfind(char *s, const char *ss, int sslen) {
  int i;
  int slen = strlen(s);

  for (i = 0; i < slen; i++) {
    if (s[i] == ss[0]) {
      if (strncmp(&s[i], ss, sslen) == 0) {
        return &s[i];
      }
    }
  }
  return NULL;
}

/* If string ss appears in string s, return a pointer to the first byte
 * after ss. Otherwise return NULL.
 */
static char *strskip(char *s, const char *ss) {
  int sslen = strlen(ss);
  char *tmp = strfind(s, ss, sslen);
  if (tmp != NULL) {
    return &tmp[sslen];
  }
  return NULL;
}

/* Remove all instances of character c in string s.
 * PRECONDITION: slen == strlen(s)
 */
static void strnzapchar(char *s, const char c, const int slen) {
  int i, nskip;
  char *t = s;

  if (0 == c) return;
  nskip = 0;
  for (i = 0; i + nskip <= slen; i++) {
    while (s[i + nskip] == c) nskip += 1;
    s[i] = s[i+nskip];
  }
}

/* Remove all instances of substring ss in string s, modifying s in place.
 * PRECONDITION: sslen = strlen(ss).
 */
static inline void CleanString(char *s, const char *ss, int sslen) {
  char *fs = strfind(s, ss, sslen);

  while (fs != NULL) {
    do {
      fs[0] = fs[sslen];
      fs++;
    } while (*fs != '\0');
    fs = strfind(s, ss, sslen);
  }
}

static inline void TestCleanString(char *s, const char *ss) {
  char *stmp = strdup(s);
  printf("CleanString(%s, %s) => ", stmp, ss);
  CleanString(stmp, ss, strlen(ss));
  printf("%s\n", stmp);
}

/* Copy src to dest, stoping at character c.
 */
static void strncpyto(char *dest, const char *src, size_t n, char c) {
  int i;

  i = 0;
  while (1) {
    dest[i] = src[i];
    if (dest[i] == c) {
      dest[i] = 0;
      break;
    }
    if (dest[i] == '\0') break;
    i++;
    if (i == n) InternalError("really long opcode");
  }
}

/* This struct holds state concerning an instruction, both from Xed
 * and from the NaCl decoder. Some of the state information is redundant,
 * preserved to avoid having to recompute it.
 */
typedef struct {
  uint8_t _itext[XED_MAX_INSTRUCTION_BYTES];
  size_t _nbytes;
  xed_state_t _xed_state;
  xed_decoded_inst_t _xedd;
  NaClInstStruct *_nacl_inst;
  Bool _nacl_legal;
  Bool _decoder_error;
  int _has_xed_disasm;
  int _has_nacl_disasm;
  char _xed_disasm[kBufferSize];
  char _nacl_disasm[kBufferSize];
  char _nacl_fast_disasm[kBufferSize];
  char _xed_opcode[kBufferSize];
  char _nacl_opcode[kBufferSize];
  char* _after_xed_opcode;   /* stores location after opcode in _xed_opcode */
  char* _after_nacl_opcode;  /* stores location after opcode in _nacl_opcode */
} ComparedInstruction;

/* Initialize a ComparedInstruction cinst with the instruction
 * corresponding to itext. nbytes is the length of the input bytes.
 * Commonly the decoded instruction will be shorter than nbytes.
 */
static void InitComparedInstruction(ComparedInstruction *cinst,
                                    uint8_t *itext, size_t nbytes) {
  /* note: nacl_inst is freed in ncdecode_helper.c */
  memcpy(cinst->_itext, itext, nbytes);
  cinst->_nbytes = nbytes;
  cinst->_nacl_inst = NULL;
  cinst->_nacl_legal = FALSE;
  cinst->_decoder_error = FALSE;
  cinst->_has_xed_disasm = 0;
  cinst->_has_nacl_disasm = 0;
  cinst->_xed_disasm[0] = 0;
  cinst->_nacl_disasm[0] = 0;
  cinst->_xed_opcode[0] = 0;
  cinst->_nacl_opcode[0] = 0;
  cinst->_after_xed_opcode = NULL;
  cinst->_after_nacl_opcode = NULL;

  xed_decoded_inst_set_input_chip(&cinst->_xedd, XED_CHIP_CORE2);
  xed_decoded_inst_zero_set_mode(&cinst->_xedd, &cinst->_xed_state);
  xed_error_enum_t xed_error = xed_decode
      (&cinst->_xedd, (const xed_uint8_t*)itext, nbytes);
  cinst->_nacl_inst = NaClParseInst(itext, nbytes, kTextAddress);
  if (gNaClLegal) {
    cinst->_nacl_legal =
        NaClInstValidates(itext,
                          NaClInstLength((cinst->_nacl_inst)),
                          kTextAddress,
                          cinst->_nacl_inst);
  }
}

/* Return NaCl disassembled version of this instruction.
 */
static char *GetNaClDisasm(ComparedInstruction *cinst) {
  char *stmp;
  if (!cinst->_has_nacl_disasm) {
    cinst->_has_nacl_disasm = 1;
    stmp = NaClInstToStr(cinst->_nacl_inst);
    strncpy(cinst->_nacl_disasm, stmp, kBufferSize);
    free(stmp);
  }
  return cinst->_nacl_disasm;
}

/* Return Xed disassembled version of this instruction.
 */
static char *GetXedDisasm(ComparedInstruction *cinst) {
  if (!cinst->_has_xed_disasm) {
    if (cinst->_xedd._decoded_length == 0) {
      strcpy(cinst->_xed_disasm, "[illegal instruction]");
    }
    xed_format_intel(&cinst->_xedd, cinst->_xed_disasm,
                     kBufferSize, kTextAddress);
    cinst->_has_xed_disasm = 1;
  }
  return cinst->_xed_disasm;
}

/* Prints out the found nacl instruction. */
static void PrintNaClInst(ComparedInstruction *cinst) {
  if (gPrintOpcodeBytesOnly) {
    /* Print out opcode sequence for NACL instruction. */
    uint8_t i;
    uint8_t len = NaClInstLength(cinst->_nacl_inst);
    for (i = 0; i < len; ++i) {
      printf("%02x", cinst->_itext[i]);
    }
    printf("\n");
  } else if (NaClInstDecodesCorrectly(cinst->_nacl_inst)) {
    printf(" NaCl: %s", GetNaClDisasm(cinst));
  }
}

/* Prints out the found xed instruction. */
static void PrintXedInst(ComparedInstruction *cinst) {
  if (cinst->_xedd._decoded_length) {
    int i;
    if (gPrintOpcodeBytesOnly) {
      for (i = 0; i < cinst->_nbytes; ++i) {
        printf("%02x", cinst->_itext[i]);
      }
      printf("\n");
    } else {
      printf("  XED: ");
      /* Since xed doesn't print out opcode sequence, and it is
       * useful to know, add it to the print out.
       */
      for (i = 0; i < cinst->_nbytes; ++i) {
        if (i < cinst->_xedd._decoded_length) {
          printf("%02x ", cinst->_itext[i]);
        } else {
          printf("   ");
        }
      }
      /* Note: spacing added so that it lines up with nacl output. */
      printf(":\t\t      %s\n", GetXedDisasm(cinst ));
    }
  }
}

static void CinstInternalError(ComparedInstruction* cinst, const char* why) {
  if (!gPrintOpcodeBytesOnly) {
    PrintXedInst(cinst);
    PrintNaClInst(cinst);
  }
  InternalError(why);
}

static char *GetXedOpcodeAux(ComparedInstruction *cinst) {
  int char0 = 0;
  char *xtmp = GetXedDisasm(cinst);
  char *prev_xtmp = xtmp;

  /* Loop while prefixes found (don't want to figure out all combinations). */
  while (1) {
    xtmp = SkipPrefix(xtmp, "lock", strlen("lock"));
    xtmp = SkipPrefix(xtmp, "repne", strlen("repne"));
    xtmp = SkipPrefix(xtmp, "rep", strlen("rep"));
    xtmp = SkipPrefix(xtmp, "hint-not-taken", strlen("hint-not-taken"));
    xtmp = SkipPrefix(xtmp, "hint-taken", strlen("hint-taken"));
    xtmp = SkipPrefix(xtmp, "addr16", strlen("addr16"));
    xtmp = SkipPrefix(xtmp, "addr32", strlen("addr32"));
    xtmp = SkipPrefix(xtmp, "data16", strlen("data16"));
    if (xtmp == prev_xtmp) break;
    prev_xtmp = xtmp;
  }
  strncpyto(cinst->_xed_opcode, xtmp, kBufferSize - char0, ' ');
  cinst->_after_xed_opcode = xtmp + strlen(cinst->_xed_opcode);
  return cinst->_xed_opcode;
}

/* Note that NaClOpcodeName commonly returns mixed case; we make
 * it lower case to facilitate strcmp comparison.
 */
static char *GetNaClOpcodeAux(ComparedInstruction *cinst) {
  const char *opcode = NaClOpcodeName(cinst->_nacl_inst);
  int i;
  for (i = 0; i < kBufferSize; i++) {
    cinst->_nacl_opcode[i] = tolower(opcode[i]);
    if (opcode[i] == '\0') break;
  }
  return cinst->_nacl_opcode;
}

/* Returns text containing mnemonic opcode name. */
static char *GetXedOpcode(ComparedInstruction *cinst) {
  if (cinst->_xed_opcode[0] != 0) return cinst->_xed_opcode;
  return GetXedOpcodeAux(cinst);
}

/* Returns the text after the mnemonic opcode. */
static char *GetXedOperands(ComparedInstruction* cinst) {
  /* Note: This code counts on caching done by GetXedOpcodeAux. */
  if (NULL == cinst->_after_xed_opcode) {
    GetXedOpcode(cinst);
  }
  return cinst->_after_xed_opcode;
}

/* Returns text containing mnemonic opcode name. */
static char *GetNaClOpcode(ComparedInstruction *cinst) {
  if (cinst->_nacl_opcode[0] != 0) return cinst->_nacl_opcode;
  return GetNaClOpcodeAux(cinst);
}

/* Returns the text after the mnemonic opcode. */
static char *GetNaClOperands(ComparedInstruction* cinst) {
  if (NULL == cinst->_after_nacl_opcode) {
    cinst->_after_nacl_opcode =
        strskip(GetNaClDisasm(cinst), GetNaClOpcode(cinst));
  }
  return cinst->_after_nacl_opcode;
}

/* Return partial NaCl disassembly of this instruction. Much
 * faster than GetNaClDisasm.
 */
static char *FastNaClDisasm(ComparedInstruction *cinst) {
  static char *h2a[256] = {
    "00", "01", "02", "03", "04", "05", "06", "07",
    "08", "09", "0a", "0b", "0c", "0d", "0e", "0f",
    "10", "11", "12", "13", "14", "15", "16", "17",
    "18", "19", "1a", "1b", "1c", "1d", "1e", "1f",
    "20", "21", "22", "23", "24", "25", "26", "27",
    "28", "29", "2a", "2b", "2c", "2d", "2e", "2f",
    "30", "31", "32", "33", "34", "35", "36", "37",
    "38", "39", "3a", "3b", "3c", "3d", "3e", "3f",
    "40", "41", "42", "43", "44", "45", "46", "47",
    "48", "49", "4a", "4b", "4c", "4d", "4e", "4f",
    "50", "51", "52", "53", "54", "55", "56", "57",
    "58", "59", "5a", "5b", "5c", "5d", "5e", "5f",
    "60", "61", "62", "63", "64", "65", "66", "67",
    "68", "69", "6a", "6b", "6c", "6d", "6e", "6f",
    "70", "71", "72", "73", "74", "75", "76", "77",
    "78", "79", "7a", "7b", "7c", "7d", "7e", "7f",
    "80", "81", "82", "83", "84", "85", "86", "87",
    "88", "89", "8a", "8b", "8c", "8d", "8e", "8f",
    "90", "91", "92", "93", "94", "95", "96", "97",
    "98", "99", "9a", "9b", "9c", "9d", "9e", "9f",
    "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
    "a8", "a9", "aa", "ab", "ac", "ad", "ae", "af",
    "b0", "b1", "b2", "b3", "b4", "b5", "b6", "b7",
    "b8", "b9", "ba", "bb", "bc", "bd", "be", "bf",
    "c0", "c1", "c2", "c3", "c4", "c5", "c6", "c7",
    "c8", "c9", "ca", "cb", "cc", "cd", "ce", "cf",
    "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
    "d8", "d9", "da", "db", "dc", "dd", "de", "df",
    "e0", "e1", "e2", "e3", "e4", "e5", "e6", "e7",
    "e8", "e9", "ea", "eb", "ec", "ed", "ee", "ef",
    "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",
    "f8", "f9", "fa", "fb", "fc", "fd", "fe", "ff"
  };
  static const int kSampleBytes = 6;
  char stmp[kBufferSize];
  int i;

  for (i = 0; i < kSampleBytes; i++) {
    stmp[i * 3] = h2a[cinst->_itext[i]][0];
    stmp[i * 3 + 1] = h2a[cinst->_itext[i]][1];
    stmp[i * 3 + 2] = ' ';
  }
  stmp[kSampleBytes * 3] = '\0';
  snprintf(cinst->_nacl_fast_disasm, kBufferSize, "%s...:  %s",
           stmp, GetNaClOpcode(cinst));
  return cinst->_nacl_fast_disasm;
}

#define NOT_AN_OPCODE "not an opcode";

/* Holds the name of the last (bad) opcode matched. */
static char last_bad_opcode[kBufferSize] = NOT_AN_OPCODE;

/* Returns how many decoder errors were skipped for an opcode. */
static int nSkipped = 0;

/* Reset the counters for skipping decoder errors, assuming
 * the last_bad_opcode (if non-null) was the given value.
 */
static void ResetSkipCounts(const char* last_opcode) {
  nSkipped = 0;
  strncpy(last_bad_opcode,
          (last_opcode == NULL) ? "not an opcode" : last_opcode,
          kBufferSize);
}

static void ReportOnSkippedErrors(const char* last_opcode) {
  if (nSkipped > 0 && !gPrintOpcodeBytesOnly) {
    printf("...skipped %d errors for %s\n", nSkipped, last_bad_opcode);
  }
  ResetSkipCounts(last_opcode);
}

/* Report a disagreement between the Xed and NaCl decoders. To reduce
 * noice from uninteresting related errors, gSkipRepeatReports will
 * avoid printing consecutive errors for the same opcode.
 */
static void DecoderError(const char *why,
                         ComparedInstruction *cinst,
                         const char *details) {
  /* get/fix NaCl opcode */
  cinst->_decoder_error = TRUE;
  if (gSkipRepeatReports) {
    if (strncmp(GetNaClOpcode(cinst), last_bad_opcode,
                strlen(last_bad_opcode)) == 0) {
      nSkipped += 1;
      return;
    } else {
      ReportOnSkippedErrors(GetNaClOpcode(cinst));
    }
  }

  if (!gPrintOpcodeBytesOnly) {
    printf("**** ERROR: %s: %s\n", why, (details == NULL ? "" : details));
    PrintXedInst(cinst);
    PrintNaClInst(cinst);
  }
}

/* Returns the integer corresponding to a hex value in ASCII. This would
 * be faster as an array lookup, however since it's only used for command
 * line input it doesn't matter.
 */
static unsigned int A2INibble(char nibble) {
  switch (nibble) {
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case 'a':
    case 'A': return 0xA;
    case 'b':
    case 'B': return 0xB;
    case 'c':
    case 'C': return 0xC;
    case 'd':
    case 'D': return 0xD;
    case 'e':
    case 'E': return 0xE;
    case 'f':
    case 'F': return 0xF;
    default: break;
  }
  fprintf(stderr, "bad hex value %c", nibble);
  Usage();
}

/* Convert a two-character string representing an byte value in hex
 * into the corresponding byte value.
 */
static unsigned int A2IByte(char nibble1, char nibble2) {
  return A2INibble(nibble2) + A2INibble(nibble1) * 0x10;
}

static int IsReservedReg(xed_reg_enum_t reg) {
  if (kDebug) printf("IsReservedReg(%s)\n", xed_reg_enum_t2str(reg));
  switch (reg) {
    case XED_REG_RSP:
    case XED_REG_RBP:
    case XED_REG_R15:
      return 1;
  }
  return 0;
}

static int IsWriteAction(xed_operand_action_enum_t rw) {
  if (kDebug) printf("IsWriteAction(%s)\n", xed_operand_action_enum_t2str(rw));
  switch (rw) {
    case XED_OPERAND_ACTION_RW:
    case XED_OPERAND_ACTION_W:
    case XED_OPERAND_ACTION_RCW:
    case XED_OPERAND_ACTION_CW:
    case XED_OPERAND_ACTION_CRW:
      return 1;
  }
  return 0;
}

/* The instructions:
 *    48 89 e5  mov  rbp, rsp
 *    4a 89 e5  mov  rbp, rsp
 * look bad based on the simple rule, but are safe because they are
 * moving a safe address between two protected registers.
 */
static int IsSpecialSafeRegWrite(ComparedInstruction *cinst) {
  uint8_t byte0 = cinst->_itext[0];
  uint8_t byte1 = cinst->_itext[1];
  uint8_t byte2 = cinst->_itext[2];

  return ((byte0 == 0x48 && byte1 == 0x89 && byte2 == 0xe5) ||
          (byte0 == 0x48 && byte1 == 0x89 && byte2 == 0xec) ||
          (byte0 == 0x48 && byte1 == 0x8b && byte2 == 0xe5) ||
          (byte0 == 0x48 && byte1 == 0x8b && byte2 == 0xec) ||
          (byte0 == 0x4a && byte1 == 0x89 && byte2 == 0xe5) ||
          (byte0 == 0x4a && byte1 == 0x89 && byte2 == 0xec) ||
          (byte0 == 0x4a && byte1 == 0x8b && byte2 == 0xe5) ||
          (byte0 == 0x4a && byte1 == 0x8b && byte2 == 0xec));
}

/* If we get this far, and Xed says it writes a NaCl reserved
 * register, this is a lethal error.
 */
static int CheckBadRegWrite(ComparedInstruction *cinst) {
  unsigned int i;
  xed_inst_t const *const xi = xed_decoded_inst_inst(&cinst->_xedd);
  unsigned int noperands = xed_inst_noperands(xi);

  for (i = 0; i < noperands ; i++) {
    xed_operand_t const* op = xed_inst_operand(xi, i);
    xed_operand_enum_t op_name = xed_operand_name(op);
    if (xed_operand_is_register(op_name)) {
      if (IsReservedReg(xed_decoded_inst_get_reg(&cinst->_xedd, op_name))
          && IsWriteAction(xed_operand_rw(op))) {
        /* check if NaCl thinks this is a legal instruction */
        if (NaClSegmentValidates(cinst->_itext,
                                 cinst->_xedd._decoded_length,
                                 kTextAddress)) {
          /* Report problem if NaCl accepted instruction, but is
           * not one of the special safe writes.
           */
          if (IsSpecialSafeRegWrite(cinst)) return 0;
          gSawLethalError = 1;
          DecoderError("ILLEGAL REGISTER WRITE", cinst, FastNaClDisasm(cinst));
          return 1;
        }
      }
    }
  }
  return 0;
}

/* Compare operands, reporting an error if Xed and NaCl disagree.
 * Returns 0 on success, non-zero for failure.
 */
static int CompareOperands(ComparedInstruction *cinst) {
  char *noperands = GetNaClOperands(cinst);
  char *xoperands = GetXedOperands(cinst);
  char xclean[kBufferSize];
  char nclean[kBufferSize];
  char sbuf[kBufferSize];

  if (noperands == NULL || xoperands == NULL) {
    CinstInternalError(cinst, "CompareOperands");
  }
  /* remove uninteresting decoration */
  strncpy(xclean, xoperands, kBufferSize);
  strncpy(nclean, noperands, kBufferSize);
  /* NOTE: these patterns need to be ordered from most to least specific */
  CleanString(xclean, "byte ptr ", strlen("byte ptr "));
  CleanString(xclean, "dword ptr ", strlen("dword ptr "));
  CleanString(xclean, "qword ptr ", strlen("qword ptr "));
  CleanString(xclean, "xmmword ptr ", strlen("xmmword ptr "));
  CleanString(xclean, "word ptr ", strlen("word ptr "));
  CleanString(xclean, "ptr ", strlen("ptr "));
  CleanString(xclean, "far ", strlen("far "));

  strnzapchar(nclean, '%', strlen(nclean));
  strnzapchar(nclean, '\n', strlen(nclean));

  if (strcmp(strip(xclean), strip(nclean)) != 0) {
    sprintf(sbuf, "'%s' != '%s'", xclean, nclean);
    DecoderError("OPERAND MISMATCH", cinst, sbuf);
    return 1;
  }
  return 0;
}

typedef struct {
  const char* nacl_name;
  const char* xed_name;
} NaClToXedPairs;

/* Returns the NaCl Opcode name for the instruction, or if there is
 * a different but equivalent name in xed, then return the equivalent
 * name used by xed.
 */
static const char* GetXedOpcodeEquivalent(ComparedInstruction *cinst) {
  const char* nacl_name = GetNaClOpcode(cinst);
  /* First try to fix misnamed instructions. */
  if ((nacl_name[0] == 'p') && (nacl_name[1] == 'f') && (nacl_name[2] == 'r')) {
    static const NaClToXedPairs pairs[] = {
      { "pfrsqrt", "pfsqrt" },
      { "pfrcpit1", "pfcpit1" }
    };
    int i;
    for (i = 0; i < NACL_ARRAY_SIZE(pairs); ++i) {
      if (0 == strcmp(nacl_name, pairs[i].nacl_name)) {
        return pairs[i].xed_name;
      }
    }
  }
  /* Now handle special cases where we should treat the nacl instruction as a
   * nop, so that they will match xed instructions.
   */
  if (gNoCompareNops && (0 == strcmp("nop", GetXedOpcode(cinst)))) {
    static const NaClToXedPairs pairs[] = {
      { "xchg %eax, %eax" , "nop" },
      { "xchg %rax, %rax" , "nop" },
      { "xchg %ax, %ax" , "nop" },
    };
    char buf[kBufferSize];
    int i;
    const char* opcode = GetNaClOpcode(cinst);
    const char* desc = strfind(GetNaClDisasm(cinst), opcode, strlen(opcode));
    strncpy(buf, desc, kBufferSize);
    rstrip(buf);
    for (i = 0; i < NACL_ARRAY_SIZE(pairs); ++i) {
      if (0 == strcmp(buf, pairs[i].nacl_name)) {
        return pairs[i].xed_name;
      }
    }
  }
  /* If reached, not a special case. Use NaCl opcode name.*/
  return nacl_name;
}

/* Compare opcodes, reporting an error if Xed and NaCl disagree.
 * Returns 0 on success, non-zero for failure.
 */
static int CompareOpcodes(ComparedInstruction *cinst) {
  char sbuf[kBufferSize];
  if (strncmp(GetXedOpcode(cinst),
              GetXedOpcodeEquivalent(cinst),
              kBufferSize) != 0) {
    snprintf(sbuf, kBufferSize, "%s != %s",
             GetXedOpcode(cinst), GetNaClOpcode(cinst));
    DecoderError("OPCODE MISMATCH", cinst, sbuf);
    return 1;
  } else {
    if (kDebug) printf("opcodes match: %s == %s\n",
                       GetXedOpcode(cinst), GetNaClOpcode(cinst));
    if (gNoCompareNops &&
        (strncmp("nop", GetXedOpcode(cinst), kBufferSize) == 0)) {
      return 1;
    }
    return 0;
  }
}

static Bool CheckNaClValid(ComparedInstruction *cinst) {
  return NaClInstDecodesCorrectly(cinst->_nacl_inst) &&
      ((!gNaClLegal) || cinst->_nacl_legal);
}

static int NaClIsntXedImplemented(NaClInstStruct *inst) {
  static const char* nacl_but_not_xed[] = {
    "Pf2iw",
    "Pf2id"
  };
  const char* name = NaClOpcodeName(inst);
  int i;
  for (i = 0; i < NACL_ARRAY_SIZE(nacl_but_not_xed); ++i) {
    if (0 == strcmp(name, nacl_but_not_xed[i])) {
      return 0;
    }
  }
  return 1;
}

/* Compare instruction validity. Reports a failure if NaCl sees an
 * valid instruction when Xed does not. Returns 0 on success, non-zero
 * for failure.
 */
static int CheckValidInstruction(ComparedInstruction *cinst) {
  if (CheckNaClValid(cinst)) {
    if (cinst->_xedd._decoded_length == 0) {
      if (!gXedImplemented || NaClIsntXedImplemented(cinst->_nacl_inst)) {
        DecoderError("ILLEGAL INSTRUCTION", cinst, FastNaClDisasm(cinst));
      }
      return 1;
    }
  } else {
    /* NaCl sees an illegal instruction; assume it's deliberate. */
    return 1;
  }
  /* legal instruction for both decoders */
  return 0;
}

/* Compare instruction length, reporting an error if Xed and NaCl disagree.
 * Returns 0 on success, non-zero for failure.
 */
static int CompareInstructionLength(ComparedInstruction *cinst) {
  char sbuf[kBufferSize];

  if (cinst->_xedd._decoded_length != NaClInstLength(cinst->_nacl_inst)) {
    snprintf(sbuf, kBufferSize, "%d != %d",
             cinst->_xedd._decoded_length,
             NaClInstLength(cinst->_nacl_inst));
    DecoderError("LENGTH MISMATCH", cinst, sbuf);
    gSawLethalError = 1;
    return 1;
  } else {
    if (kDebug) printf("length match: %d == %d\n",
                       cinst->_xedd._decoded_length,
                       NaClInstLength(cinst->_nacl_inst));
    return 0;
  }
}

/* Print out the instruction for the tool(s) specified on the
 * command line. Returns true when the instruction has been
 * printed.
 */
static Bool PrintInst(ComparedInstruction *cinst) {
  Bool result = FALSE;
  int i;
  if (gPrintNaClInsts) {
    PrintNaClInst(cinst);
    result = TRUE;
  }
  if (gPrintXedInsts) {
    PrintXedInst(cinst);
    result = TRUE;
  }
  return result;
}

/* Test comparison for a single instruction.
 */
static void TryOneInstruction(ComparedInstruction *cinst,
                              uint8_t *itext, size_t nbytes) {
  InitComparedInstruction(cinst, itext, nbytes);
  do {
    if (CheckValidInstruction(cinst)) break;
    if (CompareInstructionLength(cinst)) break;
    if (CompareOpcodes(cinst)) break;
    if (gCheckOperands) {
      if (CompareOperands(cinst)) break;
    }
#if NACL_TARGET_SUBARCH == 64
    if (CheckBadRegWrite(cinst)) break;
#endif
    if (PrintInst(cinst)) break;
    /* no error */
    if (gVerbose) {
      int i;
      printf("================");
      for (i = 0; i < nbytes; ++i) {
        printf("%02x", itext[i]);
      }
      printf("\n");
      PrintXedInst(cinst);
      PrintNaClInst(cinst);
    }
  } while (0);
  /* saw error; should have already printed stuff. Only
   * report on skipped errors if we found an error-free
   * instruction.
   */
  if (gSkipContiguous && (!cinst->_decoder_error)) {
    ReportOnSkippedErrors(NULL);
  }
}

static int ValidInstIsShorterThan(ComparedInstruction *cinst, int len) {
  return ((cinst->_xedd._decoded_length > 0) &&
          (cinst->_xedd._decoded_length < len));
}

/* Enumerate and test all 24-bit opcode+modrm+sib patterns for a
 * particular prefix.
 */
static void TestAllWithPrefix(ComparedInstruction *cinst,
                              int prefix, int prefix_length) {
  const int kFillerByteCount = 15;
  const int kInstByteCount = 15;
  const int kIterByteCount = 3;
  uint8_t itext[kBufferSize];
  int i, op, modrm, sib;
  int min_op;
  int max_op;

  if ((gPrefix >= 0) && (gPrefix != prefix)) return;

  if (!gSilent) printf("TestAllWithPrefix(%x)\n", prefix);
  /* set up prefix */
  memcpy(itext, &prefix, prefix_length);
  /* set up filler bytes */
  for (i = 0; i < kFillerByteCount; i++) {
    itext[i + prefix_length + kIterByteCount] = (uint8_t)i;
  }
  if (gOpcode < 0) {
    min_op = 0;
    max_op = 256;
  } else {
    min_op = gOpcode;
    max_op = gOpcode + 1;
  }
  for (op = min_op; op < max_op; op++) {
    itext[prefix_length] = op;
    ResetSkipCounts(NULL);
    if (!gSilent) printf("%02x 00 00\n", op);
    for (modrm = 0; modrm < 256; modrm++) {
      itext[prefix_length + 1] = modrm;
      for (sib = 0; sib < 256; sib++) {
        itext[prefix_length + 2] = sib;
        TryOneInstruction(cinst, itext, kInstByteCount);
        if (ValidInstIsShorterThan(cinst, prefix_length + 3)) break;
      }
      if (ValidInstIsShorterThan(cinst, prefix_length + 2)) break;
    }
    ReportOnSkippedErrors(NULL);
  }
}

/* For all prefixes, call TestAllWithPrefix() to enumrate and test
 * all instructions.
 */
static void TestAllInstructions(ComparedInstruction *cinst) {
  gSkipRepeatReports = 1;
  /* NOTE: Prefix byte order needs to be reversed when written as
   * an integer. For example, for integer prefix 0x3a0f, 0f will
   * go in instruction byte 0, and 3a in byte 1.
   */
  TestAllWithPrefix(cinst, 0, 0);
  TestAllWithPrefix(cinst, 0x0f, 1);
  TestAllWithPrefix(cinst, 0x0ff2, 2);
  TestAllWithPrefix(cinst, 0x0ff3, 2);
  TestAllWithPrefix(cinst, 0x0f66, 2);
  TestAllWithPrefix(cinst, 0x0f0f, 2);
  TestAllWithPrefix(cinst, 0x380f, 2);
  TestAllWithPrefix(cinst, 0x3a0f, 2);
  TestAllWithPrefix(cinst, 0x380f66, 3);
  TestAllWithPrefix(cinst, 0x380ff2, 3);
  TestAllWithPrefix(cinst, 0x3a0f66, 3);
}

void XedSetup(ComparedInstruction *cinst) {
  xed_tables_init();
  xed_state_zero(&cinst->_xed_state);

  /* dstate.stack_addr_width = XED_ADDRESS_WIDTH_32b; */
#if (NACL_TARGET_SUBARCH == 32)
  cinst->_xed_state.mmode = XED_MACHINE_MODE_LONG_COMPAT_32;
#endif
#if (NACL_TARGET_SUBARCH == 64)
  cinst->_xed_state.mmode = XED_MACHINE_MODE_LONG_64;
#endif
}

/* Used to test one instruction at a time, for example, in regression
 * testing, or for instruction arguments from the command line.
 */
static void TestOneInstruction(ComparedInstruction *cinst, char *asciihex) {
  unsigned char ibytes[XED_MAX_INSTRUCTION_BYTES];
  unsigned char *ibp;
  unsigned int i, len, nbytes;

  len = strlen(asciihex);
  nbytes = len / 2;
  if (nbytes * 2 != len) {
    fprintf(stderr, "bad instruction %s\nMust be an even number of chars.\n",
            asciihex);
    Usage();
  }
  if (nbytes > XED_MAX_INSTRUCTION_BYTES) {
    fprintf(stderr, "bad instruction %s\nMust be less than %d bytes\n",
            asciihex, XED_MAX_INSTRUCTION_BYTES);
    Usage();
  }
  for (i = 0; i < len; i += 2) {
    ibytes[i/2] = A2IByte(asciihex[i], asciihex[i+1]);
  }
  if (gVerbose) {
    printf("trying %s (%02x%02x)\n", asciihex, ibytes[0], ibytes[1]);
  }
  TryOneInstruction(cinst, ibytes, nbytes);
}

/* A set of test cases that have caused problems in the past.
 * This is a bit stale; most of the test cases came from xed_compare.py.
 * Mostly this program has been tested by TestAllInstructions(),
 * possible since this program is much faster than xed_compare.py
 */
static void RunRegressionTests(ComparedInstruction *cinst) {
  TestOneInstruction(cinst, "0024c2");
  TestOneInstruction(cinst, "017967");
  TestOneInstruction(cinst, "0f12c0");
  TestOneInstruction(cinst, "0f13c0");
  TestOneInstruction(cinst, "0f17c0");
  TestOneInstruction(cinst, "0f01c1");
  TestOneInstruction(cinst, "0f00300000112233445566778899aa");
  TestOneInstruction(cinst, "cc");
  TestOneInstruction(cinst, "C3");
  TestOneInstruction(cinst, "0f00290000112233445566778899aa");
  TestOneInstruction(cinst, "80e4f7");
  TestOneInstruction(cinst, "e9a0ffffff");
  TestOneInstruction(cinst, "4883ec08");
  TestOneInstruction(cinst, "0f00040500112233445566778899aa");
  /* Below are newly discovered mistakes in call instructions, where the wrong
   * byte length was required by x86-64 nacl validator.
   */
  TestOneInstruction(cinst, "262e7e00");
  TestOneInstruction(cinst, "2e3e7900");
}

/* Very simple command line arg parsing. Returns index to the first
 * arg that doesn't begin with '--', or argc if there are none.
 */
static int ParseArgs(int argc, char *argv[]) {
  int i;
  uint32_t prefix;
  uint32_t opcode;

  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      do {
        if (strncmp(argv[i], "--checkoperands",
                    strlen("--checkoperands")) == 0) {
          printf("checking operands\n");
          gCheckOperands = 1;
          break;
        } else if (GrokBoolFlag("--nacllegal", argv[i],
                                &gNaClLegal) ||
                   GrokBoolFlag("--xedimplemented", argv[i],
                                &gXedImplemented) ||
                   GrokBoolFlag("--nops", argv[i], &gNoCompareNops)||
                   GrokBoolFlag("--opcode_bytes", argv[i],
                                &gPrintOpcodeBytesOnly) ||
                   GrokBoolFlag("--skipcontiguous", argv[i],
                                &gSkipContiguous)) {
          break;
        } else if (strncmp(argv[i], "--naclprint",
                           strlen("--naclprint")) == 0) {
          printf("printing nacl instructions:\n");
          gPrintNaClInsts = 1;
          break;
        } else if (strncmp(argv[i], "--xedprint",
                           strlen("--xedprint")) == 0) {
          gPrintXedInsts = 1;
          break;
        } else if (GrokUint32HexFlag("--prefix", argv[i], &prefix)) {
          gPrefix = (int) prefix;
          printf("using prefix %x\n", gPrefix);
          break;
        } else if (GrokUint32HexFlag("--opcode", argv[i], &opcode) &&
                   opcode < 256) {
          gOpcode = (int) opcode;
          printf("using opcode %x\n", gOpcode);
          break;
        }
        Usage();
      } while (0);
    } else return i;
  }
  return argc;
}

int main(int argc, char *argv[]) {
  int testargs;
  ComparedInstruction cinst;

  gArgv0 = argv[0];
  XedSetup(&cinst);

  testargs = ParseArgs(argc, argv);

  if (gPrintOpcodeBytesOnly) {
    gSilent = 1;
  }

  NaClLogModuleInit();
  NaClLogSetVerbosity(LOG_FATAL);
  if (testargs == argc) {
    if (gPrefix < 0) RunRegressionTests(&cinst);
    TestAllInstructions(&cinst);
  } else if (testargs == argc - 1) {
    gSilent = 0;
    gVerbose = 1;
    TestOneInstruction(&cinst, argv[testargs]);
  } else Usage();
  exit(gSawLethalError);
}
