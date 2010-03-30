/* Copyright (c) 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * ncval_iter.c - command line validator for NaCl.
 */

#include <string.h>
#include <errno.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/utils/flags.h"
#include "native_client/src/trusted/validator_x86/nc_jumps.h"
#include "native_client/src/trusted/validator_x86/nc_protect_base.h"
#include "native_client/src/trusted/validator_x86/ncfileutil.h"
#include "native_client/src/trusted/validator_x86/ncval_driver.h"
#include "native_client/src/trusted/validator_x86/ncvalidate_iter.h"

/* Analyze each section in the given elf file, using the given validator
 * state.
 */
static void AnalyzeSections(ncfile *ncf, struct NaClValidatorState *vstate) {
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

static void AnalyzeSegments(ncfile* ncf, NaClValidatorState* state) {
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

/* Define if we should process segments (rather than sections). */
static Bool FLAGS_analyze_segments = FALSE;

/* Analyze each code segment in the given elf file, stored in the
 * file with the given path fname.
 */
static Bool AnalyzeCodeSegments(ncfile *ncf, const char *fname) {
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
    AnalyzeSegments(ncf, vstate);
  } else {
    AnalyzeSections(ncf, vstate);
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

/* Recognizes flags in argv, processes them, and then removes them.
 * Returns the updated value for argc.
 */
static int GrokFlags(int argc, const char* argv[]) {
  int i;
  int new_argc;
  /* char* error_level = NULL; */
  if (argc == 0) return 0;
  new_argc = 1;
  for (i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    if (GrokBoolFlag("-segments", arg, &FLAGS_analyze_segments)) {
      continue;
    } else {
      argv[new_argc++] = argv[i];
    }
  }
  return new_argc;
}

/* Local data to validator run. */
typedef struct ValidateData {
  /* The name of the elf file to validate. */
  const char* fname;
  /* The elf file to validate. */
  ncfile* ncf;
} ValidateData;

/* Prints out appropriate error message during call to ValidateLoad. */
static void ValidateLoadPrintError(const char* format,
                                   ...) ATTRIBUTE_FORMAT_PRINTF(1,2);

static void ValidateLoadPrintError(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  NaClValidatorVarargMessage(LOG_ERROR, NULL, format, ap);
  va_end(ap);
}

/* Loads the elf file defined by fname. For use within ValidateLoad. */
static ncfile* ValidateLoadFile(const char* fname) {
  nc_loadfile_error_fn old_print_error;
  ncfile* file;
  old_print_error = NcLoadFileRegisterErrorFn(ValidateLoadPrintError);
  file = nc_loadfile(fname);
  NcLoadFileRegisterErrorFn(old_print_error);
  return file;
}

/* Load the elf file and return the loaded elf file. */
static Bool ValidateLoad(int argc, const char* argv[], ValidateData* data) {
  argc = GrokFlags(argc, argv);
  if (argc != 2) {
    NaClValidatorMessage(LOG_FATAL, NULL, "expected: %s file\n", argv[0]);
  }
  data->fname = argv[1];

  /* TODO(karl): Once we fix elf values put in by compiler, so that
   * we no longer get load errors from ncfilutil.c, find a way to
   * terminate early if errors occur during loading.
   */
  data->ncf = ValidateLoadFile(data->fname);

  if (NULL == data->ncf) {
    NaClValidatorMessage(LOG_FATAL, NULL, "nc_loadfile(%s): %s\n",
                         data->fname, strerror(errno));
  }
  return NULL != data->ncf;
}

/* Analyze the code segments of the elf file. */
static Bool ValidateAnalyze(ValidateData* data) {
  Bool return_value = AnalyzeCodeSegments(data->ncf, data->fname);
  nc_freefile(data->ncf);
  free(data);
  return return_value;
}

int main(int argc, const char *argv[]) {
  ValidateData data;
  return NaClRunValidator(argc, argv, &data,
                          (NaClValidateLoad) ValidateLoad,
                          (NaClValidateAnalyze) ValidateAnalyze)
      ? 0 : 1;
}
