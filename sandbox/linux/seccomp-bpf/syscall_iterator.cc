// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/syscall_iterator.h"

#include "base/macros.h"
#include "sandbox/linux/seccomp-bpf/linux_seccomp.h"

namespace sandbox {

namespace {

#if defined(__mips__) && (_MIPS_SIM == _MIPS_SIM_ABI32)
// This is true for Mips O32 ABI.
COMPILE_ASSERT(MIN_SYSCALL == __NR_Linux, min_syscall_should_be_4000);
#else
// This true for supported architectures (Intel and ARM EABI).
COMPILE_ASSERT(MIN_SYSCALL == 0u, min_syscall_should_always_be_zero);
#endif

// SyscallRange represents an inclusive range of system call numbers.
struct SyscallRange {
  uint32_t first;
  uint32_t last;
};

const SyscallRange kValidSyscallRanges[] = {
    // First we iterate up to MAX_PUBLIC_SYSCALL, which is equal to MAX_SYSCALL
    // on Intel architectures, but leaves room for private syscalls on ARM.
    {MIN_SYSCALL, MAX_PUBLIC_SYSCALL},
#if defined(__arm__)
    // ARM EABI includes "ARM private" system calls starting at
    // MIN_PRIVATE_SYSCALL, and a "ghost syscall private to the kernel" at
    // MIN_GHOST_SYSCALL.
    {MIN_PRIVATE_SYSCALL, MAX_PRIVATE_SYSCALL},
    {MIN_GHOST_SYSCALL, MAX_SYSCALL},
#endif
};

uint32_t NextSyscall(uint32_t cur, bool invalid_only) {
  for (const SyscallRange& range : kValidSyscallRanges) {
    if (range.first > 0 && cur < range.first - 1) {
      return range.first - 1;
    }
    if (cur <= range.last) {
      if (invalid_only && cur < range.last) {
        return range.last;
      }
      return cur + 1;
    }
  }

  // BPF programs only ever operate on unsigned quantities. So, that's how
  // we iterate; we return values from 0..0xFFFFFFFFu. But there are places,
  // where the kernel might interpret system call numbers as signed
  // quantities, so the boundaries between signed and unsigned values are
  // potential problem cases. We want to explicitly return these values from
  // our iterator.
  if (cur < 0x7FFFFFFFu)
    return 0x7FFFFFFFu;
  if (cur < 0x80000000u)
    return 0x80000000u;

  return 0xFFFFFFFFu;
}

}  // namespace

uint32_t SyscallIterator::Next() {
  if (done_) {
    return num_;
  }

  uint32_t val;
  do {
    val = num_;
    num_ = NextSyscall(num_, invalid_only_);
  } while (invalid_only_ && IsValid(val));

  done_ |= val == 0xFFFFFFFFu;
  return val;
}

bool SyscallIterator::IsValid(uint32_t num) {
  for (const SyscallRange& range : kValidSyscallRanges) {
    if (num >= range.first && num <= range.last) {
      return true;
    }
  }
  return false;
}

}  // namespace sandbox
