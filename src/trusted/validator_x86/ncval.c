/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * ncval.c - command line validator for NaCl.
 * Mostly for testing.
 */


#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/include/portability.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/timeb.h>
#include <time.h>
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/gio/gio.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/utils/flags.h"
#include "native_client/src/trusted/validator/ncfileutil.h"
#include "native_client/src/trusted/cpu_features/arch/x86/cpu_x86.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/ncvalidate_iter.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/ncvalidate_iter_internal.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/ncvalidate_iter_detailed.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/nc_jumps.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/nc_opcode_histogram.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/nc_memory_protect.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncdecode.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncdecode_verbose.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncvalidate.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncvalidate_detailed.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncvalidate_internaltypes.h"
#include "native_client/src/trusted/validator_x86/nc_read_segment.h"
#include "native_client/src/trusted/validator_x86/ncdis_segments.h"

#if NACL_TARGET_SUBARCH == 64
#include "native_client/src/trusted/validator/x86/decoder/nc_decode_tables.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/ncval_decode_tables.h"
#include "native_client/src/trusted/validator_x86/ncdis_decode_tables.h"
#endif

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

/* Forward declarations. */
static void usage(int exit_code);

#if NACL_TARGET_SUBARCH == 32
/* Flag defining if statistics should be printed for callback validator
 * model.
 */
static Bool NACL_FLAGS_stats_print = FALSE;
#endif

/* Flag defining if detailed error messages should be generated. When
 * false, runs performance model as used by sel_ldr.
 */
static Bool NACL_FLAGS_detailed_errors = TRUE;

/* Flag defining the name of a hex text to be used as the code segment.
 */
static char *NACL_FLAGS_hex_text = "";

/* Define if we should process segments (rather than sections) when applying SFI
 * validator.
 */
static Bool NACL_FLAGS_analyze_segments = FALSE;

/* Define how many times we will analyze the code segment.
 * Note: Values, other than 1, is only used for profiling.
 */
static int NACL_FLAGS_validate_attempts = 1;

/* Define the set of CPU features to use while validating. */
static NaClCPUFeaturesX86 ncval_cpu_features;

/* Define whether timing should be applied when running the validator. */
static Bool NACL_FLAGS_print_timing = FALSE;

/* Define what level of errors will be printed.
 * Note: If multiple flags are true, the one with
 * the highest severity will be selected.
 */
static Bool NACL_FLAGS_warnings = FALSE;
static Bool NACL_FLAGS_errors = FALSE;
static Bool NACL_FLAGS_fatal = FALSE;

/* Define if special stubout tests should be run. Such
 * tests apply stubout, and then print out the modified
 * disassembled code.
 */
static Bool NACL_FLAGS_stubout_memory = FALSE;

#if NACL_TARGET_SUBARCH == 64
/* Flag that allows validator decoder to be used in place of full decoder.
 */
static Bool NACL_FLAGS_validator_decoder = FALSE;
#endif

/* Generates NaClErrorMapping for error level suffix. */
#define NaClError(s) { #s , LOG_## s}

/**************************************************
 * Driver to apply the validator to a code segment
 *************************************************/

/* Reports if named module is safe. */
static void NaClReportFileSafety(Bool success, const char *fname) {
  if (NACL_FLAGS_stubout_memory) {
    /* The validator has been run to test stubbing out. Stubbing out,
     * in this tool, means replacing instructions (modeled using hex
     * text) that are unsafe and rejected by the validator, and are
     * replaced with HALT instructions.
     */
    NaClLog(LOG_INFO, "STUBBED OUT as follows:\n");
  } else {
#ifndef NCVAL_TESTING
    if (success) {
      NaClLog(LOG_INFO, "*** %s is safe ***\n", fname);
    } else {
      NaClLog(LOG_INFO, "*** %s IS UNSAFE ***\n", fname);
    }
#endif
  }
}

/* Reports if module is safe. */
static void NaClReportSafety(Bool success,
                             const char* filename) {
  if (0 != strcmp(NACL_FLAGS_hex_text, "")) {
    /* Special hex text processing, rather than filename. */
    if (0 == strcmp(NACL_FLAGS_hex_text, "-")) {
      NaClReportFileSafety(success, "<input>");
      return;
    }
  }
  NaClReportFileSafety(success, filename);
}

/* The model of data to be passed to the load/analyze steps. */
typedef void* NaClRunValidatorData;

/* The routine that loads the code segment(s) into memory (within
 * the data arg). Returns true iff load was successful.
 */
typedef Bool (*NaClValidateLoad)(int argc, const char* argv[],
                                 NaClRunValidatorData data);

