/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
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
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/utils/flags.h"
#include "native_client/src/trusted/validator/ncfileutil.h"
#include "native_client/src/trusted/validator/x86/nacl_cpuid.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/ncvalidate_iter.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/ncvalidator_registry.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/nc_memory_protect.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncdecode.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncdecode_verbose.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncvalidate.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncvalidate_detailed.h"
#include "native_client/src/trusted/validator_x86/nc_read_segment.h"
#include "native_client/src/trusted/validator_x86/ncdis_segments.h"
#include "native_client/src/trusted/validator_x86/ncval_driver.h"

/* Flag defining if statistics should be printed for callback validator
 * model.
 */
static Bool NACL_FLAGS_stats_print = FALSE;

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
static CPUFeatures ncval_cpu_features;

/***************************************
 * Code to run segment based validator.*
 ***************************************/

static void SegmentInfo(const char *fmt, ...) ATTRIBUTE_FORMAT_PRINTF(1, 2);

static void SegmentInfo(const char *fmt, ...) {
  va_list ap;
  fprintf(stdout, "I: ");
  va_start(ap, fmt);
  vfprintf(stdout, fmt, ap);
  va_end(ap);
}

static void SegmentDebug(const char *fmt, ...) ATTRIBUTE_FORMAT_PRINTF(1, 2);

static void SegmentDebug(const char *fmt, ...) {
  va_list ap;
  fprintf(stdout, "D: ");
  va_start(ap, fmt);
  vfprintf(stdout, fmt, ap);
  va_end(ap);
}

static void SegmentFatal(const char *fmt, ...) ATTRIBUTE_FORMAT_PRINTF(1, 2);

static void SegmentFatal(const char *fmt, ...) {
  va_list ap;
  fprintf(stdout, "Fatal error: ");
  va_start(ap, fmt);
  vfprintf(stdout, fmt, ap);
  va_end(ap);
  exit(2);
}

/* Decode and print out code segment if stubout memory is specified
 * command line.
 */
static void NaClMaybeDecodeDataSegment(
    uint8_t *mbase, NaClPcAddress vbase, NaClMemorySize size) {
  if (NACL_FLAGS_stubout_memory) {
    /* Disassemble data segment to see how halts were inserted. */
    NaClDisassembleSegment(mbase, vbase, size);
  }
}

int AnalyzeSegmentSections(ncfile *ncf, struct NCValidatorState *vstate) {
  int badsections = 0;
  int ii;
  const Elf_Phdr *phdr = ncf->pheaders;

  for (ii = 0; ii < ncf->phnum; ii++) {
    SegmentDebug(
        "segment[%d] p_type %"NACL_PRIdElf_Word
        " p_offset %"NACL_PRIxElf_Off" vaddr %"NACL_PRIxElf_Addr
        " paddr %"NACL_PRIxElf_Addr" align %"NACL_PRIuElf_Xword"\n",
        ii, phdr[ii].p_type, phdr[ii].p_offset,
        phdr[ii].p_vaddr, phdr[ii].p_paddr,
        phdr[ii].p_align);

    SegmentDebug("    filesz %"NACL_PRIxElf_Xword" memsz %"NACL_PRIxElf_Xword
                 " flags %"NACL_PRIxElf_Word"\n",
          phdr[ii].p_filesz, phdr[ii].p_memsz, phdr[ii].p_flags);
    if ((PT_LOAD != phdr[ii].p_type) ||
        (0 == (phdr[ii].p_flags & PF_X)))
      continue;
    SegmentDebug("parsing segment %d\n", ii);
    NCValidateSegment(ncf->data + (phdr[ii].p_vaddr - ncf->vbase),
                      phdr[ii].p_vaddr, phdr[ii].p_memsz, vstate);
  }
  return -badsections;
}

/* Reports if module is safe. */
static void NCValidateResults(int result, const char *fname) {
  if (NACL_FLAGS_stubout_memory) {
    /* The validator has been run to test stubbing out. Stubbing out,
     * in this tool, means replacing instructions (modeled using hex
     * text) that are unsafe and rejected by the validator, and are
     * replaced with HALT instructions.
     */
    SegmentDebug("STUBBED OUT as follows:\n");
  } else {
    if (result == 0) {
      SegmentDebug("***module %s is safe***\n", fname);
    } else {
      SegmentDebug("***MODULE %s IS UNSAFE***\n", fname);
    }
  }
}

