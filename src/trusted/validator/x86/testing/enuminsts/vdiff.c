/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* vdiff.c
 * exhaustive instruction enumeration test for x86 Native Client validators.
 *
 * This file is based on enuminsts.c, but specialized to comparing two
 * validators instead of decoders. The enuminsts.c implementation also
 * had a bunch of Xed-specific logic which complicated the validator
 * comparison in unhelpful ways.
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB.")
#endif

#include "native_client/src/trusted/validator/x86/testing/enuminsts/enuminsts.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/utils/flags.h"
#include "native_client/src/trusted/validator/x86/testing/enuminsts/str_utils.h"
#include "native_client/src/trusted/validator/x86/testing/enuminsts/text2hex.h"

/* Defines the maximum buffer size used to hold text generated for the
 * disassembly of instructions, and corresponding error messages.
 */
#define kBufferSize 1024

/* When true, print more messages (i.e. verbosely). */
static Bool gVerbose = FALSE;

/* When true, don't print out messages. That is, only print instructions
 * defined by --print directives.
 */
static Bool gSilent = FALSE;

/* When true, don't report consecutive errors for consecutive instructions
 * with the same instruction mnemonic.
 */
static Bool gSkipRepeatReports = FALSE;

/* Count of errors that have a high certainty of being exploitable. */
static int gSawLethalError = 0;

/* Defines the assumed text address for the test instruction */
const int kTextAddress = 0x1000;

/* If non-negative, defines the prefix to test. */
static unsigned int gPrefix = 0;

/* If non-negative, defines the opcode to test. */
static int gOpcode = -1;

/* The production and new R-DFA validators */
NaClEnumeratorDecoder* vProd;
NaClEnumeratorDecoder* vDFA;

/* The name of the executable (i.e. argv[0] from the command line). */
static char *gArgv0 = "argv0";

/* Records that unexpected internal error occurred. */
void InternalError(const char *why) {
  fprintf(stderr, "%s: Internal Error: %s\n", gArgv0, why);
  gSawLethalError = 1;
}

/* Records that a fatal (i.e. non-recoverable) error occurred. */
void ReportFatalError(const char* why) {
  char buffer[kBufferSize];
  SNPRINTF(buffer, kBufferSize, "%s - quitting!", why);
  InternalError(buffer);
  exit(1);
}

/* Prints out the instruction each decoder disassembled */
static void PrintDisassembledInstructionVariants(NaClEnumerator *pinst,
                                                 NaClEnumerator *dinst) {
  vProd->_print_inst_fn(pinst);
  vDFA->_print_inst_fn(dinst);
}

/* Prints out progress messages. */
static void PrintVProgress(const char* format, va_list ap) {
  if (gSilent) {
    /* Generating opcode sequences, so add special prefix so that we
     * can print these out when read by the corresponding input decoder.
     */
    printf("#PROGRESS#");
  }
  vprintf(format, ap);
}

static void PrintProgress(const char* format, ...) ATTRIBUTE_FORMAT_PRINTF(1,2);

/* Prints out progress messages. */
static void PrintProgress(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  PrintVProgress(format, ap);
  va_end(ap);
}

/* Report a disagreement between decoders. To reduce
 * noice from uninteresting related errors, gSkipRepeatReports will
 * avoid printing consecutive errors for the same opcode.
 */
static void DecoderError(const char *why,
                         NaClEnumerator *pinst,
                         NaClEnumerator *dinst,
                         const char *details) {
  /* If reached, did not skip, so report the error. */
  printf("**** ERROR: %s: %s\n", why, (details == NULL ? "" : details));
  PrintDisassembledInstructionVariants(pinst, dinst);
}

static void PrintBytes(FILE *f, uint8_t* bytes, size_t len) {
  size_t i;
  for (i = 0; i < len; i++) {
    fprintf(f, "%02x", bytes[i]);
  }
}

static Bool NotOpcodeRepeat(const char* opcode) {
  static char last_opcode[kBufferSize] = "";
  Bool result = (strcmp(last_opcode, opcode) != 0);
  strncpy(last_opcode, opcode, kBufferSize);
  return result;
}

static void CheckMnemonics(NaClEnumerator* pinst, NaClEnumerator* dinst) {
  const char* prod_opcode = vProd->_get_inst_mnemonic_fn(pinst);
  const char* dfa_opcode = vDFA->_get_inst_mnemonic_fn(dinst);

  if (0 != strcmp(prod_opcode, dfa_opcode)) {
    /* avoid redundant messages... */
    if (NotOpcodeRepeat(prod_opcode)) {
      printf("Warning: OPCODE MISMATCH: %s != %s\n", prod_opcode, dfa_opcode);
      /* PrintDisassembledInstructionVariants(pinst, dinst); */
    }
  }
}

