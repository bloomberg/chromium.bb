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
#include "native_client/src/trusted/validator/x86/testing/enuminsts/input_tester.h"
#include "native_client/src/trusted/validator/x86/testing/enuminsts/str_utils.h"
#include "native_client/src/trusted/validator/x86/testing/enuminsts/text2hex.h"

/* When non-zero, prints out additional debugging messages. */
#define kDebug 0

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

/* When true, check opcode mnemonic differences when processing each
 * instruction.
 */
static Bool gCheckMnemonics = TRUE;

/* When true, check for operand differences when processing each instruction. */
static Bool gCheckOperands = FALSE;

/* Count of errors that have a high certainty of being exploitable. */
static int gSawLethalError = 0;

/* Defines the assumed text address for the test instruction */
const int kTextAddress = 0x1000;

/* If non-negative, defines the prefix to test. */
static unsigned int gPrefix = 0;

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

/* Defines maximum number of available decoders (See comments on
 * struct NaClEnumeratorDecoder in enuminst.h for details on what
 * a decoder is.
 */
#define NACL_MAX_AVAILABLE_DECODERS 10

/* Holds the set of available decoders. */
NaClEnumeratorDecoder* kAvailableDecoders[NACL_MAX_AVAILABLE_DECODERS];

/* Holds the number of (pre)registered available decoders. */
size_t kNumAvailableDecoders;

/* This struct holds a list of instruction opcode sequences that we
 * want to treat specially. Used to filter out problem cases from
 * the enumeration.
 */
typedef struct {
  /* Pointer to array of bytes for the instruction. */
  uint8_t* bytes_;
  /* The size of bytes_. */
  size_t bytes_size_;
  /* Pointer to array of instructions. Each element is
   * the index into bytes_ where the corresponding byte sequence
   * of the instruction begins. The next element in the array is
   * the end point for the current instruction.
   */
  size_t* insts_;
  /* The size of insts_. */
  size_t insts_size_;
  /* Number of instructions stored in insts_. */
  size_t num_insts_;
  /* Number of bytes stored in bytes_. */
  size_t num_bytes_;
} InstList;

/* This struct holds state concerning an instruction, both from the
 * various available decoders. Some of the state information is
 * redundant, preserved to avoid having to recompute it.
 */
typedef struct {
  /* Holds the set of decoders to enumerate over. */
  NaClEnumerator _enumerator;
  /* True if a decoder or comparison failed due to an error with
   * the way the instruction was decoded.
   */
  Bool _decoder_error;
} ComparedInstruction;

/* The state to use to compare instructions. */
static ComparedInstruction gCinst;

/* The name of the executable (i.e. argv[0] from the command line). */
static char *gArgv0 = "argv0";

static INLINE const char* BoolName(Bool b) {
  return b ? "true" : "false";
}

/* Print out the bindings to command line arguments. */
static void PrintBindings(ComparedInstruction* cinst) {
  size_t i;
  for (i = 0; i < cinst->_enumerator._num_decoders; ++i) {
    fprintf(stderr, "--%s\n", cinst->_enumerator._decoder[i]->_id_name);
  }
  fprintf(stderr, "--checkmnemonics=%s\n", BoolName(gCheckMnemonics));
  fprintf(stderr, "--checkoperands=%s\n", BoolName(gCheckOperands));
  for (i = 0; i < cinst->_enumerator._num_decoders; ++i) {
      fprintf(stderr, "--%s=%s\n",
              (cinst->_enumerator._decoder[i]->_legal_only
               ? "legal" : "illegal"),
              cinst->_enumerator._decoder[i]->_id_name);
  }
  fprintf(stderr, "--nacllegal=%s\n", BoolName(gNaClLegal));
#ifdef NACL_REVISION
  fprintf(stderr, "--nacl_revision=%d\n", NACL_REVISION);
#endif
  if (gOpcode >= 0) fprintf(stderr, "--opcode=0x%02x\n", gOpcode);
  for (i = 0; i < cinst->_enumerator._num_decoders; ++i) {
    if (cinst->_enumerator._decoder[i]->_print_opcode_sequence) {
      fprintf(stderr, "--opcode_bytes=%s\n",
              cinst->_enumerator._decoder[i]->_id_name);
    }
  }
  for (i = 0; i < cinst->_enumerator._num_decoders; ++i) {
    if (cinst->_enumerator._decoder[i]->_print_opcode_sequence_plus_desc) {
      fprintf(stderr, "--opcode_bytes_plus_desc=%s\n",
              cinst->_enumerator._decoder[i]->_id_name);
    }
  }
  for (i = 0; i < cinst->_enumerator._num_decoders; ++i) {
    if (cinst->_enumerator._decoder[i]->_print) {
      fprintf(stderr, "--print=%s\n", cinst->_enumerator._decoder[i]->_id_name);
    }
  }
#ifdef NACL_XED_DECODER
  fprintf(stderr, "--pin_version=\"%s\"\n", NACL_PINV);
#endif
  if (gPrefix > 0) fprintf(stderr, "--prefix=0x%08x\n", gPrefix);
  fprintf(stderr, "--nops=%s\n", BoolName(gNoCompareNops));
  fprintf(stderr, "--skipcontiguous=%s\n", BoolName(gSkipContiguous));
  fprintf(stderr, "--verbose=%s\n", BoolName(gVerbose));
  fprintf(stderr, "--xedimplemented=%s\n", BoolName(gXedImplemented));
  exit(1);
}

