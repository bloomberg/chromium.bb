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
 * NaCl Simple/secure ELF loader (NaCl SEL) memory object.
 */

#include <stdlib.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/trusted/service_runtime/nacl_memory_object.h"


/*
 * Takes ownership of NaClDesc object, so no manipulation of ref count.
 */
int NaClMemObjCtor(struct NaClMemObj  *nmop,
                   struct NaClDesc    *ndp,
                   off_t              nbytes,
                   off_t              offset) {
  if (NULL == ndp) {
    NaClLog(LOG_FATAL, "NaClMemObjCtor: ndp is NULL\n");
  }
  nmop->ndp = ndp;
  NaClDescRef(ndp);
  nmop->nbytes = nbytes;
  nmop->offset = offset;
  return 1;
}


/*
 * Placement new copy ctor.
 */
int NaClMemObjCopyCtorOff(struct NaClMemObj *nmop,
                          struct NaClMemObj *src,
                          off_t             additional) {
  nmop->ndp = src->ndp;
  NaClDescRef(nmop->ndp);
  nmop->nbytes = src->nbytes;
  nmop->offset = src->offset + additional;
  return 1;
}


void NaClMemObjDtor(struct NaClMemObj *nmop) {
  NaClDescUnref(nmop->ndp);
  nmop->ndp = NULL;
  nmop->nbytes = 0;
  nmop->offset = 0;
}


struct NaClMemObj *NaClMemObjMake(struct NaClDesc *ndp,
                                  off_t           nbytes,
                                  off_t           offset) {
  struct NaClMemObj *nmop;

  if (NULL == ndp) {
    NaClLog(LOG_WARNING, "NaClMemObjMake: invoked with NULL ndp\n");
    return NULL;  /* anonymous paging file backed memory */
  }
  if (NULL == (nmop = malloc(sizeof *nmop))) {
    NaClLog(LOG_FATAL, ("NaClMemObjMake: out of memory creating object"
                        " (NaClDesc = 0x%08"PRIxPTR", offset = 0x%"PRIx64")\n"),
            (uintptr_t) ndp, (int64_t) offset);
  }
  if (!NaClMemObjCtor(nmop, ndp, nbytes, offset)) {
    NaClLog(LOG_FATAL, "NaClMemObjMake: NaClMemObjCtor failed!\n");
  }
  return nmop;
}


struct NaClMemObj *NaClMemObjSplit(struct NaClMemObj *orig,
                                   off_t             additional) {
  struct NaClMemObj *nmop;

  if (NULL == orig)
    return NULL;

  if (NULL == (nmop = malloc(sizeof *nmop))) {
    NaClLog(LOG_FATAL, ("NaClMemObjSplit: out of memory creating object"
                        " (NaClMemObj = 0x%08"PRIxPTR","
                        " additional = 0x%"PRIx64")\n"),
            (uintptr_t) orig, (int64_t) additional);
  }
  if (!NaClMemObjCopyCtorOff(nmop, orig, additional)) {
    NaClLog(LOG_FATAL, "NaClMemObjSplit: NaClMemObjCopyCtorOff failed\n");
  }
  return nmop;
}


void NaClMemObjIncOffset(struct NaClMemObj *nmop,
                         off_t             additional) {
  if (NULL != nmop) {
    nmop->offset += additional;
  }
}
