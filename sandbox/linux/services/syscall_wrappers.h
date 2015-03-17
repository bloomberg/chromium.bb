// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SERVICES_SYSCALL_WRAPPERS_H_
#define SANDBOX_LINUX_SERVICES_SYSCALL_WRAPPERS_H_

#include <stdint.h>
#include <sys/types.h>

#include "sandbox/sandbox_export.h"

struct sock_fprog;
struct rlimit64;
struct cap_hdr;
struct cap_data;

namespace sandbox {

// Provide direct system call wrappers for a few common system calls.
// These are guaranteed to perform a system call and do not rely on things such
// as caching the current pid (c.f. getpid()) unless otherwise specified.

SANDBOX_EXPORT pid_t sys_getpid(void);

SANDBOX_EXPORT pid_t sys_gettid(void);

SANDBOX_EXPORT long sys_clone(unsigned long flags);

// |regs| is not supported and must be passed as nullptr. |child_stack| must be
// nullptr, since otherwise this function cannot safely return. As a
// consequence, this function does not support CLONE_VM.
SANDBOX_EXPORT long sys_clone(unsigned long flags,
                              decltype(nullptr) child_stack,
                              pid_t* ptid,
                              pid_t* ctid,
                              decltype(nullptr) regs);

SANDBOX_EXPORT void sys_exit_group(int status);

// The official system call takes |args| as void*  (in order to be extensible),
// but add more typing for the cases that are currently used.
SANDBOX_EXPORT int sys_seccomp(unsigned int operation,
                               unsigned int flags,
                               const struct sock_fprog* args);

// Some libcs do not expose a prlimit64 wrapper.
SANDBOX_EXPORT int sys_prlimit64(pid_t pid,
                                 int resource,
                                 const struct rlimit64* new_limit,
                                 struct rlimit64* old_limit);

// Some libcs do not expose capget/capset wrappers. We want to use these
// directly in order to avoid pulling in libcap2.
SANDBOX_EXPORT int sys_capget(struct cap_hdr* hdrp, struct cap_data* datap);
SANDBOX_EXPORT int sys_capset(struct cap_hdr* hdrp,
                              const struct cap_data* datap);

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SERVICES_SYSCALL_WRAPPERS_H_
