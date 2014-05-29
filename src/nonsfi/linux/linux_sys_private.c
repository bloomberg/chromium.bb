/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This file defines various POSIX-like functions directly using Linux
 * syscalls.  This is analogous to src/untrusted/nacl/sys_private.c, which
 * defines functions using NaCl syscalls directly.
 */

#include <errno.h>
#include <unistd.h>

#include "native_client/src/include/elf32.h"
#include "native_client/src/nonsfi/linux/linux_syscall_structs.h"
#include "native_client/src/nonsfi/linux/linux_syscall_wrappers.h"
#include "native_client/src/nonsfi/linux/linux_syscalls.h"
#include "native_client/src/untrusted/nacl/tls.h"


/*
 * Note that Non-SFI NaCl uses a 4k page size, in contrast to SFI NaCl's
 * 64k page size.
 */
static const int kPageSize = 0x1000;

static int is_error_result(uint32_t result) {
  /*
   * -0x1000 is the highest address that mmap() can return as a result.
   * Linux errno values are less than 0x1000.
   */
  return result > (uint32_t) -0x1000;
}

static uint32_t errno_value_call(uint32_t result) {
  if (is_error_result(result)) {
    errno = -result;
    return -1;
  }
  return result;
}

void _exit(int status) {
  linux_syscall1(__NR_exit_group, status);
  __builtin_trap();
}

long int sysconf(int name) {
  switch (name) {
    case _SC_PAGESIZE:
      return kPageSize;
  }
  errno = EINVAL;
  return -1;
}

void *mmap(void *start, size_t length, int prot, int flags,
           int fd, off_t offset) {
  /*
   * The offset needs shifting by 12 bits for __NR_mmap2 on x86-32.
   * TODO(mseaborn): Make this case work when it is covered by a test.
   */
  if (offset != 0)
    __builtin_trap();

  return (void *) errno_value_call(
      linux_syscall6(__NR_mmap2, (uint32_t) start, length,
                     prot, flags, fd, offset));
}

int write(int fd, const void *buf, size_t count) {
  return errno_value_call(linux_syscall3(__NR_write, fd,
                                         (uint32_t) buf, count));
}

/*
 * This is a stub since _start will call it but we don't want to
 * do the normal initialization.
 */
void __libnacl_irt_init(Elf32_auxv_t *auxv) {
}

int nacl_tls_init(void *thread_ptr) {
#if defined(__i386__)
  struct linux_user_desc desc = {
    .entry_number = -1, /* Allocate new entry */
    .base_addr = (uintptr_t) thread_ptr,
    .limit = -1,
    .seg_32bit = 1,
    .contents = MODIFY_LDT_CONTENTS_DATA,
    .read_exec_only = 0,
    .limit_in_pages = 1,
    .seg_not_present = 0,
    .useable = 1,
  };
  uint32_t result = linux_syscall1(__NR_set_thread_area, (uint32_t) &desc);
  if (result != 0)
    __builtin_trap();
  /*
   * Leave the segment selector's bit 2 (table indicator) as zero because
   * set_thread_area() always allocates an entry in the GDT.
   */
  int privilege_level = 3;
  int gs_segment_selector = (desc.entry_number << 3) + privilege_level;
  __asm__("mov %0, %%gs" : : "r"(gs_segment_selector));
#elif defined(__arm__)
  uint32_t result = linux_syscall1(__NR_ARM_set_tls, (uint32_t) thread_ptr);
  if (result != 0)
    __builtin_trap();
#else
# error Unsupported architecture
#endif
  /*
   * Sanity check: Ensure that the thread pointer reads back correctly.
   * This checks that the set_thread_area() syscall worked and that the
   * thread pointer points to itself, which is required on x86-32.
   */
  if (__nacl_read_tp() != thread_ptr)
    __builtin_trap();
  return 0;
}

void *nacl_tls_get(void) {
  void *result;
#if defined(__i386__)
  __asm__("mov %%gs:0, %0" : "=r"(result));
#elif defined(__arm__)
  __asm__("mrc p15, 0, %0, c13, c0, 3" : "=r"(result));
#endif
  return result;
}
