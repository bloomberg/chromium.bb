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
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "native_client/src/include/elf32.h"
#include "native_client/src/nonsfi/linux/abi_conversion.h"
#include "native_client/src/nonsfi/linux/linux_sys_private.h"
#include "native_client/src/nonsfi/linux/linux_syscall_defines.h"
#include "native_client/src/nonsfi/linux/linux_syscall_structs.h"
#include "native_client/src/nonsfi/linux/linux_syscall_wrappers.h"
#include "native_client/src/nonsfi/linux/linux_syscalls.h"
#include "native_client/src/public/linux_syscalls/poll.h"
#include "native_client/src/public/linux_syscalls/sys/prctl.h"
#include "native_client/src/public/linux_syscalls/sys/socket.h"
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

int nanosleep(const struct timespec *req, struct timespec *rem) {
  struct linux_abi_timespec linux_req;
  nacl_timespec_to_linux_timespec(req, &linux_req);
  struct linux_abi_timespec linux_rem;
  int result = errno_value_call(linux_syscall2(__NR_nanosleep,
                                               (uintptr_t) &linux_req,
                                               (uintptr_t) &linux_rem));

  /*
   * NaCl does not support async signals, so we don't fill out rem on
   * result == -EINTR.
   */
  if (result == 0 && rem != NULL)
    linux_timespec_to_nacl_timespec(&linux_rem, rem);
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

int clock_getres(clockid_t clk_id, struct timespec *res) {
  struct linux_abi_timespec linux_res;
  int result = errno_value_call(
      linux_syscall2(__NR_clock_getres, clk_id, (uintptr_t) &linux_res));
  /* Unlike clock_gettime, clock_getres allows NULL timespecs. */
  if (result == 0 && res != NULL)
    linux_timespec_to_nacl_timespec(&linux_res, res);
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
#if defined(__i386__) || defined(__arm__)
  static const int kPageBits = 12;
  if (offset & ((1 << kPageBits) - 1)) {
    /* An unaligned offset is specified. */
    errno = EINVAL;
    return MAP_FAILED;
  }
  offset >>= kPageBits;

  return (void *) errno_value_call(
      linux_syscall6(__NR_mmap2, (uintptr_t) start, length,
                     prot, flags, fd, offset));
#else
# error Unsupported architecture
#endif
}

int munmap(void *start, size_t length) {
  return errno_value_call(
      linux_syscall2(__NR_munmap, (uintptr_t ) start, length));
}

int mprotect(void *start, size_t length, int prot) {
  return errno_value_call(
      linux_syscall3(__NR_mprotect, (uintptr_t ) start, length, prot));
}

int read(int fd, void *buf, size_t count) {
  return errno_value_call(linux_syscall3(__NR_read, fd,
                                         (uintptr_t) buf, count));
}

int write(int fd, const void *buf, size_t count) {
  return errno_value_call(linux_syscall3(__NR_write, fd,
                                         (uintptr_t) buf, count));
}

int open(char const *pathname, int oflag, ...) {
  mode_t cmode;
  va_list ap;

  if (oflag & O_CREAT) {
    va_start(ap, oflag);
    cmode = va_arg(ap, mode_t);
    va_end(ap);
  } else {
    cmode = 0;
  }

  return errno_value_call(
      linux_syscall3(__NR_open, (uintptr_t) pathname, oflag, cmode));
}

int close(int fd) {
  return errno_value_call(linux_syscall1(__NR_close, fd));
}

ssize_t pread(int fd, void *buf, size_t count, off_t offset) {
  uint32_t offset_low = (uint32_t) offset;
  uint32_t offset_high = offset >> 32;
#if defined(__i386__)
  return errno_value_call(
      linux_syscall5(__NR_pread64, fd, (uintptr_t) buf, count,
                     offset_low, offset_high));
#elif defined(__arm__)
  /*
   * On ARM, a 64-bit parameter has to be in an even-odd register
   * pair. Hence these calls ignore their fourth argument (r3) so that
   * their fifth and sixth make such a pair (r4,r5).
   */
  return errno_value_call(
      linux_syscall6(__NR_pread64, fd, (uintptr_t) buf, count,
                     0  /* dummy */, offset_low, offset_high));
#else
# error Unsupported architecture
#endif
}

ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset) {
  uint32_t offset_low = (uint32_t) offset;
  uint32_t offset_high = offset >> 32;
#if defined(__i386__)
  return errno_value_call(
      linux_syscall5(__NR_pwrite64, fd, (uintptr_t) buf, count,
                     offset_low, offset_high));
#elif defined(__arm__)
  return errno_value_call(
      linux_syscall6(__NR_pwrite64, fd, (uintptr_t) buf, count,
                     0  /* dummy */, offset_low, offset_high));
#else
# error Unsupported architecture
#endif
}

