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
 * NaCl Service Runtime
 */

#ifndef SERVICE_RUNTIME_NACL_SWITCH_TO_APP_H__
#define SERVICE_RUNTIME_NACL_SWITCH_TO_APP_H__ 1

#include "native_client/src/include/nacl_base.h"

EXTERN_C_BEGIN

struct NaClAppThread;

extern NORETURN void NaClSwitch(uint32_t prog_ctr,
                                uint32_t ebp,
                                uint32_t edi,
                                uint32_t esi,
                                uint32_t ebx,
                                uint32_t gs,  /* high 16 bits ignored */
                                uint32_t fs,
                                uint32_t es,
                                uint32_t spring_addr,
                                uint32_t cs,
                                uint32_t ds,
                                uint32_t eax,
                                uint32_t ss,
                                uint32_t stack_ptr);

NORETURN void NaClStartThreadInApp(struct NaClAppThread *natp,
                                   uint32_t             new_prog_ctr);

NORETURN void NaClSwitchToApp(struct NaClAppThread *natp,
                              uint32_t             new_prog_ctr);

EXTERN_C_END

#endif
