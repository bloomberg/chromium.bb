// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/syscall_iterator.h"

#include <stdint.h>

#include "sandbox/linux/seccomp-bpf/linux_seccomp.h"
#include "sandbox/linux/tests/unit_tests.h"

namespace sandbox {

namespace {

const bool kFalseTrue[] = {false, true};

SANDBOX_TEST(SyscallIterator, Monotonous) {
  for (bool invalid_only : kFalseTrue) {
    uint32_t prev = 0;
    bool have_prev = false;
    for (SyscallIterator iter(invalid_only); !iter.Done();) {
      uint32_t sysnum = iter.Next();

      if (have_prev) {
        SANDBOX_ASSERT(sysnum > prev);
      } else if (!invalid_only) {
        // The iterator should start at 0.
        SANDBOX_ASSERT(sysnum == 0);
      }

      prev = sysnum;
      have_prev = true;
    }

    // The iterator should always return 0xFFFFFFFFu as the last value.
    SANDBOX_ASSERT(have_prev);
    SANDBOX_ASSERT(prev == 0xFFFFFFFFu);
  }
}

// AssertRange checks that SyscallIterator produces all system call
// numbers in the inclusive range [min, max].
void AssertRange(uint32_t min, uint32_t max) {
  SANDBOX_ASSERT(min < max);
  uint32_t prev = min - 1;
  for (SyscallIterator iter(false); !iter.Done();) {
    uint32_t sysnum = iter.Next();
    if (sysnum >= min && sysnum <= max) {
      SANDBOX_ASSERT(prev == sysnum - 1);
      prev = sysnum;
    }
  }
  SANDBOX_ASSERT(prev == max);
}

SANDBOX_TEST(SyscallIterator, ValidSyscallRanges) {
  AssertRange(MIN_SYSCALL, MAX_PUBLIC_SYSCALL);
#if defined(__arm__)
  AssertRange(MIN_PRIVATE_SYSCALL, MAX_PRIVATE_SYSCALL);
  AssertRange(MIN_GHOST_SYSCALL, MAX_SYSCALL);
#endif
}

SANDBOX_TEST(SyscallIterator, InvalidSyscalls) {
  static const uint32_t kExpected[] = {
#if defined(__mips__)
    0,
    MIN_SYSCALL - 1,
#endif
    MAX_PUBLIC_SYSCALL + 1,
#if defined(__arm__)
    MIN_PRIVATE_SYSCALL - 1,
    MAX_PRIVATE_SYSCALL + 1,
    MIN_GHOST_SYSCALL - 1,
    MAX_SYSCALL + 1,
#endif
    0x7FFFFFFFu,
    0x80000000u,
    0xFFFFFFFFu,
  };

  for (bool invalid_only : kFalseTrue) {
    size_t i = 0;
    for (SyscallIterator iter(invalid_only); !iter.Done();) {
      uint32_t sysnum = iter.Next();
      if (!SyscallIterator::IsValid(sysnum)) {
        SANDBOX_ASSERT(i < arraysize(kExpected));
        SANDBOX_ASSERT(kExpected[i] == sysnum);
        ++i;
      }
    }
    SANDBOX_ASSERT(i == arraysize(kExpected));
  }
}

}  // namespace

}  // namespace sandbox
