// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_SYSCALL_H__
#define SANDBOX_LINUX_SECCOMP_BPF_SYSCALL_H__

#include <signal.h>
#include <stdint.h>

namespace playground2 {

// We have to make sure that we have a single "magic" return address for
// our system calls, which we can check from within a BPF filter. This
// works by writing a little bit of asm() code that a) enters the kernel, and
// that also b) can be invoked in a way that computes this return address.
// Passing "nr" as "-1" computes the "magic" return address. Passing any
// other value invokes the appropriate system call.
intptr_t SandboxSyscall(int nr, ...);

}  // namespace

#endif  // SANDBOX_LINUX_SECCOMP_BPF_SYSCALL_H__
