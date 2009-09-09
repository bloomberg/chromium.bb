/*
 * Copyright 2008, Google Inc.
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
 * Native Client support for thread local storage
 */

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_NACL_TLS_H
#define NATIVE_CLIENT_SRC_UNTRUSTED_NACL_TLS_H


/* TODO(robertm): try to reduce visibility of this constant */
#define TLS_ALIGNMENT 32

/* size of tls data + bss + alignment fluff + tdb_size */
size_t __nacl_tls_combined_size(size_t tdb_size);

/* start of tdb given a combined tls area, c.f. comment in .c file */
void *__nacl_tls_tdb_start(void* p);


/* given a tls block, initialize data and bss from template */
void __nacl_tls_data_bss_initialize_from_template(void* combined_area);

/*
 * This function initializes TLS and TDB for the main thread.
 * It is always called - with and without pthreads.
 * TDB initialization for the main thread is somewhat simpler than for
 * other threads.
 */
int __pthread_initialize_minimal(size_t tdb_size);


/* nacl specific addition defined in newlib */
void __newlib_thread_init();


#endif /* NATIVE_CLIENT_SRC_UNTRUSTED_NACL_TLS_H */