struct vdiff_stats {
  int64_t tried;
  int64_t valid;
  int64_t invalid;
  int64_t errors;
} gVDiffStats = {0, 0, 0, 0};

static void IncrTried() {
  gVDiffStats.tried += 1;
}

static void IncrValid() {
  gVDiffStats.valid += 1;
}

static void IncrInvalid() {
  gVDiffStats.invalid += 1;
}

static void IncrErrors() {
  gVDiffStats.errors += 1;
}

static void PrintStats() {
  printf("valid: %" NACL_PRIu64 "\n", gVDiffStats.valid);
  printf("invalid: %" NACL_PRIu64 "\n", gVDiffStats.invalid);
  printf("errors: %" NACL_PRIu64 "\n", gVDiffStats.errors);
  printf("tried: %" NACL_PRIu64 "\n", gVDiffStats.tried);
  printf("    =? %" NACL_PRIu64 " valid + invalid + errors\n",
         gVDiffStats.valid + gVDiffStats.invalid + gVDiffStats.errors);
}

static void InitInst(NaClEnumerator *nacle,
                     uint8_t *itext, size_t nbytes)
{
  memcpy(nacle->_itext, itext, nbytes);
  nacle->_num_bytes = nbytes;
}

/* Print out decodings if specified on the command line. */
/* Test comparison for a single instruction. */
static void TryOneInstruction(uint8_t *itext, size_t nbytes) {
  NaClEnumerator pinst; /* for prod validator */
  NaClEnumerator dinst; /* for dfa validator */
  Bool prod_okay, rdfa_okay;

  IncrTried();
  do {
    if (gVerbose) {
      printf("================");
      PrintBytes(stdout, itext, nbytes);
      printf("\n");
    }

    /* Try to parse the sequence of test bytes. */
    InitInst(&pinst, itext, nbytes);
    InitInst(&dinst, itext, nbytes);
    vProd->_parse_inst_fn(&pinst, kTextAddress);
    vDFA->_parse_inst_fn(&dinst, kTextAddress);
    prod_okay = vProd->_maybe_inst_validates_fn(&pinst);
    rdfa_okay = vDFA->_maybe_inst_validates_fn(&dinst);

    if (prod_okay) {
      if (rdfa_okay) {
        if (vProd->_inst_length_fn(&pinst) ==
            vDFA->_inst_length_fn(&dinst)) {
          /* Both validators see a legal instruction, */
          /* and they agree on critical details.      */
          IncrValid();
          /* Warn if decoders disagree opcode name. */
          CheckMnemonics(&pinst, &dinst);
        } else {
          DecoderError("LENGTH MISMATCH", &pinst, &dinst, "");
          IncrErrors();
        }
      } else {
        /* Validators disagree on instruction legality */
        DecoderError("VALIDATORS DISAGREE", &pinst, &dinst, "");
        IncrErrors();
      }
    } else if (rdfa_okay) {
      /* Validators disagree on instruction legality */
      DecoderError("VALIDATORS DISAGREE", &pinst, &dinst, "");
      IncrErrors();
    } else {
      /* Both validators see an illegal instruction */
      IncrInvalid();
    }

    /* no error */
    if (gVerbose) {
      PrintDisassembledInstructionVariants(&pinst, &dinst);
    }
  } while (0);
}

/* Enumerate and test all 24-bit opcode+modrm+sib patterns for a
 * particular prefix.
 */
static void TestAllWithPrefix(unsigned int prefix, size_t prefix_length) {
  const int kInstByteCount = NACL_ENUM_MAX_INSTRUCTION_BYTES;
  const int kIterByteCount = 3;
  InstByteArray itext;
  size_t i;
  int op, modrm, sib;
  int min_op;
  int max_op;

  if ((gPrefix > 0) && (gPrefix != prefix)) return;

  PrintProgress("TestAllWithPrefix(%x)\n", prefix);
  /* set up prefix */
  memcpy(itext, &prefix, prefix_length);
  /* set up filler bytes */
  for (i = prefix_length + kIterByteCount;
       i < NACL_ENUM_MAX_INSTRUCTION_BYTES; i++) {
    itext[i] = (uint8_t)i;
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
    PrintProgress("%02x 00 00\n", op);
    for (modrm = 0; modrm < 256; modrm++) {
      itext[prefix_length + 1] = modrm;
      for (sib = 0; sib < 256; sib++) {
        itext[prefix_length + 2] = sib;
        TryOneInstruction(itext, kInstByteCount);
      }
    }
  }
}

/* For x86-64, enhance the iteration by looping through REX prefixes.
 */
