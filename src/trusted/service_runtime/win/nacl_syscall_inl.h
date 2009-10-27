/*
 * Copyright 2008  The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl service runtime syscall inline header file.
 */

#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_WIN_NACL_SYSCALL_INL_H_
#define NATIVE_CLIENT_SERVICE_RUNTIME_WIN_NACL_SYSCALL_INL_H_

static INLINE uint32_t NaClAppArg(struct NaClAppThread  *natp,
                                  int                   wordnum) {
  return natp->x_sp[wordnum];
}

/*
 * Syscall return value mapper.  The posix wrappers in windows return
 * -1 on error, store the error code in the thread-specific errno
 * variable, and return -1 instead.  Since we are using these
 * wrappers, we merely detect when any host OS syscall returned -1,
 * and pass -errno back to the NaCl app.  (The syscall wrappers on the
 * NaCl app side will similarly follow the negative-values-are-errors
 * convention).
 */
static INLINE int32_t NaClXlateSysRet(int32_t  rv) {
  return (rv != -1) ? rv : -errno;
}

/*
 * TODO(bsy): NaClXlateSysRetDesc to register returned descriptor in the
 * app's open descriptor table, wrapping it in a native descriptor
 * object.
 */

static INLINE int32_t NaClXlateSysRetAddr(struct NaClApp  *nap,
                                          int32_t         rv) {
  /* if rv is a bad address, we abort */
  return (rv != -1) ? NaClSysToUser(nap, rv) : -errno;
}

#endif
