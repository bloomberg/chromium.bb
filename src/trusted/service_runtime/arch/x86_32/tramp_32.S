/*
 * Copyright 2008 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Simple/secure ELF loader (NaCl SEL).
 */

/*
 * Trampoline to transfer control from native client module to
 * sel_ldr's NaClSyscallSeg residing in the same address space
 */

#include "native_client/src/trusted/service_runtime/nacl_config.h"

#if __PIC__
        /*
         * This the PIC version of below, enabling the use of NaCl as
         * a -fPIC library, primarily for mac chrome.
         */

        .text
        .globl IDENTIFIER(NaCl_trampoline_seg_code)
HIDDEN(NaCl_trampoline_seg_code)
IDENTIFIER(NaCl_trampoline_seg_code):

        lcall   $0xcafe, $0xdeadbeef  /* orig-cs, thunk */
        .globl  IDENTIFIER(NaCl_tramp_cseg_patch)
HIDDEN(NaCl_tramp_cseg_patch)
IDENTIFIER(NaCl_tramp_cseg_patch):
        hlt
        .globl   IDENTIFIER(NaCl_trampoline_seg_end)
HIDDEN(NaCl_trampoline_seg_end)
IDENTIFIER(NaCl_trampoline_seg_end):

        .globl  IDENTIFIER(NaCl_text_prot)
HIDDEN(NaCl_text_prot)
IDENTIFIER(NaCl_text_prot):
        hlt
        .globl  IDENTIFIER(NaCl_text_prot_end)
HIDDEN(NaCl_text_prot_end)
IDENTIFIER(NaCl_text_prot_end):

        .text
        .globl  IDENTIFIER(NaClPcrelThunk)
HIDDEN(NaClPcrelThunk)
IDENTIFIER(NaClPcrelThunk):
        movl    $0xdeadbeef, %eax  /* known okay ds */
        .globl  IDENTIFIER(NaClPcrelThunk_dseg_patch)
HIDDEN(NaClPcrelThunk_dseg_patch)
IDENTIFIER(NaClPcrelThunk_dseg_patch):
        mov     %ax, %ds  /* disable data sandbox for this thread */
        movl    $0xdeadbeef, %ecx  /* nacl_user */
        .globl  IDENTIFIER(NaClPcrelThunk_globals_patch)
HIDDEN(NaClPcrelThunk_globals_patch)
IDENTIFIER(NaClPcrelThunk_globals_patch):
        jmp     IDENTIFIER(NaClSyscallSeg) /* PCREL patch */
        .globl  IDENTIFIER(NaClPcrelThunk_end)
HIDDEN(NaClPcrelThunk_end)
IDENTIFIER(NaClPcrelThunk_end):

#else

        .text
        .globl  IDENTIFIER(NaCl_trampoline_seg_code)
HIDDEN(NaCl_trampoline_seg_code)
IDENTIFIER(NaCl_trampoline_seg_code):
        movl    $0xdeadbeef, %eax  /* known okay ds */
        .globl  IDENTIFIER(NaCl_tramp_dseg_patch)
HIDDEN(NaCl_tramp_dseg_patch)
IDENTIFIER(NaCl_tramp_dseg_patch):
        mov     %ax, %ds  /* disable data sandbox for this thread */

        /*
         * space between $ and IDENTIFIER is critical, since windows cl.exe
         * will not expand IDENTIFIER(.) otherwise.
         */
        lcall   $0xcafe, $ IDENTIFIER(NaClSyscallSeg)
        .globl  IDENTIFIER(NaCl_tramp_cseg_patch)
HIDDEN(NaCl_tramp_cseg_patch)
IDENTIFIER(NaCl_tramp_cseg_patch):
        hlt
       .globl   IDENTIFIER(NaCl_trampoline_seg_end)
HIDDEN(NaCl_trampoline_seg_end)
IDENTIFIER(NaCl_trampoline_seg_end):

.globl IDENTIFIER(NaCl_text_prot)
HIDDEN(NaCl_text_prot)
IDENTIFIER(NaCl_text_prot):
        hlt
.globl IDENTIFIER(NaCl_text_prot_end)
HIDDEN(NaCl_text_prot_end)
IDENTIFIER(NaCl_text_prot_end):

#endif
