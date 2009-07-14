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
 * NaCl Simple/secure ELF loader (NaCl SEL) memory protection abstractions.
 */

#ifndef SERVICE_RUNTIME_SEL_MEMORY_H__
#define SERVICE_RUNTIME_SEL_MEMORY_H__ 1

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
* We do not use posix_memalign but instead directly attempt to mmap
* (or VirtualAlloc) memory into aligned addresses, since we want to be
* able to munmap pages to map in shared memory pages for the NaCl
* versions of shmat or mmap, esp if SHM_REMAP is used.  Note that the
* Windows ABI has 4KB pages for operations like page protection, but
* 64KB allocation granularity (see nacl_config.h), and since we want
* host-OS indistinguishability, this means we inherit this restriction
* into our least-common-denominator design.
*/
#define MAX_RETRIES     1024

int   NaCl_page_alloc(void    **p,
                      size_t  num_bytes);

void  NaCl_page_free(void     *p,
                     size_t   num_bytes);

int   NaCl_mprotect(void          *addr,
                    size_t        len,
                    int           prot);

int   NaCl_madvise(void           *start,
                   size_t         length,
                   int            advice);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*  SERVICE_RUNTIME_SEL_MEMORY_H__ */
