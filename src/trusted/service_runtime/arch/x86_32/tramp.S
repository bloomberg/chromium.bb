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
 * NaCl Simple/secure ELF loader (NaCl SEL).
 */

/*
 Trampoline to transfer control from native client module to
 sel_ldr's NaClSyscallSeg residing in the same address space
*/

#include "native_client/src/trusted/service_runtime/nacl_config.h"

        .text
.globl IDENTIFIER(NaCl_trampoline_seg_code)
        HIDDEN(NaCl_trampoline_seg_code)
IDENTIFIER(NaCl_trampoline_seg_code):
        movl    $0xdeadbeef, %eax
.globl IDENTIFIER(NaCl_tramp_dseg_patch)
        HIDDEN(NaCl_tramp_dseg_patch)
IDENTIFIER(NaCl_tramp_dseg_patch):
        mov     %eax, %ds  /* remove data sandbox */
        lcall   $0xcafe, $0xdeadbeef
        /* trampoline installer will s/0xdeadbeef/NaClSyscallSeg/ */
.globl IDENTIFIER(NaCl_tramp_cseg_patch)
        HIDDEN(NaCl_tramp_cseg_patch)
IDENTIFIER(NaCl_tramp_cseg_patch):
        ret
.globl IDENTIFIER(NaCl_trampoline_seg_end)
IDENTIFIER(NaCl_trampoline_seg_end):

.globl IDENTIFIER(NaCl_text_prot)
        HIDDEN(NaCl_text_prot)
IDENTIFIER(NaCl_text_prot):
        hlt
.globl IDENTIFIER(NaCl_text_prot_end)
        HIDDEN(NaCl_text_prot_end)
IDENTIFIER(NaCl_text_prot_end):
