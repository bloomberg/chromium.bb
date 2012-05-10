/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* for everything from libgcc_eh */
EXTERN (__deregister_frame_info __register_frame_info mywrite )

/* for mylibc from libnacl */
EXTERN (
_calloc_r
_execve
_exit
_free_r
_malloc_r
_realloc_r
)

/* for mylibc from libnacl */
EXTERN (
getpid
close
dup2
fstat
getdents
gettimeofday
lseek
nanosleep
open
read
sbrk
stat
write
)

/* also for mylibc from pthreads */
EXTERN (
__libnacl_mandatory_irt_query
__nacl_read_tp
__nacl_tls_combined_size
__nacl_tls_data_bss_initialize_from_template
__nacl_tls_tdb_start
__pthread_initialize_minimal

sched_yield

__local_lock_acquire
__local_lock_acquire_recursive
__local_lock_close_recursive
__local_lock_init_recursive
__local_lock_release
__local_lock_release_recursive
)
