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
 * Native Client dummy implementation for locks - required for newlib
 */

#include <sys/unistd.h>

struct _LOCK_T;

/*
 * These functions provide dummy definitions when libpthread is not being
 * used to resolve symbols in other parts of the runtime library.  Because
 * they are marked as weak symbols, the definitions are overridden if the
 * real libpthread is used.
 */
void __local_lock_init(_LOCK_T* lock) __attribute__ ((weak));
void __local_lock_init_recursive(_LOCK_T* lock) __attribute__ ((weak));
void __local_lock_close(_LOCK_T* lock) __attribute__ ((weak));
void __local_lock_close_recursive(_LOCK_T* lock) __attribute__ ((weak));
void __local_lock_acquire(_LOCK_T* lock) __attribute__ ((weak));
void __local_lock_acquire_recursive(_LOCK_T* lock) __attribute__ ((weak));
int __local_lock_try_acquire(_LOCK_T* lock) __attribute__ ((weak));
int __local_lock_try_acquire_recursive(_LOCK_T* lock) __attribute__ ((weak));
void __local_lock_release(_LOCK_T* lock) __attribute__ ((weak));
void __local_lock_release_recursive(_LOCK_T* lock) __attribute__ ((weak));

void __local_lock_init(_LOCK_T* lock) {
  return;
}

void __local_lock_init_recursive(_LOCK_T* lock) {
  return;
}

void __local_lock_close(_LOCK_T* lock) {
  return;
}

void __local_lock_close_recursive(_LOCK_T* lock) {
  return;
}

void __local_lock_acquire(_LOCK_T* lock) {
  return;
}

void __local_lock_acquire_recursive(_LOCK_T* lock) {
  return;
}

int __local_lock_try_acquire(_LOCK_T* lock) {
  return 0;
}

int __local_lock_try_acquire_recursive(_LOCK_T* lock) {
  return 0;
}

void __local_lock_release(_LOCK_T* lock) {
  return;
}

void __local_lock_release_recursive(_LOCK_T* lock) {
  return;
}