off_t lseek(int fd, off_t offset, int whence) {
#if defined(__i386__) || defined(__arm__)
  uint32_t offset_low = (uint32_t) offset;
  uint32_t offset_high = offset >> 32;
  off_t result;
  int rc = errno_value_call(
      linux_syscall5(__NR__llseek, fd, offset_high, offset_low,
                     (uintptr_t) &result, whence));
  if (rc == -1)
    return -1;
  return result;
#else
# error Unsupported architecture
#endif
}

int dup(int fd) {
  return errno_value_call(linux_syscall1(__NR_dup, fd));
}

int dup2(int oldfd, int newfd) {
  return errno_value_call(linux_syscall2(__NR_dup2, oldfd, newfd));
}

int fstat(int fd, struct stat *st) {
  struct linux_abi_stat64 linux_st;
  int rc = errno_value_call(
      linux_syscall2(__NR_fstat64, fd, (uintptr_t) &linux_st));
  if (rc == -1)
    return -1;
  linux_stat_to_nacl_stat(&linux_st, st);
  return 0;
}

int stat(const char *file, struct stat *st) {
  struct linux_abi_stat64 linux_st;
  int rc = errno_value_call(
      linux_syscall2(__NR_stat64, (uintptr_t) file, (uintptr_t) &linux_st));
  if (rc == -1)
    return -1;
  linux_stat_to_nacl_stat(&linux_st, st);
  return 0;
}

int lstat(const char *file, struct stat *st) {
  struct linux_abi_stat64 linux_st;
  int rc = errno_value_call(
      linux_syscall2(__NR_lstat64, (uintptr_t) file, (uintptr_t) &linux_st));
  if (rc == -1)
    return -1;
  linux_stat_to_nacl_stat(&linux_st, st);
  return 0;
}

int mkdir(const char *path, mode_t mode) {
  return errno_value_call(linux_syscall2(__NR_mkdir, (uintptr_t) path, mode));
}

int rmdir(const char *path) {
  return errno_value_call(linux_syscall1(__NR_rmdir, (uintptr_t) path));
}

int chdir(const char *path) {
  return errno_value_call(linux_syscall1(__NR_chdir, (uintptr_t) path));
}

char *getcwd(char *buffer, size_t len) {
  int rc = errno_value_call(
      linux_syscall2(__NR_getcwd, (uintptr_t) buffer, len));
  if (rc == -1)
    return NULL;
  return buffer;
}

int unlink(const char *path) {
  return errno_value_call(linux_syscall1(__NR_unlink, (uintptr_t) path));
}

int truncate(const char *path, off_t length) {
  uint32_t length_low = (uint32_t) length;
  uint32_t length_high = length >> 32;
#if defined(__i386__)
  return errno_value_call(
      linux_syscall3(__NR_truncate64, (uintptr_t) path,
                     length_low, length_high));
#elif defined(__arm__)
  /*
   * On ARM, a 64-bit parameter has to be in an even-odd register
   * pair. Hence these calls ignore their second argument (r1) so that
   * their third and fourth make such a pair (r2,r3).
   */
  return errno_value_call(
      linux_syscall4(__NR_truncate64, (uintptr_t) path,
                     0  /* dummy */, length_low, length_high));
#else
# error Unsupported architecture
#endif
}

int link(const char *oldpath, const char *newpath) {
  return errno_value_call(
      linux_syscall2(__NR_link, (uintptr_t) oldpath, (uintptr_t) newpath));
}

int rename(const char *oldpath, const char* newpath) {
  return errno_value_call(
      linux_syscall2(__NR_rename, (uintptr_t) oldpath, (uintptr_t) newpath));
}

int symlink(const char *oldpath, const char* newpath) {
  return errno_value_call(
      linux_syscall2(__NR_symlink, (uintptr_t) oldpath, (uintptr_t) newpath));
}

int chmod(const char *path, mode_t mode) {
  return errno_value_call(
      linux_syscall2(__NR_chmod, (uintptr_t) path, mode));
}

int access(const char *path, int amode) {
  return errno_value_call(
      linux_syscall2(__NR_access, (uintptr_t) path, amode));
}

int readlink(const char *path, char *buf, int bufsize) {
  return errno_value_call(
      linux_syscall3(__NR_readlink, (uintptr_t) path,
                     (uintptr_t) buf, bufsize));
}

