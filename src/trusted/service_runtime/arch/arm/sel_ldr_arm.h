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

#ifndef SERVICE_RUNTIME_ARCH_ARM_SEL_LDR_H__
#define SERVICE_RUNTIME_ARCH_ARM_SEL_LDR_H__ 1

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"

#define NACL_MAX_ADDR_BITS      (26) /* mmap fails for 28 bits */

#define NACL_THREAD_MAX         (1 << NACL_PAGESHIFT)

#define NACL_NOOP_OPCODE        0xe1a00000  /* mov r0, r0 */
#define NACL_HALT_OPCODE        0xe1266676  /* mov pc, #0 */
#define NACL_HALT_LEN           4           /* length of halt instruction */

struct NaClApp;
struct NaClThreadContext;

uint32_t NaClGetThreadCombinedDescriptor(struct NaClThreadContext *user);

void NaClSetThreadCombinedDescriptor(struct NaClThreadContext *user,
                                     uint32_t tls_idx);

extern char NaClReadTP_start;
extern char NaClReadTP_end;

#endif /* SERVICE_RUNTIME_ARCH_ARM_SEL_LDR_H__ */
