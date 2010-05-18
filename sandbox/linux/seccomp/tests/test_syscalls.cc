// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <dirent.h>
#include <pthread.h>
#include <pty.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "sandbox_impl.h"


int g_intended_status_fd = -1;

// Declares the wait() status that the test subprocess intends to exit with.
void intend_exit_status(int val) {
  if (g_intended_status_fd != -1) {
    int sent = write(g_intended_status_fd, &val, sizeof(val));
    assert(sent == sizeof(val));
  }
  else {
    printf("Intending to exit with status %i...\n", val);
  }
}


// This is basically a marker to grep for.
#define TEST(name) void name()

TEST(test_dup) {
  StartSeccompSandbox();
  // Test a simple syscall that is marked as UNRESTRICTED_SYSCALL.
  int fd = dup(1);
  assert(fd >= 0);
  int rc = close(fd);
  assert(rc == 0);
}

TEST(test_segfault) {
  // Check that the sandbox's SIGSEGV handler does not stop the
  // process from dying cleanly in the event of a real segfault.
  intend_exit_status(SIGSEGV);
  asm("hlt");
}

TEST(test_exit) {
  intend_exit_status(123 << 8);
  _exit(123);
}

// This has an off-by-three error because it counts ".", "..", and the
// FD for the /proc/self/fd directory.  This doesn't matter because it
// is only used to check for differences in the number of open FDs.
int count_fds() {
  DIR *dir = opendir("/proc/self/fd");
  assert(dir != NULL);
  int count = 0;
  while (1) {
    struct dirent *d = readdir(dir);
    if (d == NULL)
      break;
    count++;
  }
  int rc = closedir(dir);
  assert(rc == 0);
  return count;
}

void *thread_func(void *x) {
  int *ptr = (int *) x;
  *ptr = 123;
  printf("In new thread\n");
  return (void *) 456;
}

TEST(test_thread) {
  StartSeccompSandbox();
  int fd_count1 = count_fds();
  pthread_t tid;
  int x = 999;
  void *result;
  pthread_create(&tid, NULL, thread_func, &x);
  printf("Waiting for thread\n");
  pthread_join(tid, &result);
  assert(result == (void *) 456);
  assert(x == 123);
  // Check that the process has not leaked FDs.
  int fd_count2 = count_fds();
  assert(fd_count2 == fd_count1);
}

int clone_func(void *x) {
  int *ptr = (int *) x;
  *ptr = 124;
  printf("In thread\n");
  // On x86-64, returning from this function calls the __NR_exit_group
  // syscall instead of __NR_exit.
  syscall(__NR_exit, 100);
  // Not reached.
  return 200;
}

#if defined(__i386__)
int get_gs() {
  int gs;
  asm volatile("mov %%gs, %0" : "=r"(gs));
  return gs;
}
#endif

void *get_tls_base() {
  void *base;
#if defined(__x86_64__)
  asm volatile("mov %%fs:0, %0" : "=r"(base));
#elif defined(__i386__)
  asm volatile("mov %%gs:0, %0" : "=r"(base));
#else
#error Unsupported target platform
#endif
  return base;
}

TEST(test_clone) {
  StartSeccompSandbox();
  int fd_count1 = count_fds();
  int stack_size = 0x1000;
  char *stack = (char *) malloc(stack_size);
  assert(stack != NULL);
  int flags = CLONE_VM | CLONE_FS | CLONE_FILES |
    CLONE_SIGHAND | CLONE_THREAD | CLONE_SYSVSEM |
    CLONE_SETTLS | CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID;
  int tid = -1;
  int x = 999;

  // The sandbox requires us to pass CLONE_TLS.  Pass settings that
  // are enough to copy the parent thread's TLS setup.  This allows us
  // to invoke libc in the child thread.
#if defined(__x86_64__)
  void *tls = get_tls_base();
#elif defined(__i386__)
  struct user_desc tls_desc, *tls = &tls_desc;
  tls_desc.entry_number = get_gs() >> 3;
  tls_desc.base_addr = (long) get_tls_base();
  tls_desc.limit = 0xfffff;
  tls_desc.seg_32bit = 1;
  tls_desc.contents = 0;
  tls_desc.read_exec_only = 0;
  tls_desc.limit_in_pages = 1;
  tls_desc.seg_not_present = 0;
  tls_desc.useable = 1;
#else
#error Unsupported target platform
#endif

  int rc = clone(clone_func, (void *) (stack + stack_size), flags, &x,
                 &tid, tls, &tid);
  assert(rc > 0);
  while (tid == rc) {
    syscall(__NR_futex, &tid, FUTEX_WAIT, rc, NULL);
  }
  assert(tid == 0);
  assert(x == 124);
  // Check that the process has not leaked FDs.
  int fd_count2 = count_fds();
  assert(fd_count2 == fd_count1);
}