/* The actual validation analysis, applied to the data returned by
 * ValidateLoad. Assume that this function also deallocates any memory
 * in loaded_data. Returns true iff analysis doesn't find any problems.
 */
typedef Bool (*NaClValidateAnalyze)(NaClRunValidatorData data);

/* Runs the validator using the given (command line) arguments.
 *
 * Parameters:
 *    data - The model of data to be passed to the load/analyze steps.
 *           Allows top-level call to pass control information
 *           to the load and analyze functions, and between these
 *           two functions.
 *    load - The function to load in the data needed to validate.
 *    analyze - The function to call to do validator analysis once
 *           the data has been read in.
 */
static Bool NaClRunValidator(int argc, const char* argv[],
                             NaClRunValidatorData data,
                             NaClValidateLoad load,
                             NaClValidateAnalyze analyze) {
  clock_t clock_0;
  clock_t clock_l;
  clock_t clock_v;
  Bool return_value;

  clock_0 = clock();
  return_value = load(argc, argv, data);
  if (!return_value) {
    NaClValidatorMessage(LOG_ERROR, NULL, "Unable to load code to validate\n");
    return FALSE;
  }
  clock_l = clock();
  return_value = analyze(data);
  clock_v = clock();

  if (NACL_FLAGS_print_timing) {
    NaClValidatorMessage(
        LOG_INFO, NULL,
        "load time: %0.6f  analysis time: %0.6f\n",
        (double)(clock_l - clock_0) /  (double)CLOCKS_PER_SEC,
        (double)(clock_v - clock_l) /  (double)CLOCKS_PER_SEC);
  }
  return return_value;
}

/* Default loader that does nothing. Typically this is because
 * the data argument already contains the bytes to validate.
 */
Bool NaClValidateNoLoad(int argc, const char* argv[],
                        NaClRunValidatorData data) {
  return TRUE;
}

/* Local file data for validator run. */
typedef struct ValidateData {
  /* The name of the elf file to validate. */
  const char *fname;
  /* The elf file to validate. */
  ncfile *ncf;
} ValidateData;

/* Load the elf file and return the loaded elf file. */
static Bool ValidateElfLoad(int argc, const char *argv[],
                                ValidateData *data) {
  if (argc != 2) {
    NaClLog(LOG_ERROR, "expected: %s file\n", argv[0]);
    usage(2);
  }
  data->fname = argv[1];

  /* TODO(karl): Once we fix elf values put in by compiler, so that
   * we no longer get load errors from ncfilutil.c, find a way to
   * terminate early if errors occur during loading.
   */
  data->ncf = nc_loadfile(data->fname);
  if (data->ncf == NULL) {
    NaClLog(LOG_ERROR, "nc_loadfile(%s): %s\n", data->fname, strerror(errno));
    usage(2);
  }
  return NULL != data->ncf;
}

/***************************************************
 * Code to run SFI validator on hex text examples. *
 ***************************************************/

/* Defines the maximum number of characters allowed on an input line
 * of the input text defined by the commands command line option.
 */
#define NACL_MAX_INPUT_LINE 4096

typedef struct NaClValidatorByteArray {
  uint8_t bytes[NACL_MAX_INPUT_LINE];
  NaClPcAddress base;
  NaClMemorySize num_bytes;
} NaClValidatorByteArray;

static void HexFatal(const char *format, ...) ATTRIBUTE_FORMAT_PRINTF(1, 2);

/* Print out given error message about the hex input file, and then exit. */
static void HexFatal(const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  NaClLogV(LOG_ERROR, format, ap);
  va_end(ap);
  /* always succed, so that the testing framework works. */
  exit(0);
}

/* Load the hex input file from the given command line arguments,
 * and put it into the given data structure.
 */
static int ValidateHexLoad(int argc, const char *argv[],
                              NaClValidatorByteArray *data) {
  if (argc != 1) {
    HexFatal("expected: %s <options>\n", argv[0]);
  }
  if (0 == strcmp(NACL_FLAGS_hex_text, "-")) {
    data->num_bytes = (NaClMemorySize)
        NaClReadHexTextWithPc(stdin, &data->base, data->bytes,
                              NACL_MAX_INPUT_LINE);
  } else {
    FILE *input = fopen(NACL_FLAGS_hex_text, "r");
    if (NULL == input) {
      HexFatal("Can't open hex text file: %s\n", NACL_FLAGS_hex_text);
    }
    data->num_bytes = (NaClMemorySize)
        NaClReadHexTextWithPc(input, &data->base, data->bytes,
                              NACL_MAX_INPUT_LINE);
    fclose(input);
    --argc;
  }
  return argc;
}

