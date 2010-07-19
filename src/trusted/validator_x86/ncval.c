/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * ncval.c - command line validator for NaCl.
 * Mostly for testing.
 */
#include "native_client/src/include/portability.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/timeb.h>
#include <time.h>
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/utils/flags.h"
#include "native_client/src/trusted/validator_x86/ncdecode.h"
#include "native_client/src/trusted/validator_x86/ncfileutil.h"
#include "native_client/src/trusted/validator_x86/nc_memory_protect.h"
#include "native_client/src/trusted/validator_x86/nc_read_segment.h"
#include "native_client/src/trusted/validator_x86/ncvalidate.h"
#include "native_client/src/trusted/validator_x86/ncval_driver.h"
#include "native_client/src/trusted/validator_x86/ncvalidate_iter.h"
#include "native_client/src/trusted/validator_x86/ncvalidator_registry.h"

/* Print time usage information. */
static Bool FLAGS_print_timing = FALSE;

/* Flag defining which validator model to use. */
static Bool FLAGS_use_iter = (64 == NACL_TARGET_SUBARCH);

/* Flag defining the name of a hex text to be used as the code segment. Assumes
 * that the pc associated with the code segment is defined by
 * FLAGS_decode_pc.
 */
static char* FLAGS_hex_text = "";

/* Define if we should process segments (rather than sections) when applying SFI
 * validator.
 */
static Bool FLAGS_analyze_segments = FALSE;

/***************************************
 * Code to run segment based validator.*
 ***************************************/

void SegmentInfo(const char *fmt, ...) {
  va_list ap;
  fprintf(stdout, "I: ");
  va_start(ap, fmt);
  vfprintf(stdout, fmt, ap);
  va_end(ap);
}

void SegmentDebug(const char *fmt, ...) {
  va_list ap;
  fprintf(stdout, "D: ");
  va_start(ap, fmt);
  vfprintf(stdout, fmt, ap);
  va_end(ap);
}


void SegmentFatal(const char *fmt, ...) {
  va_list ap;
  fprintf(stdout, "Fatal error: ");
  va_start(ap, fmt);
  vfprintf(stdout, fmt, ap);
  va_end(ap);
  exit(2);
}

int AnalyzeSegmentSections(ncfile *ncf, struct NCValidatorState *vstate) {
  int badsections = 0;
  int ii;
  const Elf_Phdr* phdr = ncf->pheaders;

  for (ii = 0; ii < ncf->phnum; ii++) {
    SegmentDebug(
        "segment[%d] p_type %d p_offset %x vaddr %x paddr %x align %u\n",
        ii, phdr[ii].p_type, (uint32_t)phdr[ii].p_offset,
        (uint32_t)phdr[ii].p_vaddr, (uint32_t)phdr[ii].p_paddr,
        (uint32_t)phdr[ii].p_align);

    SegmentDebug("    filesz %x memsz %x flags %x\n",
          phdr[ii].p_filesz, (uint32_t)phdr[ii].p_memsz,
          (uint32_t)phdr[ii].p_flags);
    if ((PT_LOAD != phdr[ii].p_type) ||
        (0 == (phdr[ii].p_flags & PF_X)))
      continue;
    SegmentDebug("parsing segment %d\n", ii);
    /*
     * This used NCDecodeSegment() instead of NCValidateSegment()
     * because the latter required a HLT at the end of the code, but
     * this requirement has now gone.
     * TODO(mseaborn): Could use NCValidateSegment() now.
     */
    NCDecodeSegment(ncf->data + (phdr[ii].p_vaddr - ncf->vbase),
                    phdr[ii].p_vaddr, phdr[ii].p_memsz, vstate);
  }
  return -badsections;
}

