/*
 * Copyright 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_NONSFI_LINUX_LINUX_SYS_PRIVATE_H_
#define NATIVE_CLIENT_SRC_NONSFI_LINUX_LINUX_SYS_PRIVATE_H_ 1

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/nonsfi/linux/linux_syscall_structs.h"

EXTERN_C_BEGIN

int linux_sigaction(int signum, const struct linux_sigaction *act,
                    struct linux_sigaction *oldact);
int linux_sigprocmask(int how, const linux_sigset_t *set, linux_sigset_t *oset);

EXTERN_C_END

#endif