static void TestAllWithPrefixREX(unsigned int prefix, size_t prefix_length) {
#if NACL_TARGET_SUBARCH == 64
  unsigned char REXp;
  unsigned int rprefix;
  /* test with REX prefixes */
  for (REXp = 0x40; REXp < 0x50; REXp++) {
    rprefix = (prefix << 8 | REXp);
    printf("Testing with prefix %x\n", rprefix);
    TestAllWithPrefix(rprefix, prefix_length + 1);
  }
#endif
  /* test with no REX prefix */
  TestAllWithPrefix(prefix, prefix_length);
}

/* For all prefixes, call TestAllWithPrefix() to enumrate and test
 * all instructions.
 */
static void TestAllInstructions() {
  gSkipRepeatReports = TRUE;
  /* NOTE: Prefix byte order needs to be reversed when written as
   * an integer. For example, for integer prefix 0x3a0f, 0f will
   * go in instruction byte 0, and 3a in byte 1.
   */
  /* TODO(bradchen): extend enuminsts-64 to iterate over 64-bit prefixes. */
  TestAllWithPrefixREX(0, 0);
  TestAllWithPrefixREX(0x0f, 1);
  TestAllWithPrefixREX(0x0ff2, 2);
  TestAllWithPrefixREX(0x0ff3, 2);
  TestAllWithPrefixREX(0x0f66, 2);
  TestAllWithPrefixREX(0x0f0f, 2);
  TestAllWithPrefixREX(0x380f, 2);
  TestAllWithPrefixREX(0x3a0f, 2);
  TestAllWithPrefixREX(0x380f66, 3);
  TestAllWithPrefixREX(0x380ff2, 3);
  TestAllWithPrefixREX(0x3a0f66, 3);
}

/* Used to test one instruction at a time, for example, in regression
 * testing, or for instruction arguments from the command line.
 */
static void TestOneInstruction(char *asciihex) {
  InstByteArray ibytes;
  int nbytes;

  nbytes = Text2Bytes(ibytes, asciihex, "Command-line argument", -1);
  if (nbytes == 0) return;
  if (gVerbose) {
    int i;
    printf("trying %s (", asciihex);
    for (i = 0; i < nbytes; ++i) {
      printf("%02x", ibytes[i]);
    }
    printf(")\n");
  }
  TryOneInstruction(ibytes, (size_t) nbytes);
}

/* A set of test cases that have caused problems in the past.
 * This is a bit stale; most of the test cases came from xed_compare.py.
 * Mostly this program has been tested by TestAllInstructions(),
 * possible since this program is much faster than xed_compare.py
 */
static void RunRegressionTests() {
  TestOneInstruction("0024c2");
  TestOneInstruction("017967");
  TestOneInstruction("0f12c0");
  TestOneInstruction("0f13c0");
  TestOneInstruction("0f17c0");
  TestOneInstruction("0f01c1");
  TestOneInstruction("0f00300000112233445566778899aa");
  TestOneInstruction("cc");
  TestOneInstruction("C3");
  TestOneInstruction("0f00290000112233445566778899aa");
  TestOneInstruction("80e4f7");
  TestOneInstruction("e9a0ffffff");
  TestOneInstruction("4883ec08");
  TestOneInstruction("0f00040500112233445566778899aa");
  /* Below are newly discovered mistakes in call instructions, where the wrong
   * byte length was required by x86-64 nacl validator.
   */
  TestOneInstruction("262e7e00");
  TestOneInstruction("2e3e7900");
  /* From the AMD manual, "An instruction may have only one REX prefix */
  /* which must immediately precede the opcode or first excape byte    */
  /* in the instruction encoding." */
  TestOneInstruction("406601d8");  /* illegal; REX before data16 */
  TestOneInstruction("664001d8");  /* legal; REX after data16    */
  TestOneInstruction("414001d8");  /* illegal; two REX bytes     */

  /* Reset the opcode repeat test, so as not to silence erros */
  /* that happened in the regression suite. */
  (void)NotOpcodeRepeat("");
}

/* Define decoders that can be registered. */
extern NaClEnumeratorDecoder* RegisterNaClDecoder();
extern NaClEnumeratorDecoder* RegisterRagelDecoder();

/* Initialize the set of available decoders. */
static void NaClInitializeAvailableDecoders() {
  vProd = RegisterNaClDecoder();
  vDFA = RegisterRagelDecoder();
}

int main(int argc, char *argv[]) {
  int testargs;

  NaClLogModuleInit();
  NaClLogSetVerbosity(LOG_FATAL);
  NaClInitializeAvailableDecoders();

  gArgv0 = argv[0];
  testargs = 1;

  if (testargs == argc) {
    if (gPrefix == 0) RunRegressionTests();
    TestAllInstructions();
  } else {
    int i;
    gVerbose = TRUE;
    for (i = testargs; i < argc; ++i) {
      TestOneInstruction(argv[i]);
    }
  }
  PrintStats();
}