int uncalled_clone_func(void *x) {
  printf("In thread func, which shouldn't happen\n");
  return 1;
}

TEST(test_clone_disallowed_flags) {
  StartSeccompSandbox();
  int stack_size = 4096;
  char *stack = (char *) malloc(stack_size);
  assert(stack != NULL);
  /* We omit the flags CLONE_SETTLS, CLONE_PARENT_SETTID and
     CLONE_CHILD_CLEARTID, which is disallowed by the sandbox. */
  int flags = CLONE_VM | CLONE_FS | CLONE_FILES |
    CLONE_SIGHAND | CLONE_THREAD | CLONE_SYSVSEM;
  int rc = clone(uncalled_clone_func, (void *) (stack + stack_size),
                 flags, NULL, NULL, NULL, NULL);
  assert(rc == -1);
  assert(errno == EPERM);
}

void *fp_thread(void *x) {
  int val;
  asm("movss %%xmm0, %0" : "=m"(val));
  printf("val=%i\n", val);
  return NULL;
}

TEST(test_fp_regs) {
  StartSeccompSandbox();
  int val = 1234;
  asm("movss %0, %%xmm0" : "=m"(val));
  pthread_t tid;
  pthread_create(&tid, NULL, fp_thread, NULL);
  pthread_join(tid, NULL);
  printf("thread done OK\n");
}

long long read_tsc() {
  long long rc;
  asm volatile(
      "rdtsc\n"
      "mov %%eax, (%0)\n"
      "mov %%edx, 4(%0)\n"
      :
      : "c"(&rc), "a"(-1), "d"(-1));
  return rc;
}

TEST(test_rdtsc) {
  StartSeccompSandbox();
  // Just check that we can do the instruction.
  read_tsc();
}

TEST(test_getpid) {
  int pid1 = getpid();
  StartSeccompSandbox();
  int pid2 = getpid();
  assert(pid1 == pid2);
  // Bypass any caching that glibc's getpid() wrapper might do.
  int pid3 = syscall(__NR_getpid);
  assert(pid1 == pid3);
}

TEST(test_gettid) {
  // glibc doesn't provide a gettid() wrapper.
  int tid1 = syscall(__NR_gettid);
  assert(tid1 > 0);
  StartSeccompSandbox();
  int tid2 = syscall(__NR_gettid);
  assert(tid1 == tid2);
}

