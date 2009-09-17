/*
 * Copyright 2009, Google Inc.
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

#ifndef SERVICE_RUNTIME_NACL_THREAD_H__
#define SERVICE_RUNTIME_NACL_THREAD_H__ 1

/*
 * This header contains the prototypes for thread/tls related
 * functions whose implementation is highly architecture/platform
 * specific.
 * The function primarily adress complications stemming from the
 * x86 segmentation model - on arm things are somewhat simpler.
 */
#include "native_client/src/include/portability.h"

struct NaClAppThread;

int NaClTlsInit();

void NaClTlsFini();


/* Allocate a new tls descriptor and return it.
 * On x86 this the value for the gs segment register on arm
 * it is the value we keep in r9.
 * This is called for the main thread and all subsequent threads
 * being created via NaClAppThreadAllocSegCtor()
 */
uint32_t NaClTlsAllocate(struct NaClAppThread *natp,
                         void *tdb,
                         uint32_t size) NACL_WUR;

/* Free a tls descriptor (almost a nop on ARM).
 * This is called from NaClAppThreadDtor which in turn is called
 * after a thread terminates.
 */
void NaClTlsFree(struct NaClAppThread *natp);


/* This is only called from the sycall TlsInit which is typically
 * call at module startup time.
 * It installs tls descriptor for the main thread and also returns it.
 */
uint32_t NaClTlsChange(struct NaClAppThread *natp,
                       void *tdb,
                       uint32_t size) NACL_WUR;

/* Get the current thread index which is used to look up information in a
 * number of internal structures, e.g.nacl_thread[], nacl_user[], nacl_sys[]
 */
uint32_t NaClGetThreadIdx(struct NaClAppThread *natp);

#endif /* SERVICE_RUNTIME_NACL_THREAD_H__ */
