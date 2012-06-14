// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <linux/unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/seccomp-bpf/util.h"

#define ERR EPERM

// We don't expect our sandbox to do anything useful yet. So, we will fail
// almost immediately. For now, force the code to continue running. The
// following line should be removed as soon as the sandbox is starting to
// actually enforce restrictions in a meaningful way:
#define _exit(x) do { } while (0)

static playground2::Sandbox::ErrorCode evaluator(int sysno) {
  switch (sysno) {
  #if defined(__NR_accept)
    case __NR_accept: case __NR_accept4:
#endif
    case __NR_alarm:
    case __NR_brk:
    case __NR_clock_gettime:
    case __NR_close:
    case __NR_dup: case __NR_dup2:
    case __NR_epoll_create: case __NR_epoll_ctl: case __NR_epoll_wait:
    case __NR_exit: case __NR_exit_group:
    case __NR_fcntl:
#if defined(__NR_fcntl64)
    case __NR_fcntl64:
#endif
    case __NR_fdatasync:
    case __NR_fstat:
#if defined(__NR_fstat64)
    case __NR_fstat64:
#endif
    case __NR_ftruncate:
    case __NR_futex:
    case __NR_getdents: case __NR_getdents64:
    case __NR_getegid:
#if defined(__NR_getegid32)
    case __NR_getegid32:
#endif
    case __NR_geteuid:
#if defined(__NR_geteuid32)
    case __NR_geteuid32:
#endif
    case __NR_getgid:
#if defined(__NR_getgid32)
    case __NR_getgid32:
#endif
    case __NR_getitimer: case __NR_setitimer:
#if defined(__NR_getpeername)
    case __NR_getpeername:
#endif
    case __NR_getpid: case __NR_gettid:
#if defined(__NR_getsockname)
    case __NR_getsockname:
#endif
    case __NR_gettimeofday:
    case __NR_getuid:
#if defined(__NR_getuid32)
    case __NR_getuid32:
#endif
#if defined(__NR__llseek)
    case __NR__llseek:
#endif
    case __NR_lseek:
    case __NR_nanosleep:
    case __NR_pipe: case __NR_pipe2:
    case __NR_poll:
    case __NR_pread64: case __NR_preadv:
    case __NR_pwrite64: case __NR_pwritev:
    case __NR_read: case __NR_readv:
    case __NR_restart_syscall:
    case __NR_set_robust_list:
    case __NR_rt_sigaction:
#if defined(__NR_sigaction)
    case __NR_sigaction:
#endif
#if defined(__NR_signal)
    case __NR_signal:
#endif
    case __NR_rt_sigprocmask:
#if defined(__NR_sigprocmask)
    case __NR_sigprocmask:
#endif
#if defined(__NR_shutdown)
    case __NR_shutdown:
#endif
    case __NR_rt_sigreturn:
#if defined(__NR_sigreturn)
    case __NR_sigreturn:
#endif
#if defined(__NR_socketpair)
    case __NR_socketpair:
#endif
    case __NR_time:
    case __NR_uname:
    case __NR_write: case __NR_writev:
      return playground2::Sandbox::SB_ALLOWED;

  // The following system calls are temporarily permitted. This must be
  // tightened later. But we currently don't implement enough of the sandboxing
  // API to do so.
  // As is, this sandbox isn't exactly safe :-/
#if defined(__NR_sendmsg)
  case __NR_sendmsg: case __NR_sendto:
  case __NR_recvmsg: case __NR_recvfrom:
  case __NR_getsockopt: case __NR_setsockopt:
#elif defined(__NR_socketcall)
  case __NR_socketcall:
#endif
#if defined(__NR_shmat)
  case __NR_shmat: case __NR_shmctl: case __NR_shmdt: case __NR_shmget:
#elif defined(__NR_ipc)
  case __NR_ipc:
#endif
#if defined(__NR_mmap2)
  case __NR_mmap2:
#else
  case __NR_mmap:
#endif
#if defined(__NR_ugetrlimit)
  case __NR_ugetrlimit:
#endif
  case __NR_getrlimit:
  case __NR_ioctl:
  case __NR_prctl:
  case __NR_clone:
  case __NR_munmap: case __NR_mprotect: case __NR_madvise:
  case __NR_remap_file_pages:
      return playground2::Sandbox::SB_ALLOWED;

  // Everything that isn't explicitly allowed is denied.
  default:
    return (playground2::Sandbox::ErrorCode)ERR;
  }
}

static void *threadFnc(void *arg) {
  return arg;
}