int fcntl(int fd, int cmd, ...) {
  if (cmd == F_GETFL || cmd == F_GETFD) {
    return errno_value_call(linux_syscall2(__NR_fcntl64, fd, cmd));
  }
  if (cmd == F_SETFL || cmd == F_SETFD) {
    va_list ap;
    va_start(ap, cmd);
    int32_t arg = va_arg(ap, int32_t);
    va_end(ap);
    return errno_value_call(linux_syscall3(__NR_fcntl64, fd, cmd, arg));
  }
  /* We only support the fcntl commands above. */
  errno = EINVAL;
  return -1;
}

int fork(void) {
  /* Set SIGCHLD as flag so we can wait. */
  return errno_value_call(
      linux_syscall5(__NR_clone, LINUX_SIGCHLD,
                     0 /* stack */, 0 /* ptid */, 0 /* tls */, 0 /* ctid */));
}

int isatty(int fd) {
  struct linux_termios term;
  return errno_value_call(
      linux_syscall3(__NR_ioctl, fd, LINUX_TCGETS, (uintptr_t) &term)) == 0;
}

int pipe(int pipefd[2]) {
  return errno_value_call(linux_syscall1(__NR_pipe, (uintptr_t) pipefd));
}

int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
  return errno_value_call(
      linux_syscall3(__NR_poll, (uintptr_t) fds, nfds, timeout));
}

int prctl(int option, uintptr_t arg2, uintptr_t arg3,
          uintptr_t arg4, uintptr_t arg5) {
  return errno_value_call(
      linux_syscall5(__NR_prctl, option, arg2, arg3, arg4, arg5));
}

#if defined(__i386__)
/* On x86-32 Linux, socket related syscalls are defined by using socketcall. */

static uintptr_t socketcall(int op, void *args) {
  return errno_value_call(
      linux_syscall2(__NR_socketcall, op, (uintptr_t) args));
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
  uint32_t args[] = { sockfd, (uintptr_t) msg, flags };
  return socketcall(SYS_RECVMSG, args);
}

ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags) {
  uint32_t args[] = { sockfd, (uintptr_t) msg, flags };
  return socketcall(SYS_SENDMSG, args);
}

int shutdown(int sockfd, int how) {
  uint32_t args[] = { sockfd, how };
  return socketcall(SYS_SHUTDOWN, args);
}

int socketpair(int domain, int type, int protocol, int sv[2]) {
  uint32_t args[] = { domain, type, protocol, (uintptr_t) sv };
  return socketcall(SYS_SOCKETPAIR, args);
}

#elif defined(__arm__)
/* On ARM Linux, socketcall is not defined. Instead use each syscall. */

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
  return errno_value_call(
      linux_syscall3(__NR_recvmsg, sockfd, (uintptr_t) msg, flags));
}

ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags) {
  return errno_value_call(
      linux_syscall3(__NR_sendmsg, sockfd, (uintptr_t) msg, flags));
}

int shutdown(int sockfd, int how) {
  return errno_value_call(linux_syscall2(__NR_shutdown, sockfd, how));
}

int socketpair(int domain, int type, int protocol, int sv[2]) {
  return errno_value_call(
      linux_syscall4(__NR_socketpair, domain, type, protocol, (uintptr_t) sv));
}

#else
# error Unsupported architecture
#endif

pid_t waitpid(pid_t pid, int *status, int options) {
  return errno_value_call(
      linux_syscall4(__NR_wait4, pid, (uintptr_t) status, options,
                     0  /* rusage */));
}

int linux_sigaction(int signum, const struct linux_sigaction *act,
                    struct linux_sigaction *oldact) {
  /* This is the size of Linux kernel's sigset_t. */
  const int kSigsetSize = 8;
  /*
   * We do not support returning from a signal handler invoked by a
   * real time signal. To support this, we need to set sa_restorer
   * when it is not set by the caller, but we probably will not need
   * this. See the following for how we do it.
   * https://code.google.com/p/linux-syscall-support/source/browse/trunk/lss/linux_syscall_support.h
   */
  return errno_value_call(
      linux_syscall4(__NR_rt_sigaction, signum,
                     (uintptr_t) act, (uintptr_t) oldact, kSigsetSize));
}

sighandler_t signal(int signum, sighandler_t handler) {
  struct linux_sigaction sa;
  memset(&sa, 0, sizeof(sa));
  /*
   * In Linux's sigaction, sa_sigaction and sa_handler share the same
   * memory region by union.
   */
  sa.sa_sigaction = (void (*)(int, linux_siginfo_t *, void *)) handler;
  sa.sa_flags = LINUX_SA_RESTART;
  sigemptyset(&sa.sa_mask);
  sigaddset(&sa.sa_mask, signum);
  struct linux_sigaction osa;
  int result = linux_sigaction(signum, &sa, &osa);
  if (result != 0)
    return SIG_ERR;
  return (sighandler_t) osa.sa_sigaction;
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
