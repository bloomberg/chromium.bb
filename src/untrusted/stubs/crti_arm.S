/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * The .init and .fini sections contain the _init and _fini functions,
 * respectively.  This file defines the prologue of each function, and
 * crtn_arm.S defines each epilogue.
 */

        .section .init
        .p2align 4
        .globl _init
        .type _init,%function
_init:

        .section .fini
        .p2align 4
        .globl _fini
        .type _fini,%function
_fini:
