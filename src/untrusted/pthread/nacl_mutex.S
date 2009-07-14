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
 * NaCl service library.
 */

#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"

    .text
    .globl __nacl_create_mutex
    .globl __nacl_lock_mutex
    .globl __nacl_unlock_mutex
    .globl __nacl_try_lock_mutex

    .globl __nacl_create_condvar
    .globl __nacl_signal_condvar
    .globl __nacl_wait_condvar
    .globl __nacl_broadcast_condvar
    .globl __nacl_relative_timed_wait_condvar
    .globl __nacl_absolute_timed_wait_condvar

    .globl __nacl_create_thread
    .globl __nacl_exit_thread

    .globl __nacl_create_sem
    .globl __nacl_post_sem
    .globl __nacl_wait_sem


    .p2align    NACLENTRYALIGN
__nacl_create_mutex:
.CREATE_MUTEX_SYSCALL_ADDRESS = NACL_SYSCALL_ADDR(NACL_sys_mutex_create)
    jmp .CREATE_MUTEX_SYSCALL_ADDRESS

    .p2align    NACLENTRYALIGN
__nacl_lock_mutex:
.LOCK_MUTEX_SYSCALL_ADDRESS = NACL_SYSCALL_ADDR(NACL_sys_mutex_lock)
    jmp .LOCK_MUTEX_SYSCALL_ADDRESS

    .p2align    NACLENTRYALIGN
__nacl_unlock_mutex:
.UNLOCK_MUTEX_SYSCALL_ADDRESS = NACL_SYSCALL_ADDR(NACL_sys_mutex_unlock)
    jmp .UNLOCK_MUTEX_SYSCALL_ADDRESS

    .p2align    NACLENTRYALIGN
__nacl_try_lock_mutex:
.TRYLOCK_MUTEX_SYSCALL_ADDRESS = NACL_SYSCALL_ADDR(NACL_sys_mutex_trylock)
    jmp .TRYLOCK_MUTEX_SYSCALL_ADDRESS

    .p2align    NACLENTRYALIGN
__nacl_create_condvar:
.CREATE_COND_SYSCALL_ADDRESS = NACL_SYSCALL_ADDR(NACL_sys_cond_create)
    jmp .CREATE_COND_SYSCALL_ADDRESS

    .p2align    NACLENTRYALIGN
__nacl_signal_condvar:
.SIGNAL_COND_SYSCALL_ADDRESS = NACL_SYSCALL_ADDR(NACL_sys_cond_signal)
    jmp .SIGNAL_COND_SYSCALL_ADDRESS

    .p2align    NACLENTRYALIGN
__nacl_wait_condvar:
.WAIT_COND_SYSCALL_ADDRESS = NACL_SYSCALL_ADDR(NACL_sys_cond_wait)
    jmp .WAIT_COND_SYSCALL_ADDRESS

    .p2align    NACLENTRYALIGN
__nacl_broadcast_condvar:
.BROADCAST_COND_SYSCALL_ADDRESS = NACL_SYSCALL_ADDR(NACL_sys_cond_broadcast)
    jmp .BROADCAST_COND_SYSCALL_ADDRESS

    .p2align    NACLENTRYALIGN
__nacl_absolute_timed_wait_condvar:
.ABS_TIMED_WAIT_COND_SYSCALL_ADDRESS = NACL_SYSCALL_ADDR(NACL_sys_cond_timed_wait_abs)
    jmp .ABS_TIMED_WAIT_COND_SYSCALL_ADDRESS

    .p2align    NACLENTRYALIGN
__nacl_create_thread:
.CREATE_THREAD_SYSCALL_ADDRESS = NACL_SYSCALL_ADDR(NACL_sys_thread_create)
    jmp .CREATE_THREAD_SYSCALL_ADDRESS

    .p2align    NACLENTRYALIGN
__nacl_exit_thread:
.EXIT_THREAD_SYSCALL_ADDRESS = NACL_SYSCALL_ADDR(NACL_sys_thread_exit)
    jmp .EXIT_THREAD_SYSCALL_ADDRESS

    .p2align    NACLENTRYALIGN
__nacl_create_sem:
.CREATE_SEM_SYSCALL_ADDRESS = NACL_SYSCALL_ADDR(NACL_sys_sem_create)
    jmp .CREATE_SEM_SYSCALL_ADDRESS

    .p2align    NACLENTRYALIGN
__nacl_post_sem:
.POST_SEM_SYSCALL_ADDRESS = NACL_SYSCALL_ADDR(NACL_sys_sem_post)
    jmp .POST_SEM_SYSCALL_ADDRESS

    .p2align    NACLENTRYALIGN
__nacl_wait_sem:
.WAIT_SEM_SYSCALL_ADDRESS = NACL_SYSCALL_ADDR(NACL_sys_sem_wait)
    jmp .WAIT_SEM_SYSCALL_ADDRESS

