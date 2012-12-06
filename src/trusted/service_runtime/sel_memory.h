/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl Simple/secure ELF loader (NaCl SEL) memory protection abstractions.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_SEL_MEMORY_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_SEL_MEMORY_H_ 1

#include "native_client/src/include/nacl_compiler_annotations.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int   NaCl_page_alloc(void    **p,
                      size_t  num_bytes) NACL_WUR;

int   NaCl_page_alloc_randomized(void   **p,
                                 size_t num_bytes) NACL_WUR;

int   NaCl_page_alloc_at_addr(void    **p,
                              size_t  num_bytes) NACL_WUR;

void  NaCl_page_free(void     *p,
                     size_t   num_bytes);

int   NaCl_mprotect(void          *addr,
                    size_t        len,
                    int           prot) NACL_WUR;

int   NaCl_madvise(void           *start,
                   size_t         length,
                   int            advice) NACL_WUR;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*  NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_SEL_MEMORY_H_ */