/* Initialize segment validator, using detailed (summary) error
 * messages if selected.
 */
struct NCValidatorState* NCValInit(const NaClPcAddress vbase,
                                   const NaClPcAddress vlimit,
                                   const uint8_t alignment) {
  return NACL_FLAGS_detailed_errors
      ? NCValidateInitDetailed(vbase, vlimit, alignment)
      : NCValidateInit(vbase, vlimit, alignment);
}

static int AnalyzeSegmentCodeSegments(ncfile *ncf, const char *fname) {
  NaClPcAddress vbase, vlimit;
  struct NCValidatorState *vstate;
  int result;

  GetVBaseAndLimit(ncf, &vbase, &vlimit);
  vstate = NCValInit(vbase, vlimit, ncf->ncalign);
  if (vstate == NULL) return 0;
  NCValidateSetErrorReporter(vstate, &kNCVerboseErrorReporter);
  if (AnalyzeSegmentSections(ncf, vstate) < 0) {
    SegmentInfo("%s: text validate failed\n", fname);
  }
  result = NCValidateFinish(vstate);
  NCValidateResults(result, fname);
  if (NACL_FLAGS_stats_print) NCStatsPrint(vstate);
  NCValidateFreeState(&vstate);
  SegmentDebug("Validated %s\n", fname);
  return result;
}

/******************************
 * Code to run SFI validator. *
 ******************************/

/* Reports if module is safe. */
static void NaClReportSafety(Bool success) {
  if (NACL_FLAGS_stubout_memory) {
    /* The validator has been run to test stubbing out. Stubbing out,
     * in this tool, means replacing instructions (modeled using hex
     * text) that are unsafe and rejected by the validator, and are
     * replaced with HALT instructions.
     */
    gprintf(NaClLogGetGio(), "STUBBED OUT as follows:\n");
  } else {
    if (success) {
      gprintf(NaClLogGetGio(), "***module is safe***\n");
    } else {
      gprintf(NaClLogGetGio(), "***MODULE IS UNSAFE***\n");
    }
  }
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
    NaClValidateSegment(ncf->data + (phdr[ii].p_vaddr - ncf->vbase),
                        phdr[ii].p_vaddr, phdr[ii].p_memsz, vstate);
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
    NaClValidateSegment(ncf->data + (shdr[ii].sh_addr - ncf->vbase),
                        shdr[ii].sh_addr, shdr[ii].sh_size, state);
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
  vstate = NaClValidatorStateCreate(vbase, vlimit - vbase,
                                    ncf->ncalign, nacl_base_register);
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
  if (NaClValidatesOk(vstate)) {
    NaClValidatorMessage(LOG_INFO, vstate, "***module %s is safe***\n", fname);
  } else {
    return_value = FALSE;
    NaClValidatorMessage(LOG_INFO, vstate,
                         "***MODULE %s IS UNSAFE***\n", fname);
  }
  NaClValidatorMessage(LOG_INFO, vstate, "Validated %s\n", fname);
  NaClValidatorStateDestroy(vstate);
  return return_value;
}

/* Local data to validator run. */
typedef struct ValidateData {
  /* The name of the elf file to validate. */
  const char *fname;
  /* The elf file to validate. */
  ncfile *ncf;
} ValidateData;

/* Prints out appropriate error message during call to ValidateSfiLoad. */
static void ValidateSfiLoadPrintError(const char *format,
                                   ...) ATTRIBUTE_FORMAT_PRINTF(1,2);

static void ValidateSfiLoadPrintError(const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  NaClValidatorVarargMessage(LOG_ERROR, NULL, format, ap);
  va_end(ap);
}

/* Loads the elf file defined by fname. For use within ValidateSfiLoad. */
static ncfile *ValidateSfiLoadFile(const char *fname) {
  return nc_loadfile_with_error_fn(fname, ValidateSfiLoadPrintError);
}

