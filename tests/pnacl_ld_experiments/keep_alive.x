/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

EXTERN (__deregister_frame_info __register_frame_info mywrite )

/* for mylibc */
EXTERN (
_calloc_r
_execve
_exit
_free_r
_malloc_r
_realloc_r
close
dup2
fcntl
fork
forkfstat
fstat
getdents
getlogin
getpid
getpwnam
getpwuid
gettimeofday
getuid
issetugid
issetugid
kill
link
lseek
lstat
mkdir
nanosleep
open
pipe
read
sbrk
sigprocmask
stat
times
unlink
vfork
wait
waitpid
write
)

/* also for mylibc */
EXTERN (
__local_lock_acquire
__local_lock_acquire_recursive
__local_lock_close_recursive
__local_lock_init_recursive
__local_lock_release
__local_lock_release_recursive
)
