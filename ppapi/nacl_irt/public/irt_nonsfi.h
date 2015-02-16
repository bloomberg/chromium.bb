/*
 * Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
// TODO(mazda): Remove this file once NaCl has been rolled to the revision
// that includes the definition of nacl_irt_icache.
// https://code.google.com/p/nativeclient/issues/detail?id=4033

#ifndef PPAPI_NACL_IRT_PUBLIC_IRT_NONSFI_H_
#define PPAPI_NACL_IRT_PUBLIC_IRT_NONSFI_H_

#if !defined(NACL_IRT_ICACHE_v0_1)

#include <stddef.h>

/*
 * This interface is only available on ARM, only for Non-SFI.
 */
#define NACL_IRT_ICACHE_v0_1 "nacl-irt-icache-0.1"
struct nacl_irt_icache {
  /*
   * clear_cache() makes instruction cache and data cache for the address
   * range from |addr| to |(intptr_t)addr + size| (exclusive) coherent.
   */
  int (*clear_cache)(void* addr, size_t size);
};

#endif

#endif  // PPAPI_NACL_IRT_PUBLIC_IRT_NONSFI_H_