/* Load the elf file and return the loaded elf file. */
static Bool ValidateSfiLoad(int argc, const char *argv[], ValidateData *data) {
  if (argc != 2) {
    NaClValidatorMessage(LOG_ERROR, NULL, "expected: %s file\n", argv[0]);
    return FALSE;
  }
  data->fname = argv[1];

  /* TODO(karl): Once we fix elf values put in by compiler, so that
   * we no longer get load errors from ncfilutil.c, find a way to
   * terminate early if errors occur during loading.
   */
  data->ncf = ValidateSfiLoadFile(data->fname);

  if (NULL == data->ncf) {
    NaClValidatorMessage(LOG_ERROR, NULL, "nc_loadfile(%s): %s\n",
                         data->fname, strerror(errno));
  }
  return NULL != data->ncf;
}

/* Analyze the code segments of the elf file. */
static Bool ValidateAnalyze(ValidateData *data) {
  int i;
  Bool results = TRUE;
  for (i = 0; i < NACL_FLAGS_validate_attempts; ++i) {
    Bool return_value = AnalyzeSfiCodeSegments(data->ncf, data->fname);
    if (!return_value) {
      results = FALSE;
    }
  }
  nc_freefile(data->ncf);
  return results;
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

static void SfiHexFatal(const char *format, ...) ATTRIBUTE_FORMAT_PRINTF(1, 2);

static void SfiHexFatal(const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  NaClLogV(LOG_ERROR, format, ap);
  va_end(ap);
  /* always succed, so that the testing framework works. */
  exit(0);
}

static int ValidateSfiHexLoad(int argc, const char *argv[],
                              NaClValidatorByteArray *data) {
  if (argc != 1) {
    SfiHexFatal("expected: %s <options>\n", argv[0]);
  }
  if (0 == strcmp(NACL_FLAGS_hex_text, "-")) {
    data->num_bytes = (NaClMemorySize)
        NaClReadHexTextWithPc(stdin, &data->base, data->bytes,
                              NACL_MAX_INPUT_LINE);
  } else {
    FILE *input = fopen(NACL_FLAGS_hex_text, "r");
    if (NULL == input) {
      SfiHexFatal("Can't open hex text file: %s\n", NACL_FLAGS_hex_text);
    }
    data->num_bytes = (NaClMemorySize)
        NaClReadHexTextWithPc(input, &data->base, data->bytes,
                              NACL_MAX_INPUT_LINE);
    fclose(input);
    --argc;
  }
  return argc;
}

/***************************
 * Top-level driver code. *
 **************************/

static void usage() {
  printf(
      "usage: ncval [options] [file]\n"
      "\n"
      "Validates an x86-%d nexe file\n"
      "\n"
      "Options (general):\n"
      "\n"
      "--alignment=N\n"
      "\tSet block alignment to N bytes (only 16 or 32 allowed).\n"
      "--CLFLUSH\n"
      "\tModel a CPU that supports the clflush instruction.\n"
      "--CMOV\n"
      "\tModel a CPU that supports the cmov instructions.\n"
      "--cpuid-all\n"
      "\tModel a CPU that supports all available features.\n"
      "--cpuid-none\n"
      "\tModel a CPU that supports no avaliable features.\n"
      "--CX8\n"
      "\tModel a CPU that supports the cmpxchg8b instruction.\n"
      "--FXSR\n"
      "\tModel a CPU that supports the sfence instructions.\n"
      "--help\n"
      "\tPrint this message, and then exit.\n"
      "--hex_text=<file>\n"
      "\tRead text file of hex bytes, and use that\n"
      "\tas the definition of the code segment. Note: -hex_text=- will\n"
      "\tread from stdin instead of a file.\n"
      "--local_cpuid\n"
      "\tSet cpuid to values defined by local host this command is run on.\n"
      "--LZCNT\n"
      "\tModel a CPU that supports the lzcnt instruction.\n"
      "--MMX\n"
      "\tModel a CPU that supports MMX instructions.\n"
      "--MOVBE\n"
      "\tModel a CPU that supports the movbe instruction.\n"
      "--MSR\n"
      "\tModel a CPU that supports the rdmsr and wrmsr instructions.\n"
      "--POPCNT\n"
      "\tModel a CPU that supports the popcnt instruction.\n"
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
      "--stubout\n"
      "\tRun using stubout mode, replacing bad instructions with halts.\n"
      "\tStubbed out disassembly will be printed after validator\n"
      "\terror messages. Note: Only applied if --hex_text option is\n"
      "\talso specified\n"
      "-t\n"
      "\tTime the validator and print out results.\n"
      "--TSC\n"
      "\tModel a CPU that supports the rdtsc instruction.\n"
      "--use_iter\n"
      "\tUse the iterator model. Currently, the x86-64 bit\n"
      "\tvalidator uses the iterator model while the x86-32 bit model\n"
      "\tdoes not.\n"
      "--x87\n"
      "\tModel a CPU that supports x87 instructions.\n"
      "--VMX\n"
      "\tModel a CPU that supports VMX instructions.\n"
      "--3DNOW\n"
      "\tModel a CPU that supports 3DNOW instructions.\n"
      "--E3DNOW\n"
      "\tModel a CPU that supports E3DNOW instructions.\n"
      "\n"
      "Options (iterator model):\n"
      "--annotate\n"
      "\tRun validator using annotations that will be understood\n"
      "\tby ncval_annotate.py.\n"
      "--detailed\n"
      "\tPrint out detailed error messages, rather than use performant\n"
      "\tcode used by sel_ldr\n"
      "--errors\n"
      "\tPrint out error and fatal error messages, but not\n"
      "\tinformative and warning messages\n"
      "--fatal\n"
      "\tOnly print out fatal error messages.\n"
      "--histogram\n"
      "\tPrint out a histogram of found opcodes.\n"
      "--identity_mask\n"
      "\tMask jumps using 0xFF instead of one matching\n"
      "\tthe block alignment.\n"
      "--max_errors=N\n"
      "\tPrint out at most N error messages. If N is zero,\n"
      "\tno messages are printed. If N is < 0, all messages are printed.\n"
      "--readwrite_sfi\n"
      "\tCheck for memory read and write software fault isolation.\n"
      "--segments\n"
      "\tAnalyze code in segments in elf file, instead of headers.\n"
      "--SVM\n"
      "\tModel a the CPU supports SVM instrutions.\n"
      "--time\n"
      "\tTime the validator and print out results. Same as option -t.\n"
      "--trace_insts\n"
      "\tTurn on validator trace of instructions, as processed..\n"
      "--trace_verbose\n"
      "\tTurn on all trace validator messages. Note: this\n"
      "\tflag also implies --trace.\n"
      "--warnings\n"
      "\tPrint out warnings, errors, and fatal errors, but not\n"
      "\tinformative messages.\n"
      "--write_sfi\n"
      "\tOnly check for memory write software fault isolation.\n"
      "--attempts=N\n"
      "\tRun the validator on the nexe file (after loading) N times.\n"
      "\tNote: this flag should only be used for profiling.\n"
      "\n"
      "Options (callback model):\n"
      "--stats\n"
      "\tPrint statistics collected by the validator.\n",
      NACL_TARGET_SUBARCH);
  exit(0);
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
    { "--detailed", &NACL_FLAGS_detailed_errors },
    { "--stubout", &NACL_FLAGS_stubout_memory },
    { "--trace_insts", &NACL_FLAGS_validator_trace_instructions },
    { "-t", &NACL_FLAGS_print_timing },
    { "--use_iter", &NACL_FLAGS_use_iter },
    { "--stats", &NACL_FLAGS_stats_print },
    { "--annotate", &NACL_FLAGS_ncval_annotate },
    { "--x87"    , &ncval_cpu_features.f_x87 },
    { "--MMX"    , &ncval_cpu_features.f_MMX },
    { "--SSE"    , &ncval_cpu_features.f_SSE },
    { "--SSE2"   , &ncval_cpu_features.f_SSE2 },
    { "--SSE3"   , &ncval_cpu_features.f_SSE3 },
    { "--SSSE3"  , &ncval_cpu_features.f_SSSE3 },
    { "--SSE41"  , &ncval_cpu_features.f_SSE41 },
    { "--SSE42"  , &ncval_cpu_features.f_SSE42 },
    { "--MOVBE"  , &ncval_cpu_features.f_MOVBE },
    { "--POPCNT" , &ncval_cpu_features.f_POPCNT },
    { "--CX8"    , &ncval_cpu_features.f_CX8 },
    { "--CX16"   , &ncval_cpu_features.f_CX16 },
    { "--CMOV"   , &ncval_cpu_features.f_CMOV },
    { "--MON"    , &ncval_cpu_features.f_MON },
    { "--FXSR"   , &ncval_cpu_features.f_FXSR },
    { "--CLFLUSH", &ncval_cpu_features.f_CLFLUSH },
    { "--TSC"    , &ncval_cpu_features.f_TSC },
    { "--MSR"    , &ncval_cpu_features.f_MSR },
    { "--VME"    , &ncval_cpu_features.f_VME },
    { "--PSN"    , &ncval_cpu_features.f_PSN },
    { "--VMX"    , &ncval_cpu_features.f_VMX },
    { "--3DNOW"  , &ncval_cpu_features.f_3DNOW },
    { "--EMMX"   , &ncval_cpu_features.f_EMMX },
    { "--E3DNOW" , &ncval_cpu_features.f_E3DNOW },
    { "--LZCNT"  , &ncval_cpu_features.f_LZCNT },
    { "--SSE4A"  , &ncval_cpu_features.f_SSE4A },
    { "--LM"     , &ncval_cpu_features.f_LM },
    { "--SVM"    , &ncval_cpu_features.f_SVM },
  };
  int i;
  for (i = 0; i < NACL_ARRAY_SIZE(flags); ++i) {
    if (GrokBoolFlag(flags[i].flag_name, arg, flags[i].flag_ptr)) {
      return TRUE;
    }
  }
  return FALSE;
}

