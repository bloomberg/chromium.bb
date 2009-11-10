/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#include "native_client/src/trusted/validator_x86/ncfileutil.h"
#include "native_client/src/trusted/validator_x86/ncvalidate.h"
#include "native_client/src/trusted/validator_x86/ncdecode.h"


void Info(const char *fmt, ...) {
  va_list ap;
  fprintf(stdout, "I: ");
  va_start(ap, fmt);
  vfprintf(stdout, fmt, ap);
  va_end(ap);
}


void Debug(const char *fmt, ...) {
  va_list ap;
  fprintf(stdout, "D: ");
  va_start(ap, fmt);
  vfprintf(stdout, fmt, ap);
  va_end(ap);
}


void Fatal(const char *fmt, ...) {
  va_list ap;
  fprintf(stdout, "Fatal error: ");
  va_start(ap, fmt);
  vfprintf(stdout, fmt, ap);
  va_end(ap);
  exit(2);
}


int AnalyzeSections(ncfile *ncf, struct NCValidatorState *vstate) {
  int badsections = 0;
  int ii;
  const Elf_Phdr* phdr = ncf->pheaders;

  for (ii = 0; ii < ncf->phnum; ii++) {
    Debug("segment[%d] p_type %d p_offset %x vaddr %x paddr %x align %u\n",
          ii, phdr[ii].p_type, (uint32_t)phdr[ii].p_offset,
          (uint32_t)phdr[ii].p_vaddr, (uint32_t)phdr[ii].p_paddr,
          (uint32_t)phdr[ii].p_align);

    Debug("    filesz %x memsz %x flags %x\n",
          phdr[ii].p_filesz, (uint32_t)phdr[ii].p_memsz,
          (uint32_t)phdr[ii].p_flags);
    if ((PT_LOAD != phdr[ii].p_type) ||
        (0 == (phdr[ii].p_flags & PF_X)))
      continue;
    Debug("parsing segment %d\n", ii);
    /* note we use NCDecodeSegment instead of NCValidateSegment */
    /* because we don't want the check for a hlt at the end of  */
    /* the text segment as required by NaCl.                    */
    NCDecodeSegment(ncf->data + (phdr[ii].p_vaddr - ncf->vbase),
                    phdr[ii].p_vaddr, phdr[ii].p_memsz, vstate);
  }
  return -badsections;
}


static int AnalyzeCodeSegments(ncfile *ncf, const char *fname) {
  PcAddress vbase, vlimit;
  struct NCValidatorState *vstate;
  int result;

  GetVBaseAndLimit(ncf, &vbase, &vlimit);
  vstate = NCValidateInit(vbase, vlimit, ncf->ncalign);
  if (AnalyzeSections(ncf, vstate) < 0) {
    Info("%s: text validate failed\n", fname);
  }
  result = NCValidateFinish(vstate);
  if (result != 0) {
    Debug("***MODULE %s IS UNSAFE***\n", fname);
  } else {
    Debug("***module %s is safe***\n", fname);
  }
  Stats_Print(stdout, vstate);
  NCValidateFreeState(&vstate);
  Debug("Validated %s\n", fname);
  return result;
}

/* make output deterministic */
static int GlobalPrintTiming = 0;


int main(int argc, const char *argv[]) {
  int result = 0;
  int i;
  for (i=1; i< argc; i++) {
    clock_t clock_0;
    clock_t clock_l;
    clock_t clock_v;
    ncfile *ncf;
    if (0 == strcmp("-t", argv[i])) {
      GlobalPrintTiming = 1;
      continue;
    }

    clock_0 = clock();
    ncf = nc_loadfile(argv[i]);
    if (ncf == NULL) {
      Fatal("nc_loadfile(%s): %s\n", argv[i], strerror(errno));
    }

    clock_l = clock();
    if (AnalyzeCodeSegments(ncf, argv[i]) != 0) {
      result = 1;
    }
    clock_v = clock();

    if (GlobalPrintTiming) {
      Info("%s: load time: %0.6f  analysis time: %0.6f\n",
           argv[i],
           (double)(clock_l - clock_0) /  (double)CLOCKS_PER_SEC,
           (double)(clock_v - clock_l) /  (double)CLOCKS_PER_SEC);
    }

    nc_freefile(ncf);
  }
  return result;
}
