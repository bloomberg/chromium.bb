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
#if NACL_WINDOWS
#define _CRT_RAND_S  /* enable decl of rand_s() */
#endif

#include "native_client/src/trusted/validator/x86/testing/enuminsts/enuminsts.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

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

/* Count of errors that have a high certainty of being exploitable. */
static int gSawLethalError = 0;

/* Defines the assumed text address for the test instruction */
const int kTextAddress = 0x1000;

/* If non-negative, defines the prefix to test. */
static unsigned int gPrefix = 0;

/* If non-negative, defines the opcode to test. */
static int gOpcode = -1;

/* This option triggers a set of behaviors that help produce repeatable
 * output, for easier diffs on the buildbots.
 */
static Bool gEasyDiffMode;

/* The production and new R-DFA validators */
NaClEnumeratorDecoder* vProd;
NaClEnumeratorDecoder* vDFA;

/* The name of the executable (i.e. argv[0] from the command line). */
static const char *gArgv0 = "argv0";
#define FLAG_EasyDiff "--easydiff"

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

/* Report a disagreement between decoders.
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

struct vdiff_stats {
  int64_t tried;
  int64_t valid;
  int64_t invalid;
  int64_t errors;
} gVDiffStats = {0, 0, 0, 0};

static void IncrTried(void) {
  gVDiffStats.tried += 1;
}

static void IncrValid(void) {
  gVDiffStats.valid += 1;
}

static void IncrInvalid(void) {
  gVDiffStats.invalid += 1;
}

static void IncrErrors(void) {
  gVDiffStats.errors += 1;
}

static void PrintStats(void) {
  printf("Stats:\n");
  if (!gEasyDiffMode) {
    printf("valid: %" NACL_PRIu64 "\n", gVDiffStats.valid);
    printf("invalid: %" NACL_PRIu64 "\n", gVDiffStats.invalid);
  }
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

    if (prod_okay && rdfa_okay) {
      if (vProd->_inst_length_fn(&pinst) ==
          vDFA->_inst_length_fn(&dinst)) {
        /* Both validators see a legal instruction, */
        /* and they agree on critical details.      */
        IncrValid();
      } else {
        DecoderError("LENGTH MISMATCH", &pinst, &dinst, "");
        IncrErrors();
      }
    } else if (prod_okay && !rdfa_okay) {
      /* Validators disagree on instruction legality */
      DecoderError("VALIDATORS DISAGREE (prod accepts, RDFA rejects)",
                   &pinst,
                   &dinst,
                   "");
      IncrErrors();
    } else if (!prod_okay && rdfa_okay) {
      /* Validators disagree on instruction legality */
      DecoderError("VALIDATORS DISAGREE (prod rejects, RDFA accepts)",
                   &pinst,
                   &dinst,
                   "");
      IncrErrors();
    } else {
      /* Both validators see an illegal instruction */
      IncrInvalid();
    }

    if (gVerbose) {
      PrintDisassembledInstructionVariants(&pinst, &dinst);
    }
  } while (0);
}

/* A function type for instruction "TestAll" functions.
 * Parameters:
 *     prefix: up to four bytes of prefix.
 *     prefix_length: size_t on [0..4] specifying length of prefix.
 *     print_prefix: For easy diff of test output, avoid printing
 *         the value of a randomly selected REX prefix.
 */
typedef void (*TestAllFunction)(const unsigned int prefix,
                                const size_t prefix_length,
                                const char* print_prefix);

/* Create a char* rendition of a prefix string, appending bytes
 * in ps. When using a randomly generated REX prefix on the bots,
 * it's useful to avoid printing the actual REX prefix so that
 * output can be diffed from run-to-run. For example, instead of
 * printing "0F45" you might print "0FXX". Parameters:
 *     prefix: The part of the prefix value to print
 *     ps: 'postscript', string to append to prefix value
 *     str: where to put the ASCII version of the prefix
 */
static char* StrPrefix(const unsigned int prefix, char* ps, char* str) {
  sprintf(str, "%x%s", prefix, (ps == NULL) ? "" : ps);
  return str;
}

/* Enumerate and test all 24-bit opcode+modrm+sib patterns for a
 * particular prefix.
 */
