/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_NONSFI_LINUX_LINUX_SYSCALL_STRUCTS_H_
#define NATIVE_CLIENT_SRC_NONSFI_LINUX_LINUX_SYSCALL_STRUCTS_H_ 1

#include <stdint.h>

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

#endif