#if NACL_TARGET_SUBARCH == 32
/***************************************
 * Code to run segment based validator.*
 ***************************************/

int AnalyzeSegmentSections(ncfile *ncf, struct NCValidatorState *vstate) {
  int badsections = 0;
  int ii;
  const Elf_Phdr *phdr = ncf->pheaders;

  for (ii = 0; ii < ncf->phnum; ii++) {
    NaClLog(LOG_INFO,
        "segment[%d] p_type %"NACL_PRIdElf_Word
        " p_offset %"NACL_PRIxElf_Off" vaddr %"NACL_PRIxElf_Addr
        " paddr %"NACL_PRIxElf_Addr" align %"NACL_PRIuElf_Xword"\n",
        ii, phdr[ii].p_type, phdr[ii].p_offset,
        phdr[ii].p_vaddr, phdr[ii].p_paddr,
        phdr[ii].p_align);

    NaClLog(LOG_INFO,
            "    filesz %"NACL_PRIxElf_Xword" memsz %"NACL_PRIxElf_Xword
            " flags %"NACL_PRIxElf_Word"\n",
          phdr[ii].p_filesz, phdr[ii].p_memsz, phdr[ii].p_flags);
    if ((PT_LOAD != phdr[ii].p_type) ||
        (0 == (phdr[ii].p_flags & PF_X)))
      continue;
    NaClLog(LOG_INFO, "parsing segment %d\n", ii);
    NCValidateSegment(ncf->data + (phdr[ii].p_vaddr - ncf->vbase),
                      phdr[ii].p_vaddr, phdr[ii].p_memsz, vstate);
  }
  return -badsections;
}

/* Initialize segment validator, using detailed (summary) error
 * messages if selected.
 */
struct NCValidatorState* NCValInit(const NaClPcAddress vbase,
                                   const NaClMemorySize codesize) {
  return NACL_FLAGS_detailed_errors
      ? NCValidateInitDetailed(vbase, codesize, &ncval_cpu_features)
      : NCValidateInit(vbase, codesize, FALSE, &ncval_cpu_features);
}


static Bool AnalyzeSegmentCodeSegments(ncfile *ncf, const char *fname) {
  NaClPcAddress vbase, vlimit;
  struct NCValidatorState *vstate;
  Bool result;

  GetVBaseAndLimit(ncf, &vbase, &vlimit);
  vstate = NCValInit(vbase, vlimit - vbase);
  if (vstate == NULL) return FALSE;
  NCValidateSetErrorReporter(vstate, &kNCVerboseErrorReporter);
  if (AnalyzeSegmentSections(ncf, vstate) < 0) {
    NaClLog(LOG_INFO, "%s: text validate failed\n", fname);
  }
  result = (0 == NCValidateFinish(vstate)) ? TRUE : FALSE;
  NaClReportSafety(result, fname);
  if (NACL_FLAGS_stats_print) NCStatsPrint(vstate);
  NCValidateFreeState(&vstate);
  NaClLog(LOG_INFO, "Validated %s\n", fname);
  return result;
}
#endif

#if NACL_TARGET_SUBARCH == 64
/******************************
 * Code to run SFI validator. *
 ******************************/

/* Define what should be used as the base register for
 * memory accesses.
 */
static NaClOpKind nacl_base_register =
    (64 == NACL_TARGET_SUBARCH ? RegR15 : RegUnknown);

/* Create validator state using detailed (summary) error messages
 * if selected.
 */
struct NaClValidatorState* NaClValStateCreate(
    const NaClPcAddress vbase,
    const NaClMemorySize sz,
    const NaClOpKind base_register) {
  return
      NACL_FLAGS_detailed_errors
      ? NaClValidatorStateCreateDetailed(vbase, sz, base_register,
                                         &ncval_cpu_features)
      : NaClValidatorStateCreate(vbase, sz, base_register, FALSE,
                                 &ncval_cpu_features);
}

/* Returns the decoder tables to use. */
static const NaClDecodeTables* NaClGetDecoderTables(void) {
  return NACL_FLAGS_validator_decoder
      ? kNaClValDecoderTables
      : kNaClDecoderTables;
}

/* Analyze each section in the given elf file, using the given validator
 * state.
 */