static void TestAllWithPrefix(const unsigned int prefix,
                              const size_t prefix_length,
                              const char* print_prefix) {
  const size_t kInstByteCount = NACL_ENUM_MAX_INSTRUCTION_BYTES;
  const size_t kIterByteCount = 3;
  InstByteArray itext;
  size_t i;
  int op, modrm, sib;
  int min_op;
  int max_op;

  if ((gPrefix > 0) && (gPrefix != prefix)) return;

  PrintProgress("TestAllWithPrefix(%s)\n", print_prefix);
  /* set up prefix */
  memcpy(itext, &prefix, prefix_length);
  /* set up filler bytes */
  for (i = prefix_length + kIterByteCount; i < kInstByteCount; i++) {
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
    if (!gEasyDiffMode) PrintProgress("%02x 00 00\n", op);
    for (modrm = 0; modrm < 256; modrm++) {
      itext[prefix_length + 1] = modrm;
      for (sib = 0; sib < 256; sib++) {
        itext[prefix_length + 2] = sib;
        TryOneInstruction(itext, kInstByteCount);
      }
    }
  }
}

/* For 3DNow!, the operand byte goes at the end. Format is:
 *   0F 0F [ModRM] [SIB] [displacement] imm8_opcode
 * See AMD doc 24594, page 435.
 */
static void TestAll3DNow(const unsigned int prefix,
                         const size_t prefix_length,
                         const char* print_prefix) {
  const size_t kInstByteCount = NACL_ENUM_MAX_INSTRUCTION_BYTES;
  const size_t kIterByteCount = 3;
  InstByteArray itext;
  size_t i;
  int op, modrm, sib;

  if ((gPrefix > 0) && (gPrefix != prefix)) return;

  PrintProgress("TestAll3DNow(%s)\n", print_prefix);
  /* set up prefix */
  memcpy(itext, &prefix, prefix_length);
  /* set up filler bytes */
  for (i = prefix_length + kIterByteCount; i < kInstByteCount; i++) {
    itext[i] = (uint8_t)i;
  }

  for (op = 0; op < 256; op++) {
    if (!gEasyDiffMode) PrintProgress("%02x 00 00\n", op);
    /* Use opcode as fill byte, forcing iteration through 3DNow opcodes. */
    for (i = prefix_length + 2; i < kIterByteCount; i++) itext[i] = op;

    for (modrm = 0; modrm < 256; modrm++) {
      itext[prefix_length] = modrm;
      for (sib = 0; sib < 256; sib++) {
        itext[prefix_length + 1] = sib;
        TryOneInstruction(itext, kInstByteCount);
      }
    }
  }
}

#if NACL_TARGET_SUBARCH == 64
/* REX prefixes range from 0x40 to 0x4f. */
const uint32_t kREXBase = 0x40;
const uint32_t kREXRange = 0x10;
const uint32_t kREXMax = 0x50;  /* kREXBase + kREXRange */

/* Generate a random REX prefix, to use for the entire run. */
static uint32_t RandomRexPrefix(void) {
  static uint32_t static_rex_prefix = 0;

  if (0 == static_rex_prefix) {
#if NACL_LINUX || NACL_OSX
    static_rex_prefix = kREXBase + (random() % kREXRange);
#elif NACL_WINDOWS
    if (rand_s(&static_rex_prefix) != 0) {
      ReportFatalError("rand_s() failed\n");
    } else {
      static_rex_prefix = kREXBase + (static_rex_prefix % kREXRange);
    }
#else
# error "Unknown operating system."
#endif
  }
  return static_rex_prefix;
}
#endif

#define AppendPrefixByte(oldprefix, pbyte) (((oldprefix) << 8) | (pbyte))
/* For x86-64, enhance the iteration by looping through REX prefixes.
 */
