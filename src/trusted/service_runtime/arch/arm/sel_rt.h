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
 * NaCl Secure Runtime
 */

#ifndef __ARCH_ARM_SEL_RT_H__
#define __ARCH_ARM_SEL_RT_H__    1

#include "native_client/src/include/portability.h"

uint32_t NaClGetSp(void);

struct NaClThreadContext {
uint32_t    r4, r5, r6, r7, r8, r9, r10, fp, esp, lr, eip;
          /* 0   4   8   c  10  14   18  1c   20  24   28 */
};

/*
 * A sanity check -- should be invoked in some early function, e.g.,
 * main, or something that main invokes early.
 */
/* TODO(petr): move this to platform-specific file,
 * should be done together with splitting sel_ldr.c
 */
#define NACL_THREAD_CHECK do {                    \
    CHECK(sizeof(struct NaClThreadContext) == 44);  \
  } while (0)

struct NaClApp;  /* fwd */

int NaClThreadContextCtor(struct NaClThreadContext  *ntcp,
                          uintptr_t                 pc,
                          uintptr_t                 sp,
                          uint16_t                  r9);

void NaClThreadContextDtor(struct NaClThreadContext *ntcp);

#endif /* __ARCH_ARM_SEL_RT_H__ */

