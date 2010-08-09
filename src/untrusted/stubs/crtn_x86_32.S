 /*
  * Copyright 2010 The Native Client Authors. All rights reserved.
  * Use of this source code is governed by a BSD-style license that can
  * be found in the LICENSE file.
  */


    /*
     * The .init section needs to be finished by a function return.
     */
    .section .init
    .p2align NACLENTRYALIGN,0x90

    leave
    popl %ecx
    nacljmp %ecx

.p2align NACLENTRYALIGN,0xf4

    /*
     * The .fini section needs to be finished by a function return.
     */
    .section .fini
    .p2align NACLENTRYALIGN,0x90

    leave
    popl %ecx
    nacljmp %ecx

    .p2align NACLENTRYALIGN,0xf4

    .text
    .weak __NaClSrpcListServices
__NaClSrpcListServices:

    /*
     * This section contains the simple rpc method descriptors.
     */
    .section .nacl_rpc_methods, ""
    .4byte 0
    .4byte 0
    .global __kNaClSrpcHandlerEnd
__kNaClSrpcHandlerEnd:
