// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SERVICES_SYSCALL_WRAPPERS_H_
#define SANDBOX_LINUX_SERVICES_SYSCALL_WRAPPERS_H_

#include <sys/types.h>

#include "sandbox/sandbox_export.h"

namespace sandbox {

// Provide direct system call wrappers for a few common system calls.
// These are guaranteed to perform a system call and do not rely on things such
// as caching the current pid (c.f. getpid()).

SANDBOX_EXPORT pid_t sys_getpid(void);

SANDBOX_EXPORT pid_t sys_gettid(void);

SANDBOX_EXPORT long sys_clone(unsigned long flags);

// |regs| is not supported and must be passed as nullptr.
SANDBOX_EXPORT long sys_clone(unsigned long flags,
                              void* child_stack,
                              pid_t* ptid,
                              pid_t* ctid,
                              decltype(nullptr) regs);

SANDBOX_EXPORT void sys_exit_group(int status);

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SERVICES_SYSCALL_WRAPPERS_H_
