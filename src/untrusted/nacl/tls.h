/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Native Client support for thread local storage
 */

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_NACL_TLS_H
#define NATIVE_CLIENT_SRC_UNTRUSTED_NACL_TLS_H


/* size of the thread stack plus optional alignment fluff */
size_t __nacl_thread_stack_size(int add_fluff);

/* start of tdb given a combined tls area, c.f. comment in .c file */
char *__nacl_thread_stack_align(void* p);


/* size of tls data + bss + optional alignment fluff + tdb_size */
size_t __nacl_tls_combined_size(size_t tdb_size, int add_fluff);

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