/* Prints out summary of how to use this executable. and then exits. */
static void Usage(void) {
  size_t i;
  fprintf(stderr, "usage: %s [decoders] [options] [hexbytes ...]\n",
          gArgv0);
  fprintf(stderr, "\n");
  fprintf(stderr, "    Compare %s instruction decoders\n",
          target_machine);
  fprintf(stderr, "\n");
  fprintf(stderr, "    With no arguments, enumerate all %s instructions.\n",
          target_machine);
  fprintf(stderr, "    With arguments, decode each sequence of "
          "opcode bytes.\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Available decoders are (select using --name):\n");
  fprintf(stderr, "\n");
  for (i = 0; i < kNumAvailableDecoders; ++i) {
    fprintf(stderr, "    %s: %s\n",
            kAvailableDecoders[i]->_id_name,
            kAvailableDecoders[i]->_usage_message);
  }
  fprintf(stderr, "\n");
  fprintf(stderr, "One or more filters are required:\n");
  fprintf(stderr, "    --illegal=XX: Filter instructions to only consider "
          "those instructions\n");
  fprintf(stderr, "         that are illegal instructions, as defined by "
          "decoder XX\n");
  fprintf(stderr, "    --legal=XX: Filter instructions to only consider "
          "those instructions\n");
  fprintf(stderr, "         that are legal instructions, as defined by "
          "decoder XX\n");
  fprintf(stderr, "    --print=XX: Prints out set of enumerated "
          "instructions,\n");
  fprintf(stderr, "                for the specified decoder XX (may be "
          "repeated).\n");
  fprintf(stderr, "                Also registers decoder XX if needed.\n");

  fprintf(stderr, "\nAdditional options:\n");
  fprintf(stderr, "    --bindings: Prints out current (command-line) "
          "bindings\n");
  fprintf(stderr, "    --checkmnemonics: enables opcode mnemonic "
          "comparisons\n");
  fprintf(stderr, "    --checkoperands: enables operand comparison (slow)\n");
  fprintf(stderr, "    --ignore_mnemonic=file: ignore mnemonic name "
          "comparison\n");
  fprintf(stderr, "         for instruction sequences in file (may be "
          "repeated)\n");
  fprintf(stderr, "    --ignored=<file>: ignore instruction sequences "
          "in file (may be repeated)\n");
  fprintf(stderr, "    --nacllegal: use validator checks/instruction type)\n");
  fprintf(stderr, "        in addition to decoder errors with nacl\n");
  fprintf(stderr, "        instruction filters.\n");
#ifdef NACL_REVISION
  fprintf(stderr, "    --nacl_revision: print nacl revision used to build\n"
          "        the nacl decoder\n");
#endif
  fprintf(stderr, "    --opcode=XX: only process given opcode XX for "
          "each prefix\n");
  fprintf(stderr, "    --opcode_bytes=XX: Prints out opcode bytes for set of\n"
          "        enumerated instructions. To be used by decoder --in\n");
  fprintf(stderr,
          "    --opcode_bytes_plus_desc=XX: Prints out opcode bytes for set\n"
          "        of enumerated instruction, plus a print description (as a\n"
          "        comment). To be used by decoder --n\n");
#ifdef NACL_XED_DECODER
  fprintf(stderr, "    --pin_version: Prints out pin version used for xed "
          "decoder\n");
#endif
  fprintf(stderr, "    --prefix=XX: only process given prefix XX\n");
  fprintf(stderr, "    --nops: Don't operand compare nops.\n");
  fprintf(stderr, "    --skipcontiguous: Only skip contiguous errors\n");
  fprintf(stderr, "    --verbose: add verbose comments to output\n");
  fprintf(stderr, "    --xedimplemented: only compare NaCl instruction that "
          "are also implemented in xed\n");
  exit(gSawLethalError);
}

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

/* Returns true if the given opcode sequence text is in the
 * given instruction list.
 */
static Bool InInstructionList(InstList* list, uint8_t* itext, size_t nbytes) {
  size_t i;
  size_t j;
  if (NULL == list) return FALSE;
  for (i = 0; i < list->num_insts_; ++i) {
    Bool found_match = TRUE;
    size_t start = list->insts_[i];
    size_t end = list->insts_[i + 1];
    size_t inst_bytes = (end - start);
    if (inst_bytes < nbytes) continue;
    for (j = 0; j < inst_bytes; j++) {
      if (itext[j] != list->bytes_[start + j]) {
        found_match = FALSE;
        break;
      }
    }
    if (found_match) {
      return TRUE;
    }
  }
  return FALSE;
}

/* Takes the given text and sends it to each of the instruction
 * decoders to decode the first instruction in the given text.
 * Commonly the decoded instruction will be shorter than nbytes.
 */
static void ParseFirstInstruction(ComparedInstruction *cinst,
                                  uint8_t *itext, size_t nbytes) {
  size_t i;
  memcpy(cinst->_enumerator._itext, itext, nbytes);
  cinst->_enumerator._num_bytes = nbytes;
  cinst->_decoder_error = FALSE;
  for (i = 0; i < cinst->_enumerator._num_decoders; ++i) {
    cinst->_enumerator._decoder[i]->
        _parse_inst_fn(&cinst->_enumerator, kTextAddress);
  }
}