static int GrokFlags(int argc, const char *argv[]) {
  int i;
  int new_argc;
  Bool help = FALSE;
  Bool write_sandbox = !NACL_FLAGS_read_sandbox;
  if (argc == 0) return 0;
  new_argc = 1;
  for (i = 1; i < argc; ++i) {
    Bool flag;
    const char *arg = argv[i];
    if (GrokABoolFlag(arg) ||
        GrokCstringFlag("--hex_text", arg, &NACL_FLAGS_hex_text) ||
        GrokIntFlag("--alignment", arg, &NACL_FLAGS_block_alignment) ||
        GrokIntFlag("--max_errors", arg, &NACL_FLAGS_max_reported_errors) ||
        GrokIntFlag("--attempts", arg, &NACL_FLAGS_validate_attempts)) {
      /* Valid processed flag, continue to next flag. */
    } else if (GrokBoolFlag("--help", arg, &help)) {
        usage();
    } else if (0 == strcmp("--trace_verbose", arg)) {
      NaClValidatorFlagsSetTraceVerbose();
    } else if (GrokBoolFlag("--write_sfi", arg, &write_sandbox)) {
      NACL_FLAGS_read_sandbox = !write_sandbox;
    } else if (GrokBoolFlag("--readwrite_sfi", arg, &NACL_FLAGS_read_sandbox)) {
      write_sandbox = !NACL_FLAGS_read_sandbox;
      continue;
    } else if (0 == strcmp("--cpuid-all", arg)) {
      NaClSetAllCPUFeatures(&ncval_cpu_features);
    } else if (0 == strcmp("--cpuid-none", arg)) {
      NaClClearCPUFeatures(&ncval_cpu_features);
    } else if (GrokBoolFlag("--local_cpuid", arg, &flag)) {
      NaClCPUData data;
      NaClCPUDataGet(&data);
      GetCPUFeatures(&data, &ncval_cpu_features);
    } else {
      argv[new_argc++] = argv[i];
    }
  }
  return new_argc;
}

