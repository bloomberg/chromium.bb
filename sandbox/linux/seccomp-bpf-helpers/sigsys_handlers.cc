// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: any code in this file MUST be async-signal safe.

#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"

#include <unistd.h>

#include "base/basictypes.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"

namespace {

inline bool IsArchitectureX86_64() {
#if defined(__x86_64__)
  return true;
#else
  return false;
#endif
}

// Write |error_message| to stderr. Similar to RawLog(), but a bit more careful
// about async-signal safety. |size| is the size to write and should typically
// not include a terminating \0.
void WriteToStdErr(const char* error_message, size_t size) {
  while (size > 0) {
    // TODO(jln): query the current policy to check if send() is available and
    // use it to perform a non-blocking write.
    const int ret = HANDLE_EINTR(write(STDERR_FILENO, error_message, size));
    // We can't handle any type of error here.
    if (ret <= 0 || static_cast<size_t>(ret) > size) break;
    size -= ret;
    error_message += ret;
  }
}

// Print a seccomp-bpf failure to handle |sysno| to stderr in an
// async-signal safe way.
void PrintSyscallError(uint32_t sysno) {
  if (sysno >= 1024)
    sysno = 0;
  // TODO(markus): replace with async-signal safe snprintf when available.
  const size_t kNumDigits = 4;
  char sysno_base10[kNumDigits];
  uint32_t rem = sysno;
  uint32_t mod = 0;
  for (int i = kNumDigits - 1; i >= 0; i--) {
    mod = rem % 10;
    rem /= 10;
    sysno_base10[i] = '0' + mod;
  }
  static const char kSeccompErrorPrefix[] =
      __FILE__":**CRASHING**:seccomp-bpf failure in syscall ";
  static const char kSeccompErrorPostfix[] = "\n";
  WriteToStdErr(kSeccompErrorPrefix, sizeof(kSeccompErrorPrefix) - 1);
  WriteToStdErr(sysno_base10, sizeof(sysno_base10));
  WriteToStdErr(kSeccompErrorPostfix, sizeof(kSeccompErrorPostfix) - 1);
}

}  // namespace.

namespace sandbox {

intptr_t CrashSIGSYS_Handler(const struct arch_seccomp_data& args, void* aux) {
  uint32_t syscall = args.nr;
  if (syscall >= 1024)
    syscall = 0;
  PrintSyscallError(syscall);

  // Encode 8-bits of the 1st two arguments too, so we can discern which socket
  // type, which fcntl, ... etc., without being likely to hit a mapped
  // address.
  // Do not encode more bits here without thinking about increasing the
  // likelihood of collision with mapped pages.
  syscall |= ((args.args[0] & 0xffUL) << 12);
  syscall |= ((args.args[1] & 0xffUL) << 20);
  // Purposefully dereference the syscall as an address so it'll show up very
  // clearly and easily in crash dumps.
  volatile char* addr = reinterpret_cast<volatile char*>(syscall);
  *addr = '\0';
  // In case we hit a mapped address, hit the null page with just the syscall,
  // for paranoia.
  syscall &= 0xfffUL;
  addr = reinterpret_cast<volatile char*>(syscall);
  *addr = '\0';
  for (;;)
    _exit(1);
}

// TODO(jln): refactor the reporting functions.

intptr_t SIGSYSCloneFailure(const struct arch_seccomp_data& args, void* aux) {
  static const char kSeccompCloneError[] =
      __FILE__":**CRASHING**:clone() failure\n";
  WriteToStdErr(kSeccompCloneError, sizeof(kSeccompCloneError) - 1);
  // "flags" is the first argument in the kernel's clone().
  // Mark as volatile to be able to find the value on the stack in a minidump.
  volatile uint64_t clone_flags = args.args[0];
  volatile char* addr;
  if (IsArchitectureX86_64()) {
    addr = reinterpret_cast<volatile char*>(clone_flags & 0xFFFFFF);
    *addr = '\0';
  }
  // Hit the NULL page if this fails to fault.
  addr = reinterpret_cast<volatile char*>(clone_flags & 0xFFF);
  *addr = '\0';
  for (;;)
    _exit(1);
}

intptr_t SIGSYSPrctlFailure(const struct arch_seccomp_data& args,
                            void* /* aux */) {
  static const char kSeccompPrctlError[] =
      __FILE__":**CRASHING**:prctl() failure\n";
  WriteToStdErr(kSeccompPrctlError, sizeof(kSeccompPrctlError) - 1);
  // Mark as volatile to be able to find the value on the stack in a minidump.
  volatile uint64_t option = args.args[0];
  volatile char* addr =
      reinterpret_cast<volatile char*>(option & 0xFFF);
  *addr = '\0';
  for (;;)
    _exit(1);
}

intptr_t SIGSYSIoctlFailure(const struct arch_seccomp_data& args,
                            void* /* aux */) {
  static const char kSeccompIoctlError[] =
      __FILE__":**CRASHING**:ioctl() failure\n";
  WriteToStdErr(kSeccompIoctlError, sizeof(kSeccompIoctlError) - 1);
  // Make "request" volatile so that we can see it on the stack in a minidump.
  volatile uint64_t request = args.args[1];
  volatile char* addr = reinterpret_cast<volatile char*>(request & 0xFFFF);
  *addr = '\0';
  // Hit the NULL page if this fails.
  addr = reinterpret_cast<volatile char*>(request & 0xFFF);
  *addr = '\0';
  for (;;)
    _exit(1);
}

}  // namespace sandbox.
