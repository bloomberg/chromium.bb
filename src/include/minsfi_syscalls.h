/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_INCLUDE_MINSFI_SYSCALLS_H_
#define NATIVE_CLIENT_SRC_INCLUDE_MINSFI_SYSCALLS_H_

#include <errno.h>
#include <stdint.h>
#include <unistd.h>

/*
 * This header declares the system calls supported by MinSFI and is included
 * by both the trusted and untrusted code.
 *
 * All prototypes must use exact-width integer types to ensure compatibility.
 * Function names must be prefixed with '__minsfi_syscall_' and return int32_t
 * in order to pass the PNaCl ABI verifier.
 *
 * Syscalls with pointer-type arguments will sandbox the untrusted pointers
 * but access the memory without further checking whether it points to a guard
 * region or not. Invoking syscalls with invalid pointers will therefore cause
 * a segmentation fault, as if the sandboxed code attempted to access the
 * memory address itself. The descriptions of individual syscalls below assume
 * pointer validity.
 */

/*
 * The MinsfiSyscallPtrTy macro translates pointer types used in untrusted code
 * to the sfiptr_t type for holding untrusted addresses when the header file
 * is included from trusted source files. This allows us to include the same
 * header file everywhere.
 *
 * Note that the trusted implementation of these functions must still sandbox
 * and correctly cast the untrusted pointers.
 */
#ifdef MINSFI_TRUSTED
#include "native_client/src/include/minsfi_ptr.h"
#define MinsfiSyscallPtrTy(TYPE) sfiptr_t
#else  // MINSFI_TRUSTED
#define MinsfiSyscallPtrTy(TYPE) TYPE
#endif  // MINSFI_TRUSTED

/* Immediately terminate the sandbox. This function never returns. */
int32_t __minsfi_syscall_exit(int32_t status) __attribute__((noreturn));

/*
 * Queries the limits of the memory region to be used as a heap.
 * The syscall stores addresses of the heap boundaries, relative to the sandbox
 * base, into the provided untrusted pointers. The upper limit is not inclusive.
 * It always succeeds and returns 0.
 */
int32_t __minsfi_syscall_heaplim(MinsfiSyscallPtrTy(char **) lower_lim,
                                 MinsfiSyscallPtrTy(char **) upper_lim);

/*
 * Queries system configuration at runtime.
 * Supported configuration names are listed below. Returns 0 and stores the
 * result in the provided integer if successful. Otherwise returns -1 and
 * stores EINVAL into the given pointer to errno.
 */
int32_t __minsfi_syscall_sysconf(uint32_t name,
                                 MinsfiSyscallPtrTy(int32_t *) result,
                                 MinsfiSyscallPtrTy(int32_t *) errno_ptr);

/*
 * List of supported sysconf configuration names. Values must be kept in sync
 * with NaCl's header files.
 */
#define MINSFI_SYSCONF_PAGESIZE       2

#endif  // NATIVE_CLIENT_SRC_INCLUDE_MINSFI_SYSCALLS_H_
