/*
 * Copyright 2010  The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#ifndef NATIVE_CLIENT_SRC_INCLUDE_CONCURRENCY_OPS_H_
#define NATIVE_CLIENT_SRC_INCLUDE_CONCURRENCY_OPS_H_ 1


#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/portability.h"

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86

static INLINE void NaClWriteMemoryBarrier() {
#if NACL_WINDOWS
  /* Inline assembly is not available in x86-64 MSVC.  Use built-in. */
  _mm_sfence();
#else
  __asm__ __volatile__("sfence");
#endif
}

#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm

static INLINE void NaClWriteMemoryBarrier() {
  /*
   * This depends on ARMv7.  Ideally we should pass "-march=armv7-a"
   * to gcc instead of using ".arch", but that breaks the linking of
   * the plugin .so, which fails to build all its required libraries
   * with -fPIC.  TODO(mseaborn): Fix build to use -fPIC when needed.
   * See http://code.google.com/p/nativeclient/issues/detail?id=121
   */
  __asm__ __volatile__(".arch armv7-a\n"
                       "dsb");

  /*
   * We could support ARMv6 by instead using:
   * __asm__ __volatile__("mcr p15, 0, %0, c7, c10, 5"
   *                      : : "r" (0) : "memory");
   */
}

#else

#error "Define for other architectures"

#endif


#endif  /* NATIVE_CLIENT_SRC_INCLUDE_CONCURRENCY_OPS_H_ */
