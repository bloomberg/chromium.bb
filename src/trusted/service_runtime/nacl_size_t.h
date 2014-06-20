/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_NACL_SIZE_T_H
#define NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_NACL_SIZE_T_H

#include "native_client/src/include/portability.h"

/*
 * WORDSIZE is defined by service_runtime/include/machine/_types.h.  It is
 * defined as NACL_ABI_WORDSIZE before it is munged by export_header.py.
 */
#if defined(__native_client__) && !defined(NACL_ABI_WORDSIZE)
#define NACL_ABI_WORDSIZE WORDSIZE
#endif  /* defined(__native_client__) && !defined(NACL_ABI_WORDSIZE) */

#ifndef nacl_abi_size_t_defined
#define nacl_abi_size_t_defined
typedef uint32_t nacl_abi_size_t;
#endif

#define NACL_ABI_SIZE_T_MIN ((nacl_abi_size_t) 0)
#define NACL_ABI_SIZE_T_MAX ((nacl_abi_size_t) -1)

#ifndef nacl_abi_ssize_t_defined
#define nacl_abi_ssize_t_defined
typedef int32_t nacl_abi_ssize_t;
#endif

#define NACL_ABI_SSIZE_T_MAX \
  ((nacl_abi_ssize_t) (NACL_ABI_SIZE_T_MAX >> 1))
#define NACL_ABI_SSIZE_T_MIN \
  (~NACL_ABI_SSIZE_T_MAX)

#define NACL_PRIdNACL_SIZE NACL_PRI_(d, NACL_ABI_WORDSIZE)
#define NACL_PRIiNACL_SIZE NACL_PRI_(i, NACL_ABI_WORDSIZE)
#define NACL_PRIoNACL_SIZE NACL_PRI_(o, NACL_ABI_WORDSIZE)
#define NACL_PRIuNACL_SIZE NACL_PRI_(u, NACL_ABI_WORDSIZE)
#define NACL_PRIxNACL_SIZE NACL_PRI_(x, NACL_ABI_WORDSIZE)
#define NACL_PRIXNACL_SIZE NACL_PRI_(X, NACL_ABI_WORDSIZE)

/**
 * Inline functions to aid in conversion between system (s)size_t and
 * nacl_abi_(s)size_t
 *
 * These are defined as inline functions only if __native_client__ is
 * undefined, since in a nacl module size_t and nacl_abi_size_t are always
 * the same (and we don't have a definition for INLINE, so these won't compile)
 *
 * If __native_client__ *is* defined, these turn into no-ops.
 */
#ifdef __native_client__
  /**
   * NB: The "no-op" version of these functions does NO type conversion.
   * Please DO NOT CHANGE THIS. If you get a type error using these functions
   * in a NaCl module, it's a real error and should be fixed in your code.
   */
  #define nacl_abi_size_t_saturate(x) (x)
  #define nacl_abi_ssize_t_saturate(x) (x)
#else /* __native_client */
  static INLINE nacl_abi_size_t nacl_abi_size_t_saturate(size_t x) {
    if (x > NACL_ABI_SIZE_T_MAX) {
      return NACL_ABI_SIZE_T_MAX;
    } else {
      return (nacl_abi_size_t)x;
    }
  }

  static INLINE nacl_abi_ssize_t nacl_abi_ssize_t_saturate(ssize_t x) {
    if (x > NACL_ABI_SSIZE_T_MAX) {
      return NACL_ABI_SSIZE_T_MAX;
    } else if (x < NACL_ABI_SSIZE_T_MIN) {
      return NACL_ABI_SSIZE_T_MIN;
    } else {
      return (nacl_abi_ssize_t) x;
    }
  }
#endif /* __native_client */

#endif
