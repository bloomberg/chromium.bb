/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_NONSFI_LINUX_LINUX_SYSCALL_STRUCTS_H_
#define NATIVE_CLIENT_SRC_NONSFI_LINUX_LINUX_SYSCALL_STRUCTS_H_ 1

#include <signal.h>
#include <stdint.h>

#if defined(__i386__)

/*
 * x86 segment descriptor accepted by Linux's set_thread_area(),
 * modify_ldt() and clone() syscalls on x86-32.  See:
 * https://www.kernel.org/doc/man-pages/online/pages/man2/modify_ldt.2.html
 * https://www.kernel.org/doc/man-pages/online/pages/man2/set_thread_area.2.html
 *
 * See "Legacy Segment Descriptors" in Volume 2 of the AMD64 Architecture
 * Programmer's Manual for the segment descriptor format used by x86
 * hardware, which has a different layout from the struct below.
 */
struct linux_user_desc {
  uint32_t entry_number;
  uint32_t base_addr;
  uint32_t limit;
  unsigned int seg_32bit : 1;
  unsigned int contents : 2;
  unsigned int read_exec_only : 1;
  unsigned int limit_in_pages : 1;
  unsigned int seg_not_present : 1;
  unsigned int useable : 1;
};

#define MODIFY_LDT_CONTENTS_DATA 0

static inline struct linux_user_desc create_linux_user_desc(
    int allocate_new_entry, void *thread_ptr) {
  int entry_number = -1;   /* Allocate new entry */
  if (!allocate_new_entry) {
    uint32_t gs;
    __asm__ __volatile__("mov %%gs, %0" : "=r"(gs));
    entry_number = (gs & 0xffff) >> 3;
  }
  struct linux_user_desc desc = {
    .entry_number = entry_number,
    .base_addr = (uintptr_t) thread_ptr,
    .limit = -1,
    .seg_32bit = 1,
    .contents = MODIFY_LDT_CONTENTS_DATA,
    .read_exec_only = 0,
    .limit_in_pages = 1,
    .seg_not_present = 0,
    .useable = 1,
  };
  return desc;
}

#endif

typedef struct {
  int si_signo;
  int si_errno;
  int si_code;
} linux_siginfo_t;

struct linux_sigaction {
  void (*sa_sigaction)(int, linux_siginfo_t *, void *);
  int sa_flags;
  void *sa_restorer;
  /*
   * Though newlib's sigset_t is bigger than linux kernel's, their
   * layout is compatible. We do not define linux_sigset_t and helper
   * functions such as sigaddset by ourselves.
   */
  sigset_t sa_mask;
};

#endif
