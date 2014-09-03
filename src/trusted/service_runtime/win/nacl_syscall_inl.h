/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl service runtime syscall inline header file.
 */

#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_WIN_NACL_SYSCALL_INL_H_
#define NATIVE_CLIENT_SERVICE_RUNTIME_WIN_NACL_SYSCALL_INL_H_

/*
 * Syscall return value mapper.  The posix wrappers in windows return
 * -1 on error, store the error code in the thread-specific errno
 * variable, and return -1 instead.  Since we are using these
 * wrappers, we merely detect when any host OS syscall returned -1,
 * and pass -errno back to the NaCl app.  (The syscall wrappers on the
 * NaCl app side will similarly follow the negative-values-are-errors
 * convention).
 */
static INLINE intptr_t NaClXlateSysRet(intptr_t rv) {
  return (rv != -1) ? rv : -NaClXlateErrno(errno);
}

#endif
