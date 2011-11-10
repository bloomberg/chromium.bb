/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

    /*
     * The .init section contains one symbol, _init, which is the entry
     * of a function and hence needs to be aligned in accordance with
     * the NativeClient conventions.
     */
    .section .init
    .p2align NACLENTRYALIGN
    .global _init
_init:
    pushq  %rbp
    movq   %rsp, %rbp
    /*
     * The code that follows will be appended by the linker, and will not
     * be aligned individually, so we need to align it.
     */
    .p2align NACLENTRYALIGN

    /*
     * The .fini section contains one symbol, _fini, which is the entry
     * of a function and hence needs to be aligned in accordance with
     * the NativeClient conventions.
     */
    .section .fini
    .p2align NACLENTRYALIGN
    .global _fini
_fini:
    pushq  %rbp
    movq   %rsp, %rbp

    /*
     * The code that follows will be appended by the linker, and will not
     * be aligned individually, so we need to align it.
     */
    .p2align NACLENTRYALIGN
