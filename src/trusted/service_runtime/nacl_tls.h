/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SERVICE_RUNTIME_NACL_THREAD_H__
#define SERVICE_RUNTIME_NACL_THREAD_H__ 1

/*
 * This header contains the prototypes for thread/tls related
 * functions whose implementation is highly architecture/platform
 * specific.
 *
 * The function primarily adress complications stemming from the
 * x86-32 segmentation model - on arm things are somewhat simpler.
 *
 * On x86-64, no segment is used either.  This API is (ab)used to
 * stash the tdb in a thread-local variable (TLS on NACL_LINUX and
 * NACL_WINDOWS, and TSD on NACL_OSX since OSX does not implement
 * __thread).
 */
#include "native_client/src/include/portability.h"

struct NaClAppThread;

int NaClTlsInit();

void NaClTlsFini();


#define NACL_TLS_INDEX_INVALID 0

/*
 * Allocates a thread index for the thread.  On x86-32, the thread
 * index is the gs segment number.  On x86-64 and ARM, the thread
 * index is used internally in NaCl but has no other meaning.
 * This is called for the main thread and all subsequent threads
 * being created via NaClAppThreadMake().
 * On error, returns NACL_TLS_INDEX_INVALID.
 */
uint32_t NaClTlsAllocate(struct NaClAppThread *natp) NACL_WUR;

/*
 * Free a tls descriptor.
 * This is called from NaClAppThreadDelete() which in turn is called
 * after a thread terminates.
 */
void NaClTlsFree(struct NaClAppThread *natp);


/*
 * Called in thread bootup code, to set TLS/TSD when the thread ID is not
 * saved in a reserved register (e.g., %gs in NaCl x86-32).
 *
 * No-op for x86-32 NaCl classic.
 */
void NaClTlsSetIdx(uint32_t tls_idx);

/*
 * Gets the current thread's index value as set by NaClTlsSetIdx().
 * Only implemented for x86-64 and ARM, since these are the only sandboxes
 * where we use the host OS's TLS facility.
 * Returns NACL_TLS_INDEX_INVALID if NaClTlsSetIdx() has not been
 * called on this thread.
 */
uint32_t NaClTlsGetIdx(void);

void NaClTlsSetTlsValue1(struct NaClAppThread *natp, uint32_t value);
void NaClTlsSetTlsValue2(struct NaClAppThread *natp, uint32_t value);

uint32_t NaClTlsGetTlsValue1(struct NaClAppThread *natp);
uint32_t NaClTlsGetTlsValue2(struct NaClAppThread *natp);

/*
 * Get the current thread index which is used to look up information in a
 * number of internal structures, e.g. nacl_thread[], nacl_user[]
 */
uint32_t NaClGetThreadIdx(struct NaClAppThread *natp);


#if NACL_OSX
/*
 * OSX does not have TLS and we emulate it on the trusted side using TSD.
 */
extern pthread_key_t nacl_thread_info_key;
extern uint32_t nacl_thread_index_tls_offset;

#endif


#endif /* SERVICE_RUNTIME_NACL_THREAD_H__ */