int main(int argc, const char *argv[]) {
  int result = 0;
  struct GioFile gio_out_stream;
  struct Gio *gout = (struct Gio*) &gio_out_stream;

  if (argc < 2) {
    fprintf(stderr, "expected: %s file\n", argv[0]);
  }

  if (!GioFileRefCtor(&gio_out_stream, stdout)) {
    fprintf(stderr, "Unable to create gio file for stdout!\n");
    return 1;
  }
  NaClLogModuleInitExtended(LOG_INFO, gout);
  /* By default, assume no local cpu features are available. */
  NaClClearCPUFeatures(&ncval_cpu_features);

  argc = GrokFlags(argc, argv);
  NCValidatorSetMaxDiagnostics(NACL_FLAGS_max_reported_errors);

  NaClLogDisableTimestamp();

  if (NACL_FLAGS_use_iter) {
    Bool success = FALSE;
    NaClValidateSetCPUFeatures(&ncval_cpu_features);
    argc = NaClRunValidatorGrokFlags(argc, argv);
    if (0 == strcmp(NACL_FLAGS_hex_text, "")) {
      /* Run SFI validator on elf file. */
      ValidateData data;
      success = NaClRunValidator(argc, argv, &data,
                                (NaClValidateLoad) ValidateSfiLoad,
                                (NaClValidateAnalyze) ValidateAnalyze);
      NaClReportSafety(success);
      result = (success ? 0 : 1);
    } else {
      NaClValidatorByteArray data;
      int i;
      success = TRUE;
      argc = ValidateSfiHexLoad(argc, argv, &data);
      for (i = 0; i < NACL_FLAGS_validate_attempts; ++i) {
        if (!NaClRunValidatorBytes(
               argc, argv, (uint8_t*) &data.bytes,
               data.num_bytes, data.base)) {
          success = FALSE;
        }
      }
      NaClReportSafety(success);
      NaClMaybeDecodeDataSegment(&data.bytes[0], data.base, data.num_bytes);
      /* always succeed, so that the testing framework works. */
      result = 0;
    }
  } else if (64 == NACL_TARGET_SUBARCH) {
    SegmentFatal("Can only run %s using -use_iter=false flag on 64 bit code\n",
                 argv[0]);
  } else if (0 == strcmp(NACL_FLAGS_hex_text, "")) {
      int i;
    for (i=1; i< argc; i++) {
      /* Run x86-32 segment based validator. */
      clock_t clock_0;
      clock_t clock_l;
      clock_t clock_v;
      ncfile *ncf;

      clock_0 = clock();
      ncf = nc_loadfile(argv[i]);
      if (ncf == NULL) {
        SegmentFatal("nc_loadfile(%s): %s\n", argv[i], strerror(errno));
      }

      clock_l = clock();
      if (AnalyzeSegmentCodeSegments(ncf, argv[i]) != 0) {
        result = 1;
      }
      clock_v = clock();

      if (NACL_FLAGS_print_timing) {
        SegmentInfo("%s: load time: %0.6f  analysis time: %0.6f\n",
                    argv[i],
                    (double)(clock_l - clock_0) /  (double)CLOCKS_PER_SEC,
                    (double)(clock_v - clock_l) /  (double)CLOCKS_PER_SEC);
      }

      nc_freefile(ncf);
    }
  } else {
    NaClValidatorByteArray data;
    struct NCValidatorState *vstate;
    NCValidateSetCPUFeatures(&ncval_cpu_features);
    argc = ValidateSfiHexLoad(argc, argv, &data);
    vstate = NCValInit(data.base, data.base + data.num_bytes,
                       (uint8_t) NACL_FLAGS_block_alignment);
    if (vstate == NULL) {
      printf("Unable to create validator state, quitting!\n");
      result = 1;
    } else {
      NCValidateSetErrorReporter(vstate, &kNCVerboseErrorReporter);
      if (NACL_FLAGS_stubout_memory) {
        NCValidateSetStubOutMode(vstate, 1);
      }
      NCValidateSegment(&data.bytes[0], data.base, data.num_bytes, vstate);
      NCValidateResults(NCValidateFinish(vstate),
                        ((strcmp(NACL_FLAGS_hex_text, "-") == 0)
                         ? "<hex_text>" : NACL_FLAGS_hex_text));
      if (NACL_FLAGS_stats_print) NCStatsPrint(vstate);
      NCValidateFreeState(&vstate);
      NaClMaybeDecodeDataSegment(&data.bytes[0], data.base, data.num_bytes);
      /* always succeed, so that the testing framework works. */
      result = 0;
    }
  }
  NaClLogModuleFini();
  GioFileDtor(gout);
  return result;
}