static void AnalyzeSfiSections(ncfile *ncf, struct NaClValidatorState *vstate) {
  int ii;
  const Elf_Phdr *phdr = ncf->pheaders;

  for (ii = 0; ii < ncf->phnum; ii++) {
    /* TODO(karl) fix types for this? */
    NaClValidatorMessage(
        LOG_INFO, vstate,
        "segment[%d] p_type %d p_offset %"NACL_PRIxElf_Off
        " vaddr %"NACL_PRIxElf_Addr
        " paddr %"NACL_PRIxElf_Addr
        " align %"NACL_PRIuElf_Xword"\n",
        ii, phdr[ii].p_type, phdr[ii].p_offset,
        phdr[ii].p_vaddr, phdr[ii].p_paddr,
        phdr[ii].p_align);
    NaClValidatorMessage(
        LOG_INFO, vstate,
        "    filesz %"NACL_PRIxElf_Xword
        " memsz %"NACL_PRIxElf_Xword
        " flags %"NACL_PRIxElf_Word"\n",
        phdr[ii].p_filesz, phdr[ii].p_memsz,
        phdr[ii].p_flags);
    if ((PT_LOAD != phdr[ii].p_type) ||
        (0 == (phdr[ii].p_flags & PF_X)))
      continue;
    NaClValidatorMessage(LOG_INFO, vstate, "parsing segment %d\n", ii);
    NaClValidateSegmentUsingTables(ncf->data + (phdr[ii].p_vaddr - ncf->vbase),
                                   phdr[ii].p_vaddr, phdr[ii].p_memsz, vstate,
                                   NaClGetDecoderTables());
  }
}

static void AnalyzeSfiSegments(ncfile *ncf, NaClValidatorState *state) {
  int ii;
  const Elf_Shdr *shdr = ncf->sheaders;

  for (ii = 0; ii < ncf->shnum; ii++) {
    NaClValidatorMessage(
        LOG_INFO, state,
        "section %d sh_addr %"NACL_PRIxElf_Addr" offset %"NACL_PRIxElf_Off
        " flags %"NACL_PRIxElf_Xword"\n",
         ii, shdr[ii].sh_addr, shdr[ii].sh_offset, shdr[ii].sh_flags);
    if ((shdr[ii].sh_flags & SHF_EXECINSTR) != SHF_EXECINSTR)
      continue;
    NaClValidatorMessage(LOG_INFO, state, "parsing section %d\n", ii);
    NaClValidateSegmentUsingTables(ncf->data + (shdr[ii].sh_addr - ncf->vbase),
                                   shdr[ii].sh_addr, shdr[ii].sh_size, state,
                                   NaClGetDecoderTables());
  }
}

/* Analyze each code segment in the given elf file, stored in the
 * file with the given path fname.
 */
static Bool AnalyzeSfiCodeSegments(ncfile *ncf, const char *fname) {
  /* TODO(karl) convert these to be PcAddress and MemorySize */
  NaClPcAddress vbase, vlimit;
  NaClValidatorState *vstate;
  Bool return_value = TRUE;

  GetVBaseAndLimit(ncf, &vbase, &vlimit);
  vstate = NaClValStateCreate(vbase, vlimit - vbase, nacl_base_register);
  if (vstate == NULL) {
    NaClValidatorMessage(LOG_ERROR, vstate, "Unable to create validator state");
    return FALSE;
  }
  NaClValidatorStateSetErrorReporter(vstate, &kNaClVerboseErrorReporter);
  if (NACL_FLAGS_analyze_segments) {
    AnalyzeSfiSegments(ncf, vstate);
  } else {
    AnalyzeSfiSections(ncf, vstate);
  }
  return_value = NaClValidatesOk(vstate);
  NaClReportSafety(return_value, fname);
  NaClValidatorStateDestroy(vstate);
  return return_value;
}
#endif

/***************************
 * Top-level driver code. *
 **************************/

/* Analyze the code segments of the elf file in the validator date. */
static Bool ValidateAnalyze(ValidateData *data) {
  int i;
  Bool results = TRUE;
  for (i = 0; i < NACL_FLAGS_validate_attempts; ++i) {
    Bool return_value =
#if NACL_TARGET_SUBARCH == 64
        AnalyzeSfiCodeSegments(data->ncf, data->fname);
#else
        AnalyzeSegmentCodeSegments(data->ncf, data->fname);
#endif
    if (!return_value) {
      results = FALSE;
    }
  }
  nc_freefile(data->ncf);
  return results;
}

/* Define a sequence of bytes to validate whose virtual address begins at the
 * given base.
 */
typedef struct NaClValidateBytes {
  /* The sequence of bytes to validate. */
  uint8_t* bytes;
  /* The number of bytes in the sequence. */
  NaClMemorySize num_bytes;
  /* The virtual base adddress associated with the first byte in the
   * sequence.
   */
  NaClPcAddress base;
} NaClValidateBytes;