static int AnalyzeSegmentCodeSegments(ncfile *ncf, const char *fname) {
  NaClPcAddress vbase, vlimit;
  struct NCValidatorState *vstate;
  int result;

  GetVBaseAndLimit(ncf, &vbase, &vlimit);
  vstate = NCValidateInit(vbase, vlimit, ncf->ncalign);
  if (AnalyzeSegmentSections(ncf, vstate) < 0) {
    SegmentInfo("%s: text validate failed\n", fname);
  }
  result = NCValidateFinish(vstate);
  if (result != 0) {
    SegmentDebug("***MODULE %s IS UNSAFE***\n", fname);
  } else {
    SegmentDebug("***module %s is safe***\n", fname);
  }
  Stats_Print(stdout, vstate);
  NCValidateFreeState(&vstate);
  SegmentDebug("Validated %s\n", fname);
  return result;
}

/******************************
 * Code to run SFI validator. *
 ******************************/

/* Analyze each section in the given elf file, using the given validator
 * state.
 */
static void AnalyzeSfiSections(ncfile *ncf, struct NaClValidatorState *vstate) {
  int ii;
  const Elf_Phdr* phdr = ncf->pheaders;

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

static void AnalyzeSfiSegments(ncfile* ncf, NaClValidatorState* state) {
  int ii;
  const Elf_Shdr* shdr = ncf->sheaders;

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
                                    ncf->ncalign, nacl_base_register,
                                    NACL_FLAGS_quit_on_error,
                                    stderr);
  if (vstate == NULL) {
    NaClValidatorMessage(LOG_FATAL, vstate, "Unable to create validator state");
  }
  if (FLAGS_analyze_segments) {
    AnalyzeSfiSegments(ncf, vstate);
  } else {
    AnalyzeSfiSections(ncf, vstate);
  }
  NaClValidatorStatePrintStats(stdout, vstate);
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
  const char* fname;
  /* The elf file to validate. */
  ncfile* ncf;
} ValidateData;

/* Prints out appropriate error message during call to ValidateSfiLoad. */
static void ValidateSfiLoadPrintError(const char* format,
                                   ...) ATTRIBUTE_FORMAT_PRINTF(1,2);

static void ValidateSfiLoadPrintError(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  NaClValidatorVarargMessage(LOG_ERROR, NULL, format, ap);
  va_end(ap);
}

/* Loads the elf file defined by fname. For use within ValidateSfiLoad. */
static ncfile* ValidateSfiLoadFile(const char* fname) {
  nc_loadfile_error_fn old_print_error;
  ncfile* file;
  old_print_error = NcLoadFileRegisterErrorFn(ValidateSfiLoadPrintError);
  file = nc_loadfile(fname);
  NcLoadFileRegisterErrorFn(old_print_error);
  return file;
}

/* Load the elf file and return the loaded elf file. */
static Bool ValidateSfiLoad(int argc, const char* argv[], ValidateData* data) {
  if (argc != 2) {
    NaClValidatorMessage(LOG_FATAL, NULL, "expected: %s file\n", argv[0]);
  }
  data->fname = argv[1];

  /* TODO(karl): Once we fix elf values put in by compiler, so that
   * we no longer get load errors from ncfilutil.c, find a way to
   * terminate early if errors occur during loading.
   */
  data->ncf = ValidateSfiLoadFile(data->fname);

  if (NULL == data->ncf) {
    NaClValidatorMessage(LOG_FATAL, NULL, "nc_loadfile(%s): %s\n",
                         data->fname, strerror(errno));
  }
  return NULL != data->ncf;
}