void *map_something() {
  void *addr = mmap(NULL, 0x1000, PROT_READ,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert(addr != MAP_FAILED);
  return addr;
}

TEST(test_mmap_disallows_remapping) {
  void *addr = map_something();
  StartSeccompSandbox();
  // Overwriting a mapping that was created before the sandbox was
  // enabled is not allowed.
  void *result = mmap(addr, 0x1000, PROT_READ,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  assert(result == MAP_FAILED);
  assert(errno == EINVAL);
}

TEST(test_mmap_disallows_low_address) {
  StartSeccompSandbox();
  // Mapping pages at low addresses is not allowed because this helps
  // with exploiting buggy kernels.
  void *result = mmap(NULL, 0x1000, PROT_READ,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  assert(result == MAP_FAILED);
  assert(errno == EINVAL);
}

TEST(test_munmap_allowed) {
  StartSeccompSandbox();
  void *addr = map_something();
  int result = munmap(addr, 0x1000);
  assert(result == 0);
}

TEST(test_munmap_disallowed) {
  void *addr = map_something();
  StartSeccompSandbox();
  int result = munmap(addr, 0x1000);
  assert(result == -1);
  assert(errno == EINVAL);
}

TEST(test_mprotect_allowed) {
  StartSeccompSandbox();
  void *addr = map_something();
  int result = mprotect(addr, 0x1000, PROT_READ | PROT_WRITE);
  assert(result == 0);
}

TEST(test_mprotect_disallowed) {
  void *addr = map_something();
  StartSeccompSandbox();
  int result = mprotect(addr, 0x1000, PROT_READ | PROT_WRITE);
  assert(result == -1);
  assert(errno == EINVAL);
}

int get_tty_fd() {
  int master_fd, tty_fd;
  int rc = openpty(&master_fd, &tty_fd, NULL, NULL, NULL);
  assert(rc == 0);
  return tty_fd;
}

TEST(test_ioctl_tiocgwinsz_allowed) {
  int tty_fd = get_tty_fd();
  StartSeccompSandbox();
  int size[2];
  // Get terminal width and height.
  int result = ioctl(tty_fd, TIOCGWINSZ, size);
  assert(result == 0);
}

TEST(test_ioctl_disallowed) {
  int tty_fd = get_tty_fd();
  StartSeccompSandbox();
  // This ioctl call inserts a character into the tty's input queue,
  // which provides a way to send commands to an interactive shell.
  char c = 'x';
  int result = ioctl(tty_fd, TIOCSTI, &c);
  assert(result == -1);
  assert(errno == EINVAL);
}

TEST(test_socket) {
  StartSeccompSandbox();
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  assert(fd == -1);
  // TODO: Make it consistent between i386 and x86-64.
  assert(errno == EINVAL || errno == ENOSYS);
}

TEST(test_open) {
  StartSeccompSandbox();
  int fd = open("/dev/null", O_RDONLY);
  assert(fd >= 0);
  int rc = close(fd);
  assert(rc == 0);
  fd = open("/dev/null", O_WRONLY);
  assert(fd == -1);
  assert(errno == EACCES);
}

TEST(test_access) {
  StartSeccompSandbox();
  int rc = access("/dev/null", R_OK);
  assert(rc == 0);
  rc = access("path-that-does-not-exist", R_OK);
  assert(rc == -1);
  assert(errno == ENOENT);
}

TEST(test_stat) {
  StartSeccompSandbox();
  struct stat st;
  // All file accesses are denied. TODO: Update if the sandbox gets a more
  // fine-grained policy
  int rc = stat("/dev/null", &st);
  assert(rc == -1);
  rc = stat("path-that-does-not-exist", &st);
  assert(rc == -1);
  assert(errno == EACCES);
}

int g_value;

void signal_handler(int sig) {
  g_value = 300;
  printf("In signal handler\n");
}

void sigaction_handler(int sig, siginfo_t *a, void *b) {
  g_value = 300;
  printf("In sigaction handler\n");
}

TEST(test_signal_handler) {
  sighandler_t result = signal(SIGTRAP, signal_handler);
  assert(result != SIG_ERR);

  StartSeccompSandbox();

  // signal() is not allowed inside the sandbox yet.
  result = signal(SIGTRAP, signal_handler);
  assert(result == SIG_ERR);
  assert(errno == ENOSYS);

  g_value = 200;
  asm("int3");
  assert(g_value == 300);
}

TEST(test_sigaction_handler) {
  struct sigaction act;
  act.sa_sigaction = sigaction_handler;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_SIGINFO;
  int rc = sigaction(SIGTRAP, &act, NULL);
  assert(rc == 0);

  StartSeccompSandbox();

  // sigaction() is not allowed inside the sandbox yet.
  rc = sigaction(SIGTRAP, &act, NULL);
  assert(rc == -1);
  assert(errno == ENOSYS);

  g_value = 200;
  asm("int3");
  assert(g_value == 300);
}

TEST(test_blocked_signal) {
  sighandler_t result = signal(SIGTRAP, signal_handler);
  assert(result != SIG_ERR);
  StartSeccompSandbox();

  // Initially the signal should not be blocked.
  sigset_t sigs;
  sigfillset(&sigs);
  int rc = sigprocmask(0, NULL, &sigs);
  assert(rc == 0);
  assert(!sigismember(&sigs, SIGTRAP));

  sigemptyset(&sigs);
  sigaddset(&sigs, SIGTRAP);
  rc = sigprocmask(SIG_BLOCK, &sigs, NULL);
  assert(rc == 0);

  // Check that we can read back the blocked status.
  sigemptyset(&sigs);
  rc = sigprocmask(0, NULL, &sigs);
  assert(rc == 0);
  assert(sigismember(&sigs, SIGTRAP));

  // Check that the signal handler really is blocked.
  intend_exit_status(SIGTRAP);
  asm("int3");
}

TEST(test_sigaltstack) {
  // The sandbox does not support sigaltstack() yet.  Just test that
  // it returns an error.
  StartSeccompSandbox();
  stack_t st;
  st.ss_size = 0x4000;
  st.ss_sp = malloc(st.ss_size);
  assert(st.ss_sp != NULL);
  st.ss_flags = 0;
  int rc = sigaltstack(&st, NULL);
  assert(rc == -1);
  assert(errno == ENOSYS);
}


struct testcase {
  const char *test_name;
  void (*test_func)();
};

struct testcase all_tests[] = {
#include "test-list.h"
  { NULL, NULL },
};

int run_test_forked(struct testcase *test) {
  printf("** %s\n", test->test_name);
  int pipe_fds[2];
  int rc = pipe(pipe_fds);
  assert(rc == 0);
  int pid = fork();
  if (pid == 0) {
    rc = close(pipe_fds[0]);
    assert(rc == 0);
    g_intended_status_fd = pipe_fds[1];

    test->test_func();
    intend_exit_status(0);
    _exit(0);
  }
  rc = close(pipe_fds[1]);
  assert(rc == 0);

  int intended_status;
  int got = read(pipe_fds[0], &intended_status, sizeof(intended_status));
  bool got_intended_status = got == sizeof(intended_status);
  if (!got_intended_status) {
    printf("Test runner: Did not receive intended status\n");
  }

  int status;
  int pid2 = waitpid(pid, &status, 0);
  assert(pid2 == pid);
  if (!got_intended_status) {
    printf("Test returned exit status %i\n", status);
    return 1;
  }
  else if (status != intended_status) {
    printf("Test failed with exit status %i, expected %i\n",
           status, intended_status);
    return 1;
  }
  else {
    return 0;
  }
}

int run_test_by_name(const char *name) {
  struct testcase *test;
  for (test = all_tests; test->test_name != NULL; test++) {
    if (strcmp(name, test->test_name) == 0) {
      printf("Running test %s...\n", name);
      test->test_func();
      printf("OK\n");
      return 0;
    }
  }
  fprintf(stderr, "Test '%s' not found\n", name);
  return 1;
}

int main(int argc, char **argv) {
  if (argc == 2) {
    // Run one test without forking, to aid debugging.
    return run_test_by_name(argv[1]);
  }
  else if (argc > 2) {
    // TODO: run multiple tests.
    fprintf(stderr, "Too many arguments\n");
    return 1;
  }
  else {
    // Run all tests.
    struct testcase *test;
    int failures = 0;
    for (test = all_tests; test->test_name != NULL; test++) {
      failures += run_test_forked(test);
    }
    if (failures == 0) {
      printf("OK\n");
      return 0;
    }
    else {
      printf("%i FAILURE(S)\n", failures);
      return 1;
    }
  }
}
