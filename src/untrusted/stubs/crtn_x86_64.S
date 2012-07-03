/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

    /*
     * The .init section needs to be finished by a function return.
     */
    .section .init
    .p2align NACLENTRYALIGN,0x90

    movq %rbp, %rsp
    popq %r11
    movl %r11d,%ebp
    addq %r15,%rbp
    naclret

    .p2align NACLENTRYALIGN,0xf4

    /*
     * The .fini section needs to be finished by a function return.
     */
    .section .fini
    .p2align NACLENTRYALIGN,0x90

    movq %rbp, %rsp
    popq %r11
    movl %r11d,%ebp
    addq %r15,%rbp
    naclret

    .p2align NACLENTRYALIGN,0xf4