/* Apply the validator to the sequence of bytes in the given data. */
static Bool NaClValidateAnalyzeBytes(NaClValidateBytes* data) {
  Bool return_value = FALSE;
#if NACL_TARGET_SUBARCH == 64
  NaClValidatorState* state;
  state = NaClValStateCreate(data->base,
                             data->num_bytes,
                             nacl_base_register);
  if (NULL == state) {
    NaClValidatorMessage(LOG_ERROR, NULL, "Unable to create validator state");
    return FALSE;
  }
  if (NACL_FLAGS_stubout_memory) {
    NaClValidatorStateSetDoStubOut(state, TRUE);
  }
  NaClValidatorStateSetErrorReporter(state, &kNaClVerboseErrorReporter);
  NaClValidateSegmentUsingTables(data->bytes, data->base, data->num_bytes,
                                 state, NaClGetDecoderTables());
  return_value = NaClValidatesOk(state);
  if (state->did_stub_out) {
    /* Used for golden file testing. */
    printf("Some instructions were replaced with HLTs.\n");
  }
  NaClValidatorStateDestroy(state);
  NaClReportSafety(return_value, "");
#else
  struct NCValidatorState *vstate;
  vstate = NCValInit(data->base, data->num_bytes);
  if (vstate == NULL) {
    printf("Unable to create validator state, quitting!\n");
  } else {
    if (NACL_FLAGS_stubout_memory) {
      NCValidateSetStubOutMode(vstate, 1);
    }
    NCValidateSetErrorReporter(vstate, &kNCVerboseErrorReporter);
    NCValidateSegment(&data->bytes[0], data->base, data->num_bytes, vstate);
    return_value = (0 == NCValidateFinish(vstate)) ? TRUE : FALSE;
    if (vstate->stats.didstubout) {
      /* Used for golden file testing. */
      printf("Some instructions were replaced with HLTs.\n");
    }
    NaClReportSafety(return_value, "");
    if (NACL_FLAGS_stats_print) NCStatsPrint(vstate);
    NCValidateFreeState(&vstate);
  }
#endif
  return return_value;
}

/* Given the command line arguments in argc/argv, and the sequence of bytes
 * in the given data, apply the validator to the given bytes.
 */
Bool NaClRunValidatorBytes(int argc,
                           const char* argv[],
                           uint8_t* bytes,
                           NaClMemorySize num_bytes,
                           NaClPcAddress base) {
  NaClValidateBytes data;
  int i;
  Bool results = TRUE;
  data.bytes = bytes;
  data.num_bytes = num_bytes;
  data.base = base;
  for (i = 0; i < NACL_FLAGS_validate_attempts; ++i) {
    if (!NaClRunValidator(argc, argv, &data,
                          (NaClValidateLoad) NaClValidateNoLoad,
                          (NaClValidateAnalyze) NaClValidateAnalyzeBytes)) {
      results = FALSE;
    }
  }
  return results;
}


static const char usage_str[] =
      "usage: ncval [options] file\n"
      "\n"
      "\tValidates an x86-%d nexe file.\n"
      "\n"
      "Options are:\n"
      "\n"
#ifdef NCVAL_TESTING
      "--print_conditions\n"
      "\tPrint all pre/post conditions, including NaCl illegal instructions.\n"
#endif
      "--annotate\n"
      "\tRun validator using annotations that will be understood\n"
      "\tby ncval_annotate.py.\n"
      "--attempts=N\n"
      "\tRun the validator on the nexe file (after loading) N times.\n"
      "\tNote: this flag should only be used for profiling.\n"
      "--CLFLUSH\n"
      "\tModel a CPU that supports the clflush instruction.\n"
      "--CMOV\n"
      "\tModel a CPU that supports the cmov instructions.\n"
#ifdef NCVAL_TESTING
      "--conds\n"
      "\tPrint out pre/post conditions associated with each instruction.\n"
#endif
      "--cpuid-all\n"
      "\tModel a CPU that supports all available features.\n"
      "--cpuid-none\n"
      "\tModel a CPU that supports no avaliable features.\n"
      "--CX8\n"
      "\tModel a CPU that supports the cmpxchg8b instruction.\n"
      "--detailed\n"
      "\tPrint out detailed error messages, rather than use performant\n"
      "\tcode used by sel_ldr\n"
      "--errors\n"
      "\tPrint out error and fatal error messages, but not\n"
      "\tinformative and warning messages\n"
      "--fatal\n"
      "\tOnly print out fatal error messages.\n"
      "--FXSR\n"
      "\tModel a CPU that supports the sfence instructions.\n"
      "--help\n"
      "\tPrint this message, and then exit.\n"
      "--hex_text=<file>\n"
      "\tRead text file of hex bytes, and use that\n"
      "\tas the definition of the code segment. Note: -hex_text=- will\n"
      "\tread from stdin instead of a file.\n"
