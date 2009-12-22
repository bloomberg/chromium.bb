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
 * NaCl Service Runtime API.
 */

#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_INCLUDE_BITS_FCNTL_H_
#define NATIVE_CLIENT_SERVICE_RUNTIME_INCLUDE_BITS_FCNTL_H_

#ifdef __native_client__
#include <sys/types.h>
#endif

/* from bits/fcntl.h */
#define NACL_ABI_O_ACCMODE     0003
#define NACL_ABI_O_RDONLY        00
#define NACL_ABI_O_WRONLY        01
#define NACL_ABI_O_RDWR          02

#define NACL_ABI_O_CREAT       0100 /* not fcntl */
#define NACL_ABI_O_TRUNC      01000 /* not fcntl */
#define NACL_ABI_O_APPEND     02000

/*
 * Features not implemented by NaCl, but required by the newlib build.
 */
#define NACL_ABI_O_EXCL        0200
#define NACL_ABI_O_NONBLOCK   04000
#define NACL_ABI_O_NDELAY      NACL_ABI_O_NONBLOCK
#define NACL_ABI_O_SYNC      010000
#define NACL_ABI_O_FSYNC       NACL_ABI_O_SYNC
#define NACL_ABI_O_ASYNC     020000

/* XXX close on exec request; must match UF_EXCLOSE in user.h */
#define FD_CLOEXEC  1 /* posix */

/* fcntl(2) requests */
#define NACL_ABI_F_DUPFD   0 /* Duplicate fildes */
#define NACL_ABI_F_GETFD   1 /* Get fildes flags (close on exec) */
#define NACL_ABI_F_SETFD   2 /* Set fildes flags (close on exec) */
#define NACL_ABI_F_GETFL   3 /* Get file flags */
#define NACL_ABI_F_SETFL   4 /* Set file flags */
#ifndef _POSIX_SOURCE
#define NACL_ABI_F_GETOWN  5 /* Get owner - for ASYNC */
#define NACL_ABI_F_SETOWN  6 /* Set owner - for ASYNC */
#endif  /* !_POSIX_SOURCE */
#define NACL_ABI_F_GETLK   7 /* Get record-locking information */
#define NACL_ABI_F_SETLK   8 /* Set or Clear a record-lock (Non-Blocking) */
#define NACL_ABI_F_SETLKW  9 /* Set or Clear a record-lock (Blocking) */
#ifndef _POSIX_SOURCE
#define NACL_ABI_F_RGETLK  10  /* Test a remote lock to see if it is blocked */
#define NACL_ABI_F_RSETLK  11  /* Set or unlock a remote lock */
#define NACL_ABI_F_CNVT    12  /* Convert a fhandle to an open fd */
#define NACL_ABI_F_RSETLKW   13  /* Set or Clear remote record-lock(Blocking) */
#endif  /* !_POSIX_SOURCE */

/* fcntl(2) flags (l_type field of flock structure) */
#define NACL_ABI_F_RDLCK   1 /* read lock */
#define NACL_ABI_F_WRLCK   2 /* write lock */
#define NACL_ABI_F_UNLCK   3 /* remove lock(s) */
#ifndef _POSIX_SOURCE
#define NACL_ABI_F_UNLKSYS 4 /* remove remote locks for a given system */
#endif  /* !_POSIX_SOURCE */

extern int open (const char *file, int oflag, ...);

#endif
