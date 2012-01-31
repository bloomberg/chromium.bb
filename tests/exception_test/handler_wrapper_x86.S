/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

        .global exception_handler_wrapper

exception_handler_wrapper:
        /*
         * Record the state of some registers on entry to the
         * exception handler so that we can check that service_runtime
         * sets the correct values.  We cannot check this exactly from C.
         */
        movl %esp, exception_handler_esp
        jmp exception_handler


        .data
        .align 4
        .global exception_handler_esp
exception_handler_esp:
        .long 0x99999999