#if NACL_TARGET_SUBARCH == 64
      "--histogram\n"
      "\tPrint out a histogram of found opcodes.\n"
      "--identity_mask\n"
      "\tMask jumps using 0xFF instead of one matching\n"
      "\tthe block alignment.\n"
#endif
      "--local_cpuid\n"
      "\tSet cpuid to values defined by local host this command is run on.\n"
      "--LZCNT\n"
      "\tModel a CPU that supports the lzcnt instruction.\n"
#if NACL_TARGET_SUBARCH == 64
      "--max_errors=N\n"
      "\tPrint out at most N error messages. If N is zero,\n"
      "\tno messages are printed. If N is < 0, all messages are printed.\n"
#endif
      "--MMX\n"
      "\tModel a CPU that supports MMX instructions.\n"
      "--MOVBE\n"
      "\tModel a CPU that supports the movbe instruction.\n"
      "--POPCNT\n"
      "\tModel a CPU that supports the popcnt instruction.\n"
#if NACL_TARGET_SUBARCH == 64
      "--readwrite_sfi\n"
      "\tCheck for memory read and write software fault isolation.\n"
      "--segments\n"
      "\tAnalyze code in segments in elf file, instead of headers.\n"
#endif
      "--SSE\n"
      "\tModel a CPU that supports SSE instructions.\n"
      "--SSE2\n"
      "\tModel a CPU that supports SSE 2 instructions.\n"
      "--SSE3\n"
      "\tModel a CPU that supports SSE 3 instructions.\n"
      "--SSSE3\n"
      "\tModel a CPU that supports SSSE 3 instructions.\n"
      "--SSE41\n"
      "\tModel a CPU that supports SSE 4.1 instructions.\n"
      "--SSE42\n"
      "\tModel a CPU that supports SSE 4.2 instructions.\n"
      "--SSE4A\n"
      "\tModel a CPU that supports SSE 4A instructions.\n"
#if NACL_TARGET_SUBARCH == 32
      "--stats\n"
      "\tPrint statistics collected by the validator.\n"
#endif
      "--stubout\n"
      "\tRun using stubout mode, replacing bad instructions with halts.\n"
      "\tStubbed out disassembly will be printed after validator\n"
      "\terror messages. Note: Only applied if --hex_text option is\n"
      "\talso specified\n"
      "-t\n"
      "\tTime the validator and print out results.\n"
      "--TSC\n"
      "\tModel a CPU that supports the rdtsc instruction.\n"
      "--x87\n"
      "\tModel a CPU that supports x87 instructions.\n"
#if NACL_TARGET_SUBARCH == 64
      "--validator_decoder\n"
      "\tUse validator (partial) decoder instead of full decoder.\n"
#endif
      "--3DNOW\n"
      "\tModel a CPU that supports 3DNOW instructions.\n"
      "--E3DNOW\n"
      "\tModel a CPU that supports E3DNOW instructions.\n"
      "\n"
      "--time\n"
      "\tTime the validator and print out results. Same as option -t.\n"
#if NACL_TARGET_SUBARCH == 64
      "--trace_insts\n"
      "\tTurn on validator trace of instructions, as processed..\n"
      "--trace_verbose\n"
      "\tTurn on all trace validator messages. Note: this\n"
      "\tflag also implies --trace.\n"
#endif
      "--warnings\n"
      "\tPrint out warnings, errors, and fatal errors, but not\n"
      "\tinformative messages.\n"
#if NACL_TARGET_SUBARCH == 64
      "--write_sfi\n"
      "\tOnly check for memory write software fault isolation.\n"
#endif
      "\n";

static void usage(int exit_code) {
  printf(usage_str, NACL_TARGET_SUBARCH);
  exit(exit_code);
}

/* Checks if arg is one of the expected "Bool" flags, and if so, sets
 * the corresponding flag and returns true.
 */
