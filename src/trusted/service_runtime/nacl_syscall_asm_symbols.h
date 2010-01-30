/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#ifndef SERVICE_RUNTIME_NACL_SYSCALL_H__
#define SERVICE_RUNTIME_NACL_SYSCALL_H__

#if !NACL_MACOSX || defined(NACL_STANDALONE)
extern int NaClSyscallSeg();
#else
// This declaration is used only on Mac OSX for Chrome build
extern int NaClSyscallSeg() __attribute__((weak_import));
#endif
#endif