static void *sendmsgStressThreadFnc(void *arg) {
  if (arg) { }
  static const int repetitions = 100;
  static const int kNumFds = 3;
  for (int rep = 0; rep < repetitions; ++rep) {
    int fds[2 + kNumFds];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds)) {
      perror("socketpair()");
      _exit(1);
    }
    size_t len = 4;
    char buf[4];
    if (!playground2::Util::sendFds(fds[0], "test", 4,
                                    fds[1], fds[1], fds[1], -1) ||
        !playground2::Util::getFds(fds[1], buf, &len,
                                   fds+2, fds+3, fds+4, NULL) ||
        len != 4 ||
        memcmp(buf, "test", len) ||
        write(fds[2], "demo", 4) != 4 ||
        read(fds[0], buf, 4) != 4 ||
        memcmp(buf, "demo", 4)) {
      perror("sending/receiving of fds");
      _exit(1);
    }
    for (int i = 0; i < 2+kNumFds; ++i) {
      if (close(fds[i])) {
        perror("close");
        _exit(1);
      }
    }
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  if (argc) { }
  if (argv) { }
  int proc_fd = open("/proc", O_RDONLY|O_DIRECTORY);
  if (playground2::Sandbox::supportsSeccompSandbox(proc_fd) !=
      playground2::Sandbox::STATUS_AVAILABLE) {
    perror("sandbox");
    _exit(1);
  }
  playground2::Sandbox::setProcFd(proc_fd);
  playground2::Sandbox::setSandboxPolicy(evaluator, NULL);
  playground2::Sandbox::startSandbox();

  // Check that we can create threads
  pthread_t thr;
  if (!pthread_create(&thr, NULL, threadFnc,
                      reinterpret_cast<void *>(0x1234))) {
    void *ret;
    pthread_join(thr, &ret);
    if (ret != reinterpret_cast<void *>(0x1234)) {
      perror("clone() failed");
      _exit(1);
    }
  } else {
    perror("clone() failed");
    _exit(1);
  }

  // Check that we handle restart_syscall() without dieing. This is a little
  // tricky to trigger. And I can't think of a good way to verify whether it
  // actually executed.
  signal(SIGALRM, SIG_IGN);
  const struct itimerval tv = { { 0, 0 }, { 0, 5*1000 } };
  const struct timespec tmo = { 0, 100*1000*1000 };
  setitimer(ITIMER_REAL, &tv, NULL);
  nanosleep(&tmo, NULL);

  // Check that we can query the size of the stack, but that all other
  // calls to getrlimit() fail.
  if (((errno = 0), !getrlimit(RLIMIT_STACK, NULL)) || errno != EFAULT ||
      ((errno = 0), !getrlimit(RLIMIT_CORE,  NULL)) || errno != ERR) {
    perror("getrlimit()");
    _exit(1);
  }

  // Check that we can query TCGETS and TIOCGWINSZ, but no other ioctls().
  if (((errno = 0), !ioctl(2, TCGETS,     NULL)) || errno != EFAULT ||
      ((errno = 0), !ioctl(2, TIOCGWINSZ, NULL)) || errno != EFAULT ||
      ((errno = 0), !ioctl(2, TCSETS,     NULL)) || errno != ERR) {
    perror("ioctl()");
    _exit(1);
  }

  // Check that prctl() can manipulate the dumpable flag, but nothing else.
  if (((errno = 0), !prctl(PR_GET_DUMPABLE))    || errno ||
      ((errno = 0),  prctl(PR_SET_DUMPABLE, 1)) || errno ||
      ((errno = 0), !prctl(PR_SET_SECCOMP,  0)) || errno != ERR) {
    perror("prctl()");
    _exit(1);
  }

  // Check that we can send and receive file handles.
  int fds[3];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds)) {
    perror("socketpair()");
    _exit(1);
  }
  size_t len = 4;
  char buf[4];
  if (!playground2::Util::sendFds(fds[0], "test", 4, fds[1], -1) ||
      !playground2::Util::getFds(fds[1], buf, &len, fds+2, NULL) ||
      len != 4 ||
      memcmp(buf, "test", len) ||
      write(fds[2], "demo", 4) != 4 ||
      read(fds[0], buf, 4) != 4 ||
      memcmp(buf, "demo", 4) ||
      close(fds[0]) ||
      close(fds[1]) ||
      close(fds[2])) {
    perror("sending/receiving of fds");
    _exit(1);
  }

  // Check whether SysV IPC works.
  int shmid;
  void *addr;
  if ((shmid = shmget(IPC_PRIVATE, 4096, IPC_CREAT|0600)) < 0 ||
      (addr = shmat(shmid, NULL, 0)) == reinterpret_cast<void *>(-1) ||
      shmdt(addr) ||
      shmctl(shmid, IPC_RMID, NULL)) {
    perror("sysv IPC");
    _exit(1);
  }

  // Print a message so that the user can see the sandbox is activated.
  time_t tm = time(NULL);
  printf("Sandbox has been started at %s", ctime(&tm));

  // Stress-test the sendmsg() code
  static const int kSendmsgStressNumThreads = 10;
  pthread_t sendmsgStressThreads[kSendmsgStressNumThreads];
  for (int i = 0; i < kSendmsgStressNumThreads; ++i) {
    if (pthread_create(sendmsgStressThreads + i, NULL,
                       sendmsgStressThreadFnc, NULL)) {
      perror("pthread_create");
      _exit(1);
    }
  }
  for (int i = 0; i < kSendmsgStressNumThreads; ++i) {
    pthread_join(sendmsgStressThreads[i], NULL);
  }

  return 0;
}