static Bool GrokABoolFlag(const char *arg) {
  /* A set of boolean flags to be checked */
  static struct {
    const char *flag_name;
    Bool *flag_ptr;
  } flags[] = {
    { "--segments" , &NACL_FLAGS_analyze_segments },
#ifdef NCVAL_TESTING
    { "--print_all_conds", &NACL_FLAGS_report_conditions_on_all },
#else
    { "--detailed", &NACL_FLAGS_detailed_errors },
#endif
    { "--stubout", &NACL_FLAGS_stubout_memory },
#if NACL_TARGET_SUBARCH == 64
    { "--trace_insts", &NACL_FLAGS_validator_trace_instructions },
#endif
    { "-t", &NACL_FLAGS_print_timing },
#if NACL_TARGET_SUBARCH == 32
    { "--stats", &NACL_FLAGS_stats_print },
#endif
    { "--annotate", &NACL_FLAGS_ncval_annotate },
#if NACL_TARGET_SUBARCH == 64
    { "--histogram", &NACL_FLAGS_opcode_histogram },
#endif
    { "--time"   , &NACL_FLAGS_print_timing },
#if NACL_TARGET_SUBARCH == 64
    { "--warnings", &NACL_FLAGS_warnings },
    { "--errors" , &NACL_FLAGS_errors },
    { "--fatal"  , &NACL_FLAGS_fatal },
    { "--validator_decoder", &NACL_FLAGS_validator_decoder },
#endif
    { "--identity_mask", &NACL_FLAGS_identity_mask },
  };

  /* A set of CPU features to check. */
  static struct {
    const char *feature_name;
    NaClCPUFeatureX86ID feature;
  } features[] = {
    { "--x87"    , NaClCPUFeatureX86_x87 },
    { "--MMX"    , NaClCPUFeatureX86_MMX },
    { "--SSE"    , NaClCPUFeatureX86_SSE },
    { "--SSE2"   , NaClCPUFeatureX86_SSE2 },
    { "--SSE3"   , NaClCPUFeatureX86_SSE3 },
    { "--SSSE3"  , NaClCPUFeatureX86_SSSE3 },
    { "--SSE41"  , NaClCPUFeatureX86_SSE41 },
    { "--SSE42"  , NaClCPUFeatureX86_SSE42 },
    { "--MOVBE"  , NaClCPUFeatureX86_MOVBE },
    { "--POPCNT" , NaClCPUFeatureX86_POPCNT },
    { "--CX8"    , NaClCPUFeatureX86_CX8 },
    { "--CX16"   , NaClCPUFeatureX86_CX16 },
    { "--CMOV"   , NaClCPUFeatureX86_CMOV },
    { "--MON"    , NaClCPUFeatureX86_MON },
    { "--FXSR"   , NaClCPUFeatureX86_FXSR },
    { "--CLFLUSH", NaClCPUFeatureX86_CLFLUSH },
    { "--TSC"    , NaClCPUFeatureX86_TSC },
    { "--3DNOW"  , NaClCPUFeatureX86_3DNOW },
    { "--EMMX"   , NaClCPUFeatureX86_EMMX },
    { "--E3DNOW" , NaClCPUFeatureX86_E3DNOW },
    { "--LZCNT"  , NaClCPUFeatureX86_LZCNT },
    { "--SSE4A"  , NaClCPUFeatureX86_SSE4A },
    { "--LM"     , NaClCPUFeatureX86_LM },
  };

  int i;
  Bool flag_state;
  for (i = 0; i < NACL_ARRAY_SIZE(flags); ++i) {
    if (GrokBoolFlag(flags[i].flag_name, arg, flags[i].flag_ptr)) {
      return TRUE;
    }
  }
  for (i = 0; i < NACL_ARRAY_SIZE(features); ++i) {
    if (GrokBoolFlag(features[i].feature_name, arg, &flag_state)) {
      NaClSetCPUFeatureX86(&ncval_cpu_features, features[i].feature,
                           flag_state);
      return TRUE;
    }
  }

  return FALSE;
}

/* Checks if arg is one of the expected "int" flags, and if so, sets
 * the corresponding flag and returns true.
 */
static Bool GrokAnIntFlag(const char *arg) {
  /* A set of boolean flags to be checked */
  static struct {
    const char *flag_name;
    int *flag_ptr;
  } flags[] = {
    { "--max_errors", &NACL_FLAGS_max_reported_errors},
    { "--attempts"  , &NACL_FLAGS_validate_attempts },
  };
  int i;
  for (i = 0; i < NACL_ARRAY_SIZE(flags); ++i) {
    if (GrokIntFlag(flags[i].flag_name, arg, flags[i].flag_ptr)) {
      return TRUE;
    }
  }
  return FALSE;
}