/* Prints out the instruction each decoder disassembled */
static void PrintDisassembledInstructionVariants(ComparedInstruction* cinst) {
  size_t i;
  for (i = 0; i < cinst->_enumerator._num_decoders; ++i) {
    cinst->_enumerator._decoder[i]->_print_inst_fn(&cinst->_enumerator);
  }
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

/* Initial value for last bad opcode. */
#define NOT_AN_OPCODE "not an opcode"

/* Holds the name of the last (bad) mnemonic name matched.
 * Uses the mnemonic name associated with instructions decoded
 * by the first decoder.
 */
static char last_bad_mnemonic[kBufferSize] = NOT_AN_OPCODE;

/* Returns how many decoder errors were skipped for an opcode. */
static int nSkipped = 0;

/* Changes the last bad mnemonic name to the new value. */
static void ChangeLastBadMnemonic(const char* new_value) {
  cstrncpy(last_bad_mnemonic, new_value, kBufferSize);
}

/* Reset the counters for skipping decoder errors, assuming
 * the last_bad_opcode (if non-null) was the given value.
 */
static void ResetSkipCounts(const char* last_opcode) {
  nSkipped = 0;
  ChangeLastBadMnemonic((last_opcode == NULL) ? "not an opcode" : last_opcode);
}

/* Report number of instructions with errors that were skipped, and
 * then reset the skip counts.
 */
static void ReportOnSkippedErrors(ComparedInstruction* cinst,
                                  const char* mnemonic) {
  UNREFERENCED_PARAMETER(cinst);
  if (nSkipped > 0 && gSilent) {
    printf("...skipped %d errors for %s\n", nSkipped, last_bad_mnemonic);
  }
  ResetSkipCounts(mnemonic);
}

/* Report a disagreement between decoders. To reduce
 * noice from uninteresting related errors, gSkipRepeatReports will
 * avoid printing consecutive errors for the same opcode.
 */
static void DecoderError(const char *why,
                         ComparedInstruction *cinst,
                         const char *details) {
  size_t i;
  cinst->_decoder_error = TRUE;

  /* Don't print errors when running silently. In such cases we
   * are generating valid opcode sequences for a decoder, and problems
   * should be ignored.
   */
  if (gSilent) return;

  /* Check if we have already reported for instruction mnemonic,
   * based on the mnemonic used by the first decoder.
   */
  if (gSkipRepeatReports) {
    /* Look for first possible name for instruction. */
    for (i = 0; i < cinst->_enumerator._num_decoders; ++i) {
      NaClEnumeratorDecoder *decoder = cinst->_enumerator._decoder[i];
      const char* mnemonic;
      if (NULL == decoder->_get_inst_mnemonic_fn) continue;
      mnemonic = cinst->_enumerator._decoder[0]->
          _get_inst_mnemonic_fn(&cinst->_enumerator);
      if (strcmp(mnemonic, last_bad_mnemonic) == 0) {
        nSkipped += 1;
        return;
      }
    }
  }

  /* If reached, did not skip, so report the error. */
  printf("**** ERROR: %s: %s\n", why, (details == NULL ? "" : details));
  PrintDisassembledInstructionVariants(cinst);
}

#if NACL_TARGET_SUBARCH == 64
/* The instructions:
 *    48 89 e5  mov  rbp, rsp
 *    4a 89 e5  mov  rbp, rsp
 * look bad based on the simple rule, but are safe because they are
 * moving a safe address between two protected registers.
 */
static int IsSpecialSafeRegWrite(ComparedInstruction *cinst) {
  uint8_t byte0 = cinst->_enumerator._itext[0];
  uint8_t byte1 = cinst->_enumerator._itext[1];
  uint8_t byte2 = cinst->_enumerator._itext[2];

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
static Bool BadRegWrite(ComparedInstruction *cinst) {
  size_t i;
  NaClEnumeratorDecoder* xed_decoder = NULL;
  size_t noperands;
  size_t ilength;

  /* First see if there is a xed decoder to compare against. */
  for (i = 0; i < cinst->_enumerator._num_decoders; ++i) {
    if (0 == strcmp("xed", cinst->_enumerator._decoder[i]->_id_name)) {
      xed_decoder = cinst->_enumerator._decoder[i];
      break;
    }
  }

  /* Quit if we don't have the right support for this function. */
  if ((NULL == xed_decoder) ||
      !xed_decoder->_is_inst_legal_fn(&cinst->_enumerator) ||
      (NULL == xed_decoder->_get_inst_num_operands_fn) ||
      (NULL == xed_decoder->_writes_to_reserved_reg_fn)) return 0;

  /* If reached, we found the xed decoder. */
  noperands = xed_decoder->_get_inst_num_operands_fn(&cinst->_enumerator);
  ilength = xed_decoder->_inst_length_fn(&cinst->_enumerator);
  for (i = 0; i < cinst->_enumerator._num_decoders; ++i) {
    size_t j;
    NaClEnumeratorDecoder* other_decoder = cinst->_enumerator._decoder[i];

    /* don't compare against self. */
    if (xed_decoder == other_decoder) break;

    /* don't compare if the other decoder doesn't know how to validate. */
    if (NULL == other_decoder->_segment_validates_fn) break;
    if (!other_decoder->_is_inst_legal_fn(&cinst->_enumerator)) break;

    for (j = 0; j < noperands ; j++) {
      if (xed_decoder->_writes_to_reserved_reg_fn(&cinst->_enumerator, j)) {
        if (other_decoder->_segment_validates_fn(&cinst->_enumerator,
                                                cinst->_enumerator._itext,
                                                ilength,
                                                kTextAddress)) {
          char sbuf[kBufferSize];
          /* Report problem if other decoder accepted instruction, but is
           * not one of the special safe writes.
           */
          if (!IsSpecialSafeRegWrite(cinst)) continue;
          gSawLethalError = 1;
          SNPRINTF(sbuf, kBufferSize, "(%s) xed operand %d\n",
                   other_decoder->_id_name, (int) j);
          DecoderError("ILLEGAL REGISTER WRITE", cinst, sbuf);
          return TRUE;
        }
      }
    }
  }
  return FALSE;
}
#endif

/* Compare operands of each decoder's disassembled instruction, reporting
 * an error if decoders disagree.
 */
static Bool AreInstOperandsEqual(ComparedInstruction *cinst) {
  size_t i;
  size_t num_decoders = 0;
  const char* operands[NACL_MAX_ENUM_DECODERS];
  NaClEnumeratorDecoder* operands_decoder[NACL_MAX_ENUM_DECODERS];
  NaClEnumerator* enumerator = &cinst->_enumerator;

  /* If no decoders, vacuously true. */
  if (0 == enumerator->_num_decoders) return FALSE;

  /* Collect operand lists and corresponding decoders. */
  for (i = 0; i < enumerator->_num_decoders; ++i) {
    /* Start by verifying that we can find operands. */
    NaClEnumeratorDecoder *decoder = enumerator->_decoder[i];
    if (NULL == decoder->_get_inst_operands_text_fn) continue;
    if (!decoder->_is_inst_legal_fn(enumerator)) continue;

    /* HACK ALERT! Special case "nops" by removing operands. We do this
     * assuming it doesn't matter what the argumnents are, the effect is
     * the same.
     */
    if (gNoCompareNops &&
        (NULL != decoder->_get_inst_mnemonic_fn) &&
        (0 == strcmp("nop", decoder->_get_inst_mnemonic_fn(enumerator)))) {
      operands[num_decoders] = "";
      operands_decoder[num_decoders++] = decoder;
      continue;
    }

    /* Not special case, record operand and decoder. */
    operands[num_decoders] = decoder->_get_inst_operands_text_fn(enumerator);
    if (NULL == operands[num_decoders]) operands[num_decoders] = "";
    operands_decoder[num_decoders++] = decoder;
  }

  /* Now test if operands compare between decoders. */
  for (i = 1; i < num_decoders; ++i) {
    if (0 != strncmp(operands[i-1], operands[i], kBufferSize)) {
      char sbuf[kBufferSize];
      SNPRINTF(sbuf, kBufferSize, "(%s) '%s' != (%s)'%s'",
               operands_decoder[i-1]->_id_name, operands[i-1],
               operands_decoder[i]->_id_name, operands[i]);
      DecoderError("OPERAND MISMATCH", cinst, sbuf);
      return FALSE;
    }
  }

  if (kDebug && (num_decoders > 0)) {
    printf("operands match: %s\n", operands[0]);
  }
  return TRUE;
}

/* If non-null, the list of instructions for which mnemonics should
 * not be compared.
 */
static InstList* kIgnoreMnemonics = NULL;

/* Compares mnemonic names between decoder's disassembled instructions,
 * returning true if they agree on the mnemonic name.
 */
static Bool AreInstMnemonicsEqual(ComparedInstruction *cinst) {
  size_t i;
  size_t num_decoders = 0;
  const char* name[NACL_MAX_ENUM_DECODERS];
  NaClEnumeratorDecoder* name_decoder[NACL_MAX_ENUM_DECODERS];

  /* If no decoders, vacuously true. */
  if (0 == cinst->_enumerator._num_decoders) return 0;

  /* Collect mnemonics of corresponding decoders (if defined). */
  for (i = 0; i < cinst->_enumerator._num_decoders; ++i) {
    /* Start by verifying that we can get name name. */
    NaClEnumeratorDecoder *decoder = cinst->_enumerator._decoder[i];
    if (NULL == decoder->_get_inst_mnemonic_fn) continue;
    if (!decoder->_is_inst_legal_fn(&cinst->_enumerator)) continue;

    /* If on ignore list, ignore. */
    if (InInstructionList(kIgnoreMnemonics,
                          cinst->_enumerator._itext,
                          decoder->_inst_length_fn(&cinst->_enumerator)))
      continue;

    /* Record mnemonic name and decoder for comparisons below. */
    name[num_decoders] = decoder->_get_inst_mnemonic_fn(&cinst->_enumerator);
    name_decoder[num_decoders++] = decoder;
  }

  /* Now compare mnemonics that were defined. */
  for (i = 1; i < num_decoders; ++i) {
    if (strncmp(name[i-1], name[i], kBufferSize) != 0) {
      char sbuf[kBufferSize];
      SNPRINTF(sbuf, kBufferSize, "(%s) %s != (%s) %s",
               name_decoder[i-1]->_id_name, name[i-1],
               name_decoder[i]->_id_name, name[i]);
      DecoderError("MNEMONIC MISMATCH", cinst, sbuf);
      return FALSE;
    }
  }

  if (kDebug && (num_decoders > 0)) {
    printf("names match: %s\n", name[0]);
  }
  return TRUE;
}

/* Returns true if the decoder decodes the instruction correctly,
 * and also (to the best it can determine) validates when
 * specified to do so on the command line.
 */
static Bool ConsiderInstLegal(NaClEnumerator* enumerator,
                              NaClEnumeratorDecoder* decoder) {
  if (!decoder->_is_inst_legal_fn(enumerator)) return FALSE;
  if (!gNaClLegal) return TRUE;
  if (NULL == decoder->_maybe_inst_validates_fn) return TRUE;
  return decoder->_maybe_inst_validates_fn(enumerator);
}

/* Returns true only if the legal filters allow the instruction to
 * be processed.
 */
static Bool RemovedByInstLegalFilters(ComparedInstruction* cinst) {
  size_t i;
  for (i = 0; i < cinst->_enumerator._num_decoders; ++i) {
    if (ConsiderInstLegal(&cinst->_enumerator,
                          cinst->_enumerator._decoder[i])
        != cinst->_enumerator._decoder[i]->_legal_only) {
      return TRUE;
    }
  }
  return FALSE;
}

/* Returns true if the instruction has the same length for all decoders.
 * Reports length differences if found.
 */
static Bool AreInstructionLengthsEqual(ComparedInstruction *cinst) {
  size_t i;
  size_t num_decoders = 0;
  size_t length[NACL_MAX_ENUM_DECODERS];
  NaClEnumeratorDecoder* length_decoder[NACL_MAX_ENUM_DECODERS];

  /* If no decoders, vacuously true. */
  if (0 == cinst->_enumerator._num_decoders) return TRUE;

  /* Collect the instruction length for each decoder. */
  for (i = 0; i < cinst->_enumerator._num_decoders; ++i) {
    if (cinst->_enumerator._decoder[i]->
        _is_inst_legal_fn(&cinst->_enumerator)) {
      length[num_decoders] = cinst->_enumerator._decoder[i]->
          _inst_length_fn(&cinst->_enumerator);
      length_decoder[num_decoders] = cinst->_enumerator._decoder[i];
      ++num_decoders;
    }
  }

  /* Print out where lengths differ, if they differ. */
  for (i = 1; i < num_decoders; ++i) {
    if (length[i-1] != length[i]) {
      char sbuf[kBufferSize];
      SNPRINTF(sbuf, kBufferSize, "(%s) %"NACL_PRIuS" != (%s) %"NACL_PRIuS,
               length_decoder[i-1]->_id_name, length[i-1],
               length_decoder[i]->_id_name, length[i]);
      DecoderError("LENGTH MISMATCH", cinst, sbuf);
      gSawLethalError = 1;
      return FALSE;
    }
  }

  if (kDebug && (num_decoders > 0)) {
    printf("length match: %"NACL_PRIuS"\n", length[0]);
  }
  return TRUE;
}

/* Print out decodings if specified on the command line. Returns true
 * instruction(s) are printed. This function is used
 */
static Bool PrintInst(ComparedInstruction *cinst) {
  Bool result = FALSE;
  size_t i;
  for (i = 0; i < cinst->_enumerator._num_decoders; ++i) {
    NaClEnumeratorDecoder* decoder = cinst->_enumerator._decoder[i];
    if (decoder->_print) {
      cinst->_enumerator._decoder[i]->_print_inst_fn(&cinst->_enumerator);
      result = TRUE;
    }
    if (decoder->_print_opcode_sequence ||
        decoder->_print_opcode_sequence_plus_desc) {
      size_t length = decoder->_inst_length_fn(&cinst->_enumerator);
      for (i = 0; i < length; ++i) {
        printf("%02x", cinst->_enumerator._itext[i]);
      }
      if (decoder->_print_opcode_sequence_plus_desc &&
          decoder->_is_inst_legal_fn(&cinst->_enumerator) &&
          (NULL != decoder->_get_inst_mnemonic_fn) &&
          (NULL != decoder->_get_inst_operands_text_fn)) {
        printf("#%s %s", decoder->_get_inst_mnemonic_fn(&cinst->_enumerator),
               decoder->_get_inst_operands_text_fn(&cinst->_enumerator));
      }
      printf("\n");
      result = TRUE;
    }
  }
  return result;
}

/* If non-null, the list of instruction bytes to ignore. */
static InstList* kIgnoredInstructions = NULL;

/* Test comparison for a single instruction.
 */
static void TryOneInstruction(ComparedInstruction *cinst,
                              uint8_t *itext, size_t nbytes) {
  do {
    if (gVerbose) {
      size_t i;
      printf("================");
      for (i = 0; i < nbytes; ++i) {
        printf("%02x", itext[i]);
      }
      printf("\n");
    }

    /* Try to parse the sequence of test bytes. */
    ParseFirstInstruction(cinst, itext, nbytes);

    /* Don't bother to compare ignored instructions. */
    if (InInstructionList(kIgnoredInstructions, itext, nbytes)) break;

    /* Apply filters */
    if (RemovedByInstLegalFilters(cinst)) break;

    /* Apply comparison checks to the decoded instructions. */
    if (!AreInstructionLengthsEqual(cinst)) break;
    if (gCheckMnemonics && !AreInstMnemonicsEqual(cinst)) break;
    if (gCheckOperands && !AreInstOperandsEqual(cinst)) break;
#if NACL_TARGET_SUBARCH == 64
    if (BadRegWrite(cinst)) break;
#endif

    /* Print the instruction if print specified. */
    if (PrintInst(cinst)) break;
    /* no error */
    if (gVerbose) {
      PrintDisassembledInstructionVariants(cinst);
    }
  } while (0);

  /* saw error; should have already printed stuff. Only
   * report on skipped errors if we found an error-free
   * instruction.
   */
  if (gSkipContiguous && (!cinst->_decoder_error)) {
    ReportOnSkippedErrors(cinst, NULL);
  }
}

/* Returns true if for all decoders recognize legal instructions, they use
 * less than len bytes.
 */
static Bool IsLegalInstShorterThan(ComparedInstruction *cinst, size_t len) {
  size_t i;
  Bool found_legal = FALSE;
  for (i = 0; i < cinst->_enumerator._num_decoders; ++i) {
    if (cinst->_enumerator._decoder[i]->
        _is_inst_legal_fn(&cinst->_enumerator)) {
      found_legal = TRUE;
      if (cinst->_enumerator._decoder[i]->_inst_length_fn(&cinst->_enumerator)
          >= len) return FALSE;
    }
  }
  return found_legal;
}

/* Enumerate and test all 24-bit opcode+modrm+sib patterns for a
 * particular prefix.
 */
static void TestAllWithPrefix(ComparedInstruction *cinst,
                              unsigned int prefix, size_t prefix_length) {
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
    ResetSkipCounts(NULL);
    PrintProgress("%02x 00 00\n", op);
    for (modrm = 0; modrm < 256; modrm++) {
      itext[prefix_length + 1] = modrm;
      for (sib = 0; sib < 256; sib++) {
        itext[prefix_length + 2] = sib;
        TryOneInstruction(cinst, itext, kInstByteCount);
        /* If all decoders decode without using the sib byte, don't
         * bother to try more variants.
         */
        if (IsLegalInstShorterThan(cinst, prefix_length + 3)) break;
      }
      /* If all decoders decode without using the modrm byte, don't
       * bother to try more variants.
       */
      if (IsLegalInstShorterThan(cinst, prefix_length + 2)) break;
    }
    /* Force flushing of skipped errors, since we are now moving on
     * to the next opcode.
     */
    ReportOnSkippedErrors(cinst, NULL);
  }
}

/* For x86-64, enhance the iteration by looping through REX prefixes.
 */
static void TestAllWithPrefixREX(ComparedInstruction *cinst,
                                 unsigned int prefix, size_t prefix_length) {
#if NACL_TARGET_SUBARCH == 64
  unsigned char REXp;
  unsigned int rprefix;
  /* test with REX prefixes */
  for (REXp = 0x40; REXp < 0x50; REXp++) {
    rprefix = (prefix << 8 | REXp);
    printf("Testing with prefix %x\n", rprefix);
    TestAllWithPrefix(cinst, rprefix, prefix_length + 1);
  }
#endif
  /* test with no REX prefix */
  TestAllWithPrefix(cinst, prefix, prefix_length);
}

/* For all prefixes, call TestAllWithPrefix() to enumrate and test
 * all instructions.
 */
static void TestAllInstructions(ComparedInstruction *cinst) {
  gSkipRepeatReports = TRUE;
  /* NOTE: Prefix byte order needs to be reversed when written as
   * an integer. For example, for integer prefix 0x3a0f, 0f will
   * go in instruction byte 0, and 3a in byte 1.
   */
  /* TODO(bradchen): extend enuminsts-64 to iterate over 64-bit prefixes. */
  TestAllWithPrefixREX(cinst, 0, 0);
  TestAllWithPrefixREX(cinst, 0x0f, 1);
  TestAllWithPrefixREX(cinst, 0x0ff2, 2);
  TestAllWithPrefixREX(cinst, 0x0ff3, 2);
  TestAllWithPrefixREX(cinst, 0x0f66, 2);
  TestAllWithPrefixREX(cinst, 0x0f0f, 2);
  TestAllWithPrefixREX(cinst, 0x380f, 2);
  TestAllWithPrefixREX(cinst, 0x3a0f, 2);
  TestAllWithPrefixREX(cinst, 0x380f66, 3);
  TestAllWithPrefixREX(cinst, 0x380ff2, 3);
  TestAllWithPrefixREX(cinst, 0x3a0f66, 3);
}

/* Enumerate and test each instruction on stdin. */
static void TestInputInstructions(ComparedInstruction *cinst) {
  int i;
  InstByteArray itext;
  int num_bytes = 0;
  while (TRUE) {
    num_bytes = ReadAnInstruction(itext);
    if (num_bytes == 0) return;
    for (i = num_bytes; i < NACL_ENUM_MAX_INSTRUCTION_BYTES; ++i) {
      itext[i] = (uint8_t) i;
    }
    TryOneInstruction(cinst, itext, NACL_ENUM_MAX_INSTRUCTION_BYTES);
  }
}

/* Used to test one instruction at a time, for example, in regression
 * testing, or for instruction arguments from the command line.
 */
static void TestOneInstruction(ComparedInstruction *cinst, char *asciihex) {
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
  TryOneInstruction(cinst, ibytes, (size_t) nbytes);
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
  /* From the AMD manual, "An instruction may have only one REX prefix */
  /* which must immediately precede the opcode or first excape byte    */
  /* in the instruction encoding." */
  TestOneInstruction(cinst, "406601d8");  /* illegal; REX before data16 */
  TestOneInstruction(cinst, "664001d8");  /* legal; REX after data16    */
  TestOneInstruction(cinst, "414001d8");  /* illegal; two REX bytes     */
}

/* Returns the decoder with the given name, if it is registered. Otherwise,
 * returns NULL.
 */
static NaClEnumeratorDecoder*
NaClGetRegisteredDecoder(ComparedInstruction* cinst,
                         const char* decoder_name) {
  size_t i;
  for (i = 0; i < cinst->_enumerator._num_decoders; ++i) {
    if (0 == strcmp(cinst->_enumerator._decoder[i]->_id_name, decoder_name)) {
      return cinst->_enumerator._decoder[i];
    }
  }
  return NULL;
}

/* Register the decoder with the given name, returning the corresponding
 * decoder.
 */
static NaClEnumeratorDecoder*
NaClRegisterEnumeratorDecoder(ComparedInstruction* cinst,
                             const char* decoder_name) {
  size_t i;
  /* First check if already registered. If so, simply return it. */
  NaClEnumeratorDecoder* decoder =
      NaClGetRegisteredDecoder(cinst, decoder_name);
  if (NULL != decoder) return decoder;

  /* If reached, not registered yet. See if one with the given name
   * is available (i.e. preregistered).
   */
  for (i = 0; i < kNumAvailableDecoders; ++i) {
    decoder = kAvailableDecoders[i];
    if (0 == strcmp(decoder->_id_name, decoder_name)) {
      if (cinst->_enumerator._num_decoders < NACL_MAX_ENUM_DECODERS) {
        cinst->_enumerator._decoder[cinst->_enumerator._num_decoders++] =
            decoder;
        return decoder;
      }
    }
  }

  /* If reached, can't find a decoder with the given name, abort. */
  fprintf(stderr, "Can't find decoder '%s', aborting!\n", decoder_name);
  exit(1);
}

/* Install legal filter values for corresponding available decoders. */
static void NaClInstallLegalFilter(ComparedInstruction* cinst,
                                   const char* decoder_name,
                                   Bool new_value) {
  NaClRegisterEnumeratorDecoder(cinst, decoder_name)->_legal_only = new_value;
}

/* The initial size for bytes_ when creating an instruction list.
 */
static const size_t kInitialInstBytesSize = 1024;

/* The initial size for insts_ when creating an instruction list.
 */
static const size_t kInitialInstListInstsSize = 256;

/* Creates an initially empty list of instructions. */
static InstList* CreateEmptyInstList(void) {
  InstList* list = (InstList*) malloc(sizeof(InstList));
  if (NULL == list) ReportFatalError("Out of memory");
  list->bytes_ = (uint8_t*) malloc(kInitialInstBytesSize);
  list->bytes_size_ = kInitialInstBytesSize;
  list->insts_ = (size_t*) malloc(kInitialInstListInstsSize);
  list->insts_size_ = kInitialInstListInstsSize;
  list->insts_[0] = 0;
  list->num_insts_ = 0;
  list->num_bytes_ = 0;
  return list;
}
/* Expands the bytes_ field of the instruction list so that
 * more instructions can be added.
 */
static void ExpandInstListBytes(InstList* list) {
  size_t i;
  uint8_t* new_buffer;
  size_t new_size = list->bytes_size_ *2;
  if (new_size < list->bytes_size_) {
    ReportFatalError("Instruction list file too big");
  }
  new_buffer = (uint8_t*) malloc(new_size);
  if (NULL == new_buffer) ReportFatalError("Out of memory");
  for (i = 0; i < list->num_bytes_; ++i) {
    new_buffer[i] = list->bytes_[i];
  }
  free(list->bytes_);
  list->bytes_ = new_buffer;
  list->bytes_size_ = new_size;
}

/* Expands the insts_ field of the instruction list so that
 * more instructions can be added.
 */
static void ExpandInstListInsts(InstList* list) {
  size_t i;
  size_t* new_buffer;
  size_t new_size = list->insts_size_ * 2;

  if (new_size < list->insts_size_)
    ReportFatalError("Instruction list file too big");
  new_buffer = (size_t*) malloc(new_size);
  if (NULL == new_buffer) ReportFatalError("Out of memory");
  for (i = 0; i < list->num_insts_; ++i) {
    new_buffer[i] = list->insts_[i];
  }
  free(list->insts_);
  list->insts_ = new_buffer;
  list->insts_size_ = new_size;
}

/* Reads the bytes defined in line, and coverts it to the corresponding
 * ignored instruction. Then adds it to the list of ignored instructions.
 */
static void ReadInstListInst(InstList* list,
                             char line[kBufferSize],
                             const char* context,
                             int line_number) {
  int i;
  InstByteArray ibytes;
  int num_bytes = Text2Bytes(ibytes, line, context, line_number);

  /* Ignore line if no opcode sequence. */
  if (num_bytes == 0) return;

  /* First update the instruction pointers. */
  if (list->num_insts_ == list->insts_size_) {
    ExpandInstListInsts(list);
  }
  ++list->num_insts_;
  list->insts_[list->num_insts_] =
      list->insts_[list->num_insts_ - 1];

  /* Now install the bytes. */
  for (i = 0; i < num_bytes; ++i) {
    /* Be sure we have room for the byte. */
    if (list->num_bytes_ == list->bytes_size_) {
      ExpandInstListBytes(list);
    }

    /* Record into the ignore instruction list. */
    list->bytes_[list->num_bytes_++] = ibytes[i];
    list->insts_[list->num_insts_] = list->num_bytes_;
  }
}

/* Reads a file containing a list of instruction bytes to ignore,
 * and adds it to the end of the list of instructions.
 */
static void GetInstList(InstList** list,
                        FILE* file,
                        const char* filename) {
  char line[kBufferSize];
  int line_number = 0;
  if (NULL == *list) {
    *list = CreateEmptyInstList();
  }
  while (TRUE) {
    ++line_number;
    if (fgets(line, kBufferSize, file) == NULL) return;
    ReadInstListInst(*list, line, filename, line_number);
  }
  return;
}

/* Read the file containing a list of instruction bytes,
 * and adds it to the end of the list of instructions.
 */
static void ReadInstList(InstList** list, const char* filename) {
  FILE* file = fopen(filename, "r");
  if (NULL == file) {
    char buffer[kBufferSize];
    SNPRINTF(buffer, kBufferSize, "%s: unable to open", filename);
    ReportFatalError(buffer);
  }
  GetInstList(list, file, filename);
  fclose(file);
}

/* Very simple command line arg parsing. Returns index to the first
 * arg that doesn't begin with '--', or argc if there are none.
 */
static int ParseArgs(ComparedInstruction* cinst, int argc, char *argv[]) {
  int i;
  uint32_t prefix;
  uint32_t opcode;
  char* cstr_value;
  Bool bool_value;
  int opcode_bytes_count = 0;

  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      do {
        if (strcmp(argv[i], "--help") == 0) {
          Usage();
        } else if (GrokBoolFlag("--checkoperands", argv[i], &gCheckOperands) ||
                   GrokBoolFlag("--nacllegal", argv[i],
                                &gNaClLegal) ||
                   GrokBoolFlag("--xedimplemented", argv[i],
                                &gXedImplemented) ||
                   GrokBoolFlag("--nops", argv[i], &gNoCompareNops)||
                   GrokBoolFlag("--skipcontiguous", argv[i],
                                &gSkipContiguous) ||
                   GrokBoolFlag("--verbose", argv[i], &gVerbose)) {
        } else if (GrokBoolFlag("--bindings", argv[i], &bool_value) &&
                   bool_value) {
          PrintBindings(cinst);
#ifdef NACL_XED_DECODER
        } else if (GrokBoolFlag("--pin_version", argv[i], &bool_value) &&
                   bool_value) {
          fprintf(stderr, "pin version: %s\n", NACL_PINV);
#endif
#ifdef NACL_REVISION
        } else if (GrokBoolFlag("--nacl_revision", argv[i], &bool_value) &&
                   bool_value) {
          fprintf(stderr, "nacl revsion: %d\n", NACL_REVISION);
#endif
        } else if (GrokCstringFlag("--opcode_bytes", argv[i], &cstr_value)) {
          NaClEnumeratorDecoder* decoder =
              NaClRegisterEnumeratorDecoder(cinst, cstr_value);
          decoder->_print_opcode_sequence = TRUE;
          decoder->_print_opcode_sequence_plus_desc = FALSE;
          gSilent = TRUE;
          ++opcode_bytes_count;
        } else if (GrokCstringFlag("--opcode_bytes_plus_desc",
                                   argv[i], &cstr_value)) {
          NaClEnumeratorDecoder* decoder =
              NaClRegisterEnumeratorDecoder(cinst, cstr_value);
          decoder->_print_opcode_sequence = FALSE;
          decoder->_print_opcode_sequence_plus_desc = TRUE;
          if ((NULL == decoder->_get_inst_mnemonic_fn) ||
              (NULL == decoder->_get_inst_operands_text_fn)) {
            fprintf(stderr, "%s doesn't define how to print out description\n",
                    argv[i]);
          }
          gSilent = TRUE;
          ++opcode_bytes_count;
          /* Print out a special message to be picked up by the input decoder,
           * so that it will look for instruction mnemonic and operands.
           */
          printf("#OPCODEPLUSDESC#\n");
        } else if (GrokCstringFlag("--ignored", argv[i], &cstr_value)) {
          ReadInstList(&kIgnoredInstructions, cstr_value);
        } else if (GrokCstringFlag("--ignore_mnemonic", argv[i], &cstr_value)) {
          ReadInstList(&kIgnoreMnemonics, cstr_value);
        } else if (GrokCstringFlag("--print", argv[i], &cstr_value)) {
          NaClRegisterEnumeratorDecoder(cinst, cstr_value)->_print = TRUE;
          gSilent = TRUE;
        } else if (GrokUint32HexFlag("--prefix", argv[i], &prefix)) {
          gPrefix = (int) prefix;
          printf("using prefix %x\n", gPrefix);
        } else if (GrokUint32HexFlag("--opcode", argv[i], &opcode) &&
                   opcode < 256) {
          gOpcode = (int) opcode;
          printf("using opcode %x\n", gOpcode);
        } else if (GrokCstringFlag("--legal", argv[i], &cstr_value)) {
          NaClInstallLegalFilter(cinst, cstr_value, TRUE);
        } else if (GrokCstringFlag("--illegal", argv[i], &cstr_value)) {
          NaClInstallLegalFilter(cinst, cstr_value, FALSE);
        } else if (argv[i] == strfind(argv[i], "--")) {
          NaClRegisterEnumeratorDecoder(cinst, argv[i] + 2);
        } else {
          fprintf(stderr, "Can't recognize option %s\n", argv[i]);
          exit(1);
        }
        if (opcode_bytes_count > 1) {
          fprintf(stderr, "Can't have more than one opcode_bytes command "
                  "line argument\n");
          exit(1);
        }
      } while (0);
    } else return i;
  }
  return argc;
}

