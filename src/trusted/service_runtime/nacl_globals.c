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
 * NaCl Server Runtime global scoped objects for handling global resources.
 */

#include <stdio.h>
#include <stdlib.h>

#include "native_client/src/include/portability.h"

#include "native_client/src/trusted/platform/nacl_log.h"
#include "native_client/src/trusted/platform/nacl_secure_random.h"
#include "native_client/src/trusted/platform/nacl_threads.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
/* LDT_ENTRIES */
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/sel_rt.h"


struct NaClThreadContext    *nacl_user[LDT_ENTRIES] = {NULL};
struct NaClThreadContext    *nacl_sys[LDT_ENTRIES] = {NULL};

struct NaClMutex            nacl_thread_mu = {NULL};
struct NaClAppThread        *nacl_thread[LDT_ENTRIES] = {NULL};

/* TODO(petr): these variables architecture specific, needs to be noved */
uint16_t                    nacl_global_cs = 0;
uint16_t                    nacl_global_ds = 0;

struct NaClTsdKey           nacl_cur_thread_key;

/*
 * Hack for gdb.  This records xlate_base in a place where (1) gdb can find it,
 * and (2) gdb doesn't need debug info (it just needs symbol info).
 */
uintptr_t                   nacl_global_xlate_base;

/* TODO(petr): this function architecture specific, needs to be noved */
void NaClGlobalModuleInit(void) {
  NaClMutexCtor(&nacl_thread_mu);
#if !NACL_ARM
  nacl_global_cs = NaClGetCs();
  nacl_global_ds = NaClGetDs();
#endif
  /* key for TSD */
  if (!NaClTsdKeyCreate(&nacl_cur_thread_key)) {
    NaClLog(LOG_FATAL,
            "Could not create thread specific data key for cur_thread\n");
  }
}


void  NaClGlobalModuleFini(void) {
  NaClMutexDtor(&nacl_thread_mu);
}


struct NaClAppThread  *GetCurThread(void) {
  return NaClTsdGetSpecific(&nacl_cur_thread_key);
}


struct NaClApp *GetCurProc(void) {
  struct NaClAppThread *natp;

  natp = GetCurThread();
  if (NULL == natp) {
    /*
     * This should never happen; if we are so lost as to not be able
     * to figure out the current thread, we probably cannot log....
     */
    NaClLog(LOG_FATAL,
            "The puce potion hits me and shatters.  Which thread am I?!?\n");
    return (struct NaClApp *) NULL;
  }
  return natp->nap;
}