static int GrokFlags(int argc, const char *argv[]) {
  int i;
  int new_argc;
  Bool help = FALSE;
#if NACL_TARGET_SUBARCH == 64
  Bool only_write_sandbox;
#endif
  if (argc == 0) return 0;
  new_argc = 1;
  for (i = 1; i < argc; ++i) {
    Bool flag;
    const char *arg = argv[i];
    if (GrokABoolFlag(arg) ||
        GrokAnIntFlag(arg) ||
        GrokCstringFlag("--hex_text", arg, &NACL_FLAGS_hex_text)) {
      /* Valid processed flag, continue to next flag. */
    } else if (GrokBoolFlag("--help", arg, &help)) {
        usage(0);
    }
#if NACL_TARGET_SUBARCH == 64
    else if (0 == strcmp("--trace_verbose", arg)) {
      NaClValidatorFlagsSetTraceVerbose();
    } else if (GrokBoolFlag("--write_sfi", arg, &only_write_sandbox)) {
      NACL_FLAGS_read_sandbox = !only_write_sandbox;
    } else if (GrokBoolFlag("--readwrite_sfi", arg,
                            &NACL_FLAGS_read_sandbox)) {
    }
#endif
    else if (0 == strcmp("--cpuid-all", arg)) {
      NaClSetAllCPUFeaturesX86((NaClCPUFeatures *) &ncval_cpu_features);
    } else if (0 == strcmp("--cpuid-none", arg)) {
      NaClClearCPUFeaturesX86(&ncval_cpu_features);
    } else if (GrokBoolFlag("--local_cpuid", arg, &flag)) {
      NaClGetCurrentCPUFeaturesX86((NaClCPUFeatures *) &ncval_cpu_features);
    } else {
      argv[new_argc++] = argv[i];
    }
  }

  /* Before returning, update internals to match command line
   * values found.
   */
  if (NACL_FLAGS_warnings) {
    NaClLogSetVerbosity(LOG_WARNING);
  }
  if (NACL_FLAGS_errors) {
    NaClLogSetVerbosity(LOG_ERROR);
  }
  if (NACL_FLAGS_fatal) {
    NaClLogSetVerbosity(LOG_FATAL);
  }
  NCValidatorSetMaxDiagnostics(NACL_FLAGS_max_reported_errors);
  if (NACL_FLAGS_stubout_memory && (NACL_FLAGS_validate_attempts != 1)) {
    fprintf(stderr, "Can't specify --stubout when --attempts!=1\n");
  }

  return new_argc;
}

/* Decode and print out code segment if stubout memory is specified
 * command line.
 */
static void NaClMaybeDecodeDataSegment(
    uint8_t *mbase, NaClPcAddress vbase, NaClMemorySize size) {
  if (NACL_FLAGS_stubout_memory) {
    /* Disassemble data segment to see how halts were inserted.
     * Note: We use the full decoder (rather than the validator decoder)
     * because the validator decoders are partial decodings, and can be
     * confusing to the reader.
     */
    NaClDisassembleSegment(mbase, vbase, size,
                           NACL_DISASSEMBLE_FLAG(NaClDisassembleFull));
  }
}

int main(int argc, const char *argv[]) {
  int result = 0;
  struct GioFile gio_out_stream;
  struct Gio *gout = (struct Gio*) &gio_out_stream;

  if (argc < 2) {
    fprintf(stderr, "expected: %s file\n", argv[0]);
    usage(1);
  }

  /* Set up logging. */
  if (!GioFileRefCtor(&gio_out_stream, stdout)) {
    fprintf(stderr, "Unable to create gio file for stdout!\n");
    return 1;
  }
  NaClLogModuleInitExtended(LOG_INFO, gout);
  NaClLogDisableTimestamp();

  /* By default, assume no local cpu features are available. */
  NaClClearCPUFeaturesX86(&ncval_cpu_features);

  /* Validate the specified input. */
  argc = GrokFlags(argc, argv);
  if (0 == strcmp(NACL_FLAGS_hex_text, "")) {
    /* Run validator on elf file. */
    ValidateData data;
    Bool success = NaClRunValidator(
        argc, argv, &data,
        (NaClValidateLoad) ValidateElfLoad,
        (NaClValidateAnalyze) ValidateAnalyze);
    NaClReportSafety(success, argv[1]);
    result = (success ? 0 : 1);
  } else {
    /* Run validator on hex text file. */
    NaClValidatorByteArray data;
    argc = ValidateHexLoad(argc, argv, &data);
    NaClRunValidatorBytes(
        argc, argv, (uint8_t*) &data.bytes,
        data.num_bytes, data.base);
    NaClMaybeDecodeDataSegment(&data.bytes[0], data.base, data.num_bytes);
    /* always succeed, so that the testing framework works. */
    result = 0;
  }

  NaClLogModuleFini();
  GioFileDtor(gout);
  return result;
}
