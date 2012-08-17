// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Generated from the Linux kernel's calls.S.
#ifndef SANDBOX_LINUX_SERVICES_ARM_LINUX_SYSCALLS_H_
#define SANDBOX_LINUX_SERVICES_ARM_LINUX_SYSCALLS_H_

// This file doesn't yet list all syscalls.
// TODO(jorgelo): define all ARM syscalls.

#if !defined(__arm__)
#error "Including header on wrong architecture"
#endif

// __NR_SYSCALL_BASE is defined in <asm/unistd.h>.
#include <asm/unistd.h>

#ifndef __NR_process_vm_readv
#define __NR_process_vm_readv (__NR_SYSCALL_BASE+376)
#endif

#ifndef __NR_process_vm_writev
#define __NR_process_vm_writev (__NR_SYSCALL_BASE+377)
#endif

#ifndef __ARM_NR_cmpxchg
#define __ARM_NR_cmpxchg  (__ARM_NR_BASE+0x00fff0)
#endif

#endif  // SANDBOX_LINUX_SERVICES_ARM_LINUX_SYSCALLS_H_

