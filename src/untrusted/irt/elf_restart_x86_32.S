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
         * arg 1:  4(%esp), new stack pointer
         * arg 2:  8(%esp), entry point
         * arg 3:  12(%esp), atexit function, to be left in %edx
         */
        .global jump_to_elf_start
jump_to_elf_start:
        mov 12(%esp), %edx
        mov 8(%esp), %eax
        mov 4(%esp), %esp
        nacljmp %eax