/* Define set of available enumerator decoders. */
static void NaClPreregisterEnumeratorDecoder(ComparedInstruction* cinst,
                                             NaClEnumeratorDecoder* decoder) {
  UNREFERENCED_PARAMETER(cinst);
  if (kNumAvailableDecoders >= NACL_MAX_AVAILABLE_DECODERS) {
    fprintf(stderr, "Too many preregistered enumerator decoders\n");
    exit(1);
  }
  decoder->_legal_only = TRUE;
  decoder->_print = FALSE;
  decoder->_print_opcode_sequence = FALSE;
  decoder->_print_opcode_sequence_plus_desc = FALSE;
  kAvailableDecoders[kNumAvailableDecoders++] = decoder;
}

/* Define decoders that can be registered. */
extern NaClEnumeratorDecoder* RegisterXedDecoder(void);
extern NaClEnumeratorDecoder* RegisterNaClDecoder(void);
extern NaClEnumeratorDecoder* RegisterRagelDecoder(void);

/* Initialize the set of available decoders. */
static void NaClInitializeAvailableDecoders(void) {
  kNumAvailableDecoders = 0;
#ifdef NACL_XED_DECODER
  NaClPreregisterEnumeratorDecoder(&gCinst, RegisterXedDecoder());
#endif
#ifdef NACL_RAGEL_DECODER
  NaClPreregisterEnumeratorDecoder(&gCinst, RegisterRagelDecoder());
#endif
  NaClPreregisterEnumeratorDecoder(&gCinst, RegisterNaClDecoder());
  NaClPreregisterEnumeratorDecoder(&gCinst, RegisterInputDecoder());
}

