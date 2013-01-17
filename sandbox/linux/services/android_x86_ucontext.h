// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SERVICES_ANDROID_X86_UCONTEXT_H_
#define SANDBOX_LINUX_SERVICES_ANDROID_X86_UCONTEXT_H_

// This is basically include/uapi/asm-generic/ucontext.h from the Linux kernel.
// In theory, Android libc's could expand the structure for its own internal
// use, but if the libc provides an API that expects this expanded structure,
// __BIONIC_HAVE_UCONTEXT_T should be defined.

#if !defined(__BIONIC_HAVE_UCONTEXT_T)
#include <asm/sigcontext.h>

// We also need greg_t for the sandbox, include it in this header as well.
typedef unsigned long greg_t;

typedef struct ucontext {
  unsigned long   uc_flags;
  struct ucontext  *uc_link;
  stack_t     uc_stack;
  struct sigcontext uc_mcontext;
  sigset_t    uc_sigmask; /* mask last for extensibility */
} ucontext_t;

#else
#include <sys/ucontext.h>
#endif  // __BIONIC_HAVE_UCONTEXT_T

#endif  // SANDBOX_LINUX_SERVICES_ANDROID_X86_UCONTEXT_H_
