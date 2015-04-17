// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_SIGNAL_H_
#define SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_SIGNAL_H_

// NOTE: On some toolchains, signal related ABI is incompatible with Linux's
// (not undefined, but defined different values and in different memory
// layouts). So, fill the gap here.

#if defined(__native_client_nonsfi__)
#if !defined(__i386__) && !defined(__arm__)
#error "Unsupported platform"
#endif

#include <signal.h>

#define LINUX_SIGBUS 7    // 10 in PNaCl toolchain.
#define LINUX_SIGSEGV 11  // 11 in PNaCl toolchain. Defined for consistency.
#define LINUX_SIGCHLD 17  // 20 in PNaCl toolchain.
#define LINUX_SIGSYS 31   // 12 in PNaCl toolchain.

#define LINUX_SIG_BLOCK 0    // 1 in PNaCl toolchain.
#define LINUX_SIG_UNBLOCK 1  // 2 in PNaCl toolchain.

#define LINUX_SA_SIGINFO 4           // 2 in PNaCl toolchain.
#define LINUX_SA_NODEFER 0x40000000  // Undefined in PNaCl toolchain.
#define LINUX_SA_RESTART 0x10000000  // Undefined in PNaCl toolchain.

#define LINUX_SIG_DFL 0  // In PNaCl toolchain, unneeded cast is applied.

struct LinuxSigInfo {
  int si_signo;
  int si_errno;
  int si_code;

  // Extra data is followed by the |si_code|. The length depends on the
  // signal number.
  char _sifields[1];
};

#include "sandbox/linux/system_headers/linux_ucontext.h"

#else  // !defined(__native_client_nonsfi__)

// Just alias the toolchain's value.
#include <signal.h>

#define LINUX_SIGBUS SIGBUS
#define LINUX_SIGSEGV SIGSEGV
#define LINUX_SIGCHLD SIGCHLD
#define LINUX_SIGSYS SIGSYS

#define LINUX_SIG_BLOCK SIG_BLOCK
#define LINUX_SIG_UNBLOCK SIG_UNBLOCK

#define LINUX_SA_SIGINFO SA_SIGINFO
#define LINUX_SA_NODEFER SA_NODEFER
#define LINUX_SA_RESTART SA_RESTART

#define LINUX_SIG_DFL SIG_DFL

typedef siginfo_t LinuxSigInfo;

#if defined(__ANDROID__)
// Android's signal.h doesn't define ucontext etc.
#include "sandbox/linux/system_headers/linux_ucontext.h"
#endif  // defined(__ANDROID__)

#endif  // !defined(__native_client_nonsfi__)

#endif  // SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_SIGNAL_H_