/* Initialize the ComparedInstruction data structure. */
static void NaClInitializeComparedInstruction(ComparedInstruction* cinst) {
  cinst->_enumerator._num_decoders = 0;
}

/* Install parsed globals, as appropriate into the corresponding
 * decoders.
 */
static void InstallFlags(NaClEnumerator* enumerator) {
  size_t i;
  for (i = 0; i < enumerator->_num_decoders; ++i) {
    enumerator->_decoder[i]->_install_flag_fn(enumerator,
                                             "--nops",
                                             &gNoCompareNops);
    enumerator->_decoder[i]->_install_flag_fn(enumerator,
                                             "--xedimplemented",
                                             &gXedImplemented);
  }
}

int main(int argc, char *argv[]) {
  int testargs;

  NaClLogModuleInit();
  NaClLogSetVerbosity(LOG_FATAL);
  NaClInitializeAvailableDecoders();
  NaClInitializeComparedInstruction(&gCinst);

  gArgv0 = argv[0];
  if (argc == 1) Usage();

  testargs = ParseArgs(&gCinst, argc, argv);
  InstallFlags(&gCinst._enumerator);


  if (gCinst._enumerator._num_decoders == 0) {
    fprintf(stderr, "No decoder specified, can't continue\n");
    exit(1);
  }

  if (testargs == argc) {
    if (NULL == NaClGetRegisteredDecoder(&gCinst, "in")) {
      if (gPrefix == 0) RunRegressionTests(&gCinst);
      TestAllInstructions(&gCinst);
    } else {
      TestInputInstructions(&gCinst);
    }
  } else {
    int i;
    gVerbose = TRUE;
    for (i = testargs; i < argc; ++i) {
      TestOneInstruction(&gCinst, argv[i]);
    }
  }
  exit(gSawLethalError);
}