static void WithREX(TestAllFunction testall,
                    const unsigned int prefix,
                    const size_t prefix_length) {
  char pstr[kBufferSize];
#if NACL_TARGET_SUBARCH == 64
  unsigned char irex;
  unsigned int rprefix;
  /* test with REX prefixes */
  printf("WithREX(testall, %x, %d, %d)\n", prefix,
         (int)prefix_length, gEasyDiffMode);
  if (gEasyDiffMode) {
    printf("With random REX prefix.\n");
    irex = RandomRexPrefix();
    rprefix = AppendPrefixByte(prefix, irex);
    testall(rprefix, prefix_length + 1, StrPrefix(prefix, "XX", pstr));
  } else {
    for (irex = kREXBase; irex < kREXMax; irex++) {
      rprefix = AppendPrefixByte(prefix, irex);
      printf("With REX prefix %x\n", rprefix);
      testall(rprefix, prefix_length + 1, StrPrefix(rprefix, "", pstr));
    }
  }
#endif
  /* test with no REX prefix */
  testall(prefix, prefix_length, StrPrefix(prefix, NULL, pstr));
}
#undef AppendPrefixByte

/* For all prefixes, call TestAllWithPrefix() to enumrate and test
 * all instructions.
 */
static void TestAllInstructions(void) {
  /* NOTE: Prefix byte order needs to be reversed when written as
   * an integer. For example, for integer prefix 0x3a0f, 0f will
   * go in instruction byte 0, and 3a in byte 1.
   */
  WithREX(TestAllWithPrefix, 0, 0);
  WithREX(TestAllWithPrefix, 0x0f, 1);     /* two-byte opcode */
  WithREX(TestAllWithPrefix, 0x0ff2, 2);   /* SSE2  */
  WithREX(TestAllWithPrefix, 0x0ff3, 2);   /* SSE   */
  WithREX(TestAllWithPrefix, 0x0f66, 2);   /* SSE2  */
  WithREX(TestAllWithPrefix, 0x380f, 2);   /* SSSE3 */
  WithREX(TestAllWithPrefix, 0x3a0f, 2);   /* SSE4  */
  WithREX(TestAllWithPrefix, 0x380f66, 3); /* SSE4+ */
  WithREX(TestAllWithPrefix, 0x380ff2, 3); /* SSE4+ */
  WithREX(TestAllWithPrefix, 0x3a0f66, 3); /* SSE4+ */
  WithREX(TestAllWithPrefix, 0x0ff366, 3); /* SSE4+ */
  WithREX(TestAll3DNow, 0x0f0f, 2);
}

/* Used to test one instruction at a time, for example, in regression
 * testing, or for instruction arguments from the command line.
 */
static void TestOneInstruction(const char *asciihex) {
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
static void RunRegressionTests(void) {
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

  /* And some tests for degenerate prefix patterns */
  TestOneInstruction("666690");
  TestOneInstruction("6690");
  TestOneInstruction("666666666666666666666690");
  TestOneInstruction("66454490");
  TestOneInstruction("66454f90");
  TestOneInstruction("456690");
}

/* Define decoders that can be registered. */
extern NaClEnumeratorDecoder* RegisterNaClDecoder(void);
extern NaClEnumeratorDecoder* RegisterRagelDecoder(void);

/* Initialize the set of available decoders. */
static void VDiffInitializeAvailableDecoders(void) {
  vProd = RegisterNaClDecoder();
  vDFA = RegisterRagelDecoder();
}

static int ParseArgv(const int argc, const char* argv[]) {
  int nextarg;

  gArgv0 = argv[0];
  nextarg = 1;
  if (nextarg < argc &&
      0 == strcmp(argv[nextarg], FLAG_EasyDiff)) {
    gEasyDiffMode = TRUE;
    nextarg += 1;
  }
  return nextarg;
}

int main(const int argc, const char *argv[]) {
  int nextarg;

  NaClLogModuleInit();
  NaClLogSetVerbosity(LOG_FATAL);
#if NACL_LINUX || NACL_OSX
  srandom(time(NULL));
#endif
  VDiffInitializeAvailableDecoders();

  nextarg = ParseArgv(argc, argv);
  if (nextarg == argc) {
    if (gPrefix == 0) RunRegressionTests();
    TestAllInstructions();
  } else {
    int i;
    gVerbose = TRUE;
    for (i = nextarg; i < argc; ++i) {
      TestOneInstruction(argv[i]);
    }
  }
  PrintStats();

  /* exit with non-zero error code if there were errors. */
  exit(gVDiffStats.errors != 0);
}
