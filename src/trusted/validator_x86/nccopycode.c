/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * nccopycode.c
 * Copies two code streams in a thread-safe way
 *
 */

#include "native_client/src/include/portability.h"

#if NACL_WINDOWS == 1
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include "native_client/src/trusted/validator_x86/ncdecode.h"
#include "native_client/src/trusted/validator_x86/ncvalidate.h"
#include "native_client/src/shared/platform/nacl_check.h"

/* x86 HALT opcode */
static const uint8_t kNaClFullStop = 0xf4;

/*
 * Max size of aligned writes we may issue to code without syncing.
 * 8 is a safe value according to:
 *   [1] Advance Micro Devices Inc. AMD64 Architecture Program-
 *   mers Manual Volume 1: Application Programming, 2009.
 *   [2] Intel Inc. Intel 64 and IA-32 Architectures Software Developers
 *   Manual Volume 3A: System Programming Guide, Part 1, 2010.
 *   [3] Vijay Sundaresan, Daryl Maier, Pramod Ramarao, and Mark
 *   Stoodley. Experiences with multi-threading and dynamic class
 *   loading in a java just-in-time compiler. Code Generation and
 *   Optimization, IEEE/ACM International Symposium on, 0:87–
 *   97, 2006.
 */
static const int kTrustAligned = 8;

/*
 * Max size of unaligned writes we may issue to code.
 * Empirically this may be larger, however no docs to support it.
 * 1 means disabled.
 */
static const int kTrustUnaligned = 1;

/*
 * Boundary no write may ever cross.
 * On AMD machines must be leq 8.  Intel machines leq cache line.
 */
static const int kInstructionFetchSize = 8;

/* defined in nccopycode_stores.S */
void _cdecl onestore_memmove4(uint8_t* dst, uint8_t* src);

/* defined in nccopycode_stores.S */
void _cdecl onestore_memmove8(uint8_t* dst, uint8_t* src);

static INLINE int IsAligned(uint8_t *dst, int size, int align) {
  uintptr_t startaligned = ((uintptr_t)dst)        & ~(align-1);
  uintptr_t stopaligned  = ((uintptr_t)dst+size-1) & ~(align-1);
  return startaligned == stopaligned;
}

/*
 * Test if it is safe to issue a unsynced change to dst/size using a
 * writesize write.  Outputs the offset to start the write at.
 * 1 if it is ok, 0 if it is not ok.
 */
static int IsTrustedWrite(uint8_t *dst,
                          int size,
                          int writesize,
                          intptr_t* offset) {
  if (size > writesize) {
    return 0;
  }
  if (!IsAligned(dst, size, kInstructionFetchSize)) {
    return 0;
  }
  if (writesize <= kTrustAligned && IsAligned(dst, size, writesize)) {
    /* aligned write is trusted */
    *offset = (intptr_t) dst & (writesize - 1);
    return 1;
  }
  if (writesize <= kTrustUnaligned) {
    /* unaligned write is trusted */
    *offset = 0;
    return 1;
  }
  return 0;
}

#if NACL_WINDOWS == 1
static void* valloc(size_t s) {
  /* allocate twice as much then round up to nearest s */
  uintptr_t m = (uintptr_t) malloc(2 * s);
  /* check for power of 2: */
  CHECK(0 == (s & (s - 1)));
  m = (m + s) & ~(s - 1);
  return (void*) m;
}
#endif

/* this is global to prevent a (very smart) compiler from optimizing it out */
void* g_squashybuffer = NULL;

static void SerializeAllProcessors() {
  /*
   * We rely on the OS mprotect() call to issue interprocessor interrupts,
   * which will cause other processors to execute an IRET, which is
   * serializing.
   */
#if NACL_WINDOWS == 1
  static const DWORD prot_a = PAGE_EXECUTE_READWRITE;
  static const DWORD prot_b = PAGE_NOACCESS;
  static DWORD prot = PAGE_NOACCESS;
  static DWORD size = 0;
  BOOL rv;
  DWORD oldprot;
  if (NULL == g_squashybuffer) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    size = si.dwPageSize;
    g_squashybuffer = valloc(size);
  }
  CHECK(0 != size);
  CHECK(NULL != g_squashybuffer);
  prot = (prot == prot_a ? prot_b : prot_a);
  rv = VirtualProtect(g_squashybuffer, size, prot, &oldprot);
  CHECK(rv);
#else
  static const int prot_a = PROT_READ|PROT_WRITE|PROT_EXEC;
  static const int prot_b = PROT_NONE;
  static int prot = PROT_NONE;
  static int size = 0;
  int rv;
  if (NULL == g_squashybuffer) {
    size = sysconf(_SC_PAGE_SIZE);
    g_squashybuffer = valloc(size);
  }
  CHECK(0 != size);
  CHECK(NULL != g_squashybuffer);
  prot = (prot == prot_a ? prot_b : prot_a);
  rv = mprotect(g_squashybuffer, size, prot);
  CHECK(rv == 0);
#endif
}

/*
 * Copy a single instruction, avoiding the possibility of other threads
 * executing a partially changed instruction.
 */
void CopyInstruction(const struct NCDecoderState *mstate_old,
                     const struct NCDecoderState *mstate_new) {
  uint8_t* dst = mstate_old->inst.maddr;
  uint8_t* src = mstate_new->inst.maddr;
  int sz = mstate_old->inst.length;
  intptr_t offset = 0;
  CHECK(mstate_new->inst.length == mstate_old->inst.length);

  while (sz > 0 && dst[0] == src[0]) {
    /* scroll to first changed byte */
    dst++, src++, sz--;
  }

  if (sz == 0) {
    /* instructions are identical, we are done */
    return;
  }

  while (sz > 0 && dst[sz-1] == src[sz-1]) {
    /* trim identical bytes at end */
    sz--;
  }

  if (sz == 1) {
    /* we assume a 1-byte change it atomic */
    *dst = *src;
  } else if (IsTrustedWrite(dst, sz, 4, &offset)) {
    uint8_t tmp[4];
    memcpy(tmp, dst-offset, sizeof tmp);
    memcpy(tmp+offset, src, sz);
    onestore_memmove4(dst-offset, tmp);
  } else if (IsTrustedWrite(dst, sz, 8, &offset)) {
    uint8_t tmp[8];
    memcpy(tmp, dst-offset, sizeof tmp);
    memcpy(tmp+offset, src, sz);
    onestore_memmove8(dst-offset, tmp);
  } else {
    /* the slow path, first flip first byte to halt*/
    uint8_t firstbyte = mstate_old->inst.maddr[0];
    mstate_old->inst.maddr[0] = kNaClFullStop;

    SerializeAllProcessors();

    /* copy the rest of instruction */
    if (dst == mstate_old->inst.maddr) {
      /* but not the first byte! */
      firstbyte = *src;
      dst++, src++, sz--;
    }
    memcpy(dst, src, sz);

    SerializeAllProcessors();

    /* flip first byte back */
    mstate_old->inst.maddr[0] = firstbyte;
  }
}

int NCCopyCode(uint8_t *dst, uint8_t *src, NaClPcAddress vbase,
               size_t sz, int bundle_size) {
  struct NCValidatorState *vstate;
  vstate = NCValidateInit(vbase, vbase+sz, bundle_size);
  CHECK(NULL != vstate);
  NCDecodeSegmentPair(dst, src, vbase, sz, vstate, CopyInstruction);
  NCValidateFreeState(&vstate);
  return 0;
}