/* Analyze the code segments of the elf file. */
static Bool ValidateAnalyze(ValidateData* data) {
  Bool return_value = AnalyzeSfiCodeSegments(data->ncf, data->fname);
  nc_freefile(data->ncf);
  return return_value;
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

static struct GioFile gio_err_stream;

static void SfiHexFatal(const char* format, ...) ATTRIBUTE_FORMAT_PRINTF(1, 2);

static void SfiHexFatal(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  gprintf((struct Gio*) &gio_err_stream, "Fatal: ");
  gvprintf((struct Gio*) &gio_err_stream, format, ap);
  va_end(ap);
  /* always succed, so that the testing framework works. */
  exit(0);
}

static int ValidateSfiHexLoad(int argc, const char* argv[],
                              NaClValidatorByteArray* data) {
  if (argc != 1) {
    SfiHexFatal("expected: %s <options>\n", argv[0]);
  }
  if (0 == strcmp(FLAGS_hex_text, "-")) {
    data->num_bytes = (NaClMemorySize)
        NaClReadHexTextWithPc(stdin, &data->base, data->bytes,
                              NACL_MAX_INPUT_LINE);
  } else {
    FILE* input = fopen(FLAGS_hex_text, "r");
    if (NULL == input) {
      SfiHexFatal("Can't open hex text file: %s\n", FLAGS_hex_text);
    }
    data->num_bytes = (NaClMemorySize)
        NaClReadHexTextWithPc(input, &data->base, data->bytes,
                              NACL_MAX_INPUT_LINE);
    fclose(input);
  }
  return argc;
}

/***************************
 * Top-level driver code. *
 **************************/

static int GrokFlags(int argc, const char* argv[]) {
  int i;
  int new_argc;
  Bool write_sandbox = !NACL_FLAGS_read_sandbox;
  if (argc == 0) return 0;
  new_argc = 1;
  for (i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    if (GrokCstringFlag("-hex_text", arg, &FLAGS_hex_text) ||
        GrokBoolFlag("-segments", arg, &FLAGS_analyze_segments) ||
        GrokBoolFlag("-trace", arg, &NACL_FLAGS_validator_trace) ||
        GrokBoolFlag("-trace_verbose",
                     arg, &NACL_FLAGS_validator_trace_verbose) ||
        GrokBoolFlag("-use_iter", arg, &FLAGS_use_iter) ||
        GrokBoolFlag("-t", arg, &FLAGS_print_timing) ||
        GrokBoolFlag("-use_iter", arg, &FLAGS_use_iter)) {
      continue;
    } else if (GrokBoolFlag("-write_sfi", arg, &write_sandbox)) {
      NACL_FLAGS_read_sandbox = !write_sandbox;
      continue;
    } else if (GrokBoolFlag("-readwrite_sfi", arg, &NACL_FLAGS_read_sandbox)) {
      write_sandbox = !NACL_FLAGS_read_sandbox;
      continue;
    } else {
      argv[new_argc++] = argv[i];
    }
  }
  return new_argc;
}

int main(int argc, const char *argv[]) {
  int result = 0;
  int i;
  argc = GrokFlags(argc, argv);
  if (FLAGS_use_iter) {
    argc = NaClRunValidatorGrokFlags(argc, argv);
    if (0 == strcmp(FLAGS_hex_text, "")) {
      /* Run SFI validator on elf file. */
      ValidateData data;
      return NaClRunValidator(argc, argv, &data,
                              (NaClValidateLoad) ValidateSfiLoad,
                              (NaClValidateAnalyze) ValidateAnalyze)
          ? 0 : 1;
    } else {
      NaClValidatorByteArray data;
      GioFileRefCtor(&gio_err_stream, stdout);
      NaClLogDisableTimestamp();
      argc = ValidateSfiHexLoad(argc, argv, &data);
      if (NaClRunValidatorBytes(argc, argv, (uint8_t*) &data.bytes,
                                data.num_bytes,
                                data.base)) {
        printf("***module is safe***\n");
      } else {
        printf("***MODULE IS UNSAFE***\n");
      }
      /* always succeed, so that the testing framework works. */
      return 0;
    }
  } else if (64 == NACL_TARGET_SUBARCH) {
    SegmentFatal("Can only run %s using -use_iter flag on 64 bit code\n");
  } else {
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

      if (FLAGS_print_timing) {
        SegmentInfo("%s: load time: %0.6f  analysis time: %0.6f\n",
                    argv[i],
                    (double)(clock_l - clock_0) /  (double)CLOCKS_PER_SEC,
                    (double)(clock_v - clock_l) /  (double)CLOCKS_PER_SEC);
      }

      nc_freefile(ncf);
    }
  }
  return result;
}
