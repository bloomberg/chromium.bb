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
#include <sched.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "native_client/src/include/elf32.h"
#include "native_client/src/nonsfi/linux/abi_conversion.h"
#include "native_client/src/nonsfi/linux/linux_syscall_structs.h"
#include "native_client/src/nonsfi/linux/linux_syscall_wrappers.h"
#include "native_client/src/nonsfi/linux/linux_syscalls.h"
#include "native_client/src/untrusted/nacl/tls.h"


/*
 * Note that Non-SFI NaCl uses a 4k page size, in contrast to SFI NaCl's
 * 64k page size.
 */
static const int kPageSize = 0x1000;

static uintptr_t errno_value_call(uintptr_t result) {
  if (linux_is_error_result(result)) {
    errno = -result;
    return -1;
  }
  return result;
}

void _exit(int status) {
  linux_syscall1(__NR_exit_group, status);
  __builtin_trap();
}

int gettimeofday(struct timeval *tv, void *tz) {
  struct linux_abi_timeval linux_tv;
  int result = errno_value_call(
      linux_syscall2(__NR_gettimeofday, (uintptr_t) &linux_tv, 0));
  if (result == 0)
    linux_timeval_to_nacl_timeval(&linux_tv, tv);
  return result;
}

int clock_gettime(clockid_t clk_id, struct timespec *ts) {
  struct linux_abi_timespec linux_ts;
  int result = errno_value_call(
      linux_syscall2(__NR_clock_gettime, clk_id, (uintptr_t) &linux_ts));
  if (result == 0)
    linux_timespec_to_nacl_timespec(&linux_ts, ts);
  return result;
}

int sched_yield(void) {
  return errno_value_call(linux_syscall0(__NR_sched_yield));
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
      linux_syscall6(__NR_mmap2, (uintptr_t) start, length,
                     prot, flags, fd, offset));
}

int write(int fd, const void *buf, size_t count) {
  return errno_value_call(linux_syscall3(__NR_write, fd,
                                         (uintptr_t) buf, count));
}

/*
 * This is a stub since _start will call it but we don't want to
 * do the normal initialization.
 */
void __libnacl_irt_init(Elf32_auxv_t *auxv) {
}

int nacl_tls_init(void *thread_ptr) {
#if defined(__i386__)
  struct linux_user_desc desc = create_linux_user_desc(
      1 /* allocate_new_entry */, thread_ptr);
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
