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
         *
         * We save the full 64 bits of %rsp and %rbp so that we can
         * check that they have been correctly extended with the value
         * of %r15.
         */
#if defined(__i386__)
        movl %esp, exception_handler_esp
#elif defined(__x86_64__)
        movq %rsp, exception_handler_rsp(%rip)
        movq %rbp, exception_handler_rbp(%rip)
#else
# error Unknown platform
#endif
        jmp exception_handler
