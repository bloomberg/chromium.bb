/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * The ELF entry point ABI is such that assembly code is required to
 * call the entry point.
 * See http://code.google.com/p/nativeclient/issues/detail?id=1131
 */

        /*
         * arg 1:  %edi, new stack pointer
         * arg 2:  %esi, entry point
         * arg 3:  %edx, atexit function, to be left in %edx
         */
        .global jump_to_elf_start
jump_to_elf_start:
        naclrestsp %edi, %r15
        nacljmp %esi, %r15
