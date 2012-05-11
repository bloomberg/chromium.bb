/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stddef.h>

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_NACL_TLS_H
#define NATIVE_CLIENT_SRC_UNTRUSTED_NACL_TLS_H

/*
 * Allocates (using sbrk) and initializes the combined area for the
 * main thread.  Always called, whether or not pthreads is in use.
 */
int __pthread_initialize_minimal(size_t tdb_size);

/*
 * Initializes the thread-local data of newlib (which achieves
 * reentrancy by making heavy use of TLS).
 */
void __newlib_thread_init();

void __newlib_thread_exit();


/*
 * Returns the allocation size of the combined area, including TLS
 * (data + bss), TDB, and padding required to guarantee alignment.
 * Since the TDB layout is platform-specific, its size is passed as a
 * parameter.
 */
size_t __nacl_tls_combined_size(size_t tdb_size);

/*
 * Initializes the TLS of a combined area by copying the TLS data area
 * from the template (ELF image .tdata) and zeroing the TLS BSS area.
 * Returns the pointer that should go in the thread register.
 */
void *__nacl_tls_data_bss_initialize_from_template(void *combined_area,
                                                   size_t tdb_size);

/*
 * Fills in the memory of the size __nacl_tls_combined_size returned.
 * Returns the pointer that should go in the thread register.
 */
void *__nacl_tls_initialize_memory(void *combined_area, size_t tdb_size);

/* Read the per-thread pointer.  */
void *__nacl_read_tp(void);

/* Read the per-thread pointer and add an offset.  */
void *__nacl_add_tp(ptrdiff_t);


#endif /* NATIVE_CLIENT_SRC_UNTRUSTED_NACL_TLS_H */
