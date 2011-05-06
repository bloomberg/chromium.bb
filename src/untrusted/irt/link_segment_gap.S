/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This file exists just to define this magic symbol.  The linker scripts
 * check for this symbol, and if it's defined in the link, it sets the
 * starting address for .rodata and similar sections.  We need this to give
 * linked executables a "segment gap" between their code and data, leaving
 * space for the IRT's code to be loaded in between.  The macro defining
 * the address is set in 'nacl_env' in the SConstruct file.
 */

        .globl __nacl_rodata_start
        .hidden __nacl_rodata_start
        __nacl_rodata_start = IRT_DATA_REGION_START
