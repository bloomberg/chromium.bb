/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
* The .init/.fini sections contain just one symbol, which is the beginning
* of a function and hence needs to be aligned in accordance with
* the NativeClient conventions.
*
* The linker will append more code to these sections.
*
* The last code appended to these sections is crtn_arm.S
* which adds the return code for the respective functions.
*
* The __kNaClSrpcHandlers symbol is used by __CommandLoop and
* NaClSrpcDefaultServerLoop
*/

#include "native_client/src/trusted/service_runtime/nacl_config.h"

/* TODO(robertm): this should really by sizeof(nc_thread_descriptor_t) */
    .set TBD_SIZE, 128
/* ====================================================================== */
/* BEGIN .init section */
/* ====================================================================== */


    .section .init
    .align NACL_ARM_BUNDLE_SIZE_LOG
    .global _init
_init:
    /* TODO(robertm): check whether r4 needs to be saved */
    push    {r4, lr}
    /*
     * The code that follows will be appended by the linker, and will not
     * be aligned individually, so we need to align it.
     */
     .align NACL_ARM_BUNDLE_SIZE_LOG

    /*
     * contained in crtn_arm.S (properly sandboxed, of course)
     *
     * pop  {r4, pc}
     */

/* ====================================================================== */
/* BEGIN .fini section */
/* ====================================================================== */
    .section .fini
    .align NACL_ARM_BUNDLE_SIZE_LOG
    .global _fini
_fini:
    /* c.f. init section comment */
    push    {r4, lr}
    .align NACL_ARM_BUNDLE_SIZE_LOG


/* ====================================================================== */
/* BEGIN .tdata section */
/* ====================================================================== */
    /* ensure we have space for a tdb at the very beginning of the tls area */
    /* NOTE: this is different from x86 where we put the tdb at the end */
    .section .tdata
    .global __tdb
__tdb:
    .fill TBD_SIZE, 1, -1

/* ====================================================================== */
/* BEGIN .nacl_rpc_methods section */
/* This section contains the simple rpc method descriptors. */
/* ====================================================================== */
    /* TODO: check proper .section syntax for arm */
    .section .nacl_rpc_methods  /* , "aw", @progbits */
    .align NACL_ARM_BUNDLE_SIZE_LOG
    .global __kNaClSrpcHandlers
__kNaClSrpcHandlers:

