// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <pthread.h>
#include <pty.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "sandbox_impl.h"


// This is basically a marker to grep for.
#define TEST(name) void name()

void *thread_func(void *x) {
  int *ptr = (int *) x;
  *ptr = 123;
  printf("In new thread\n");
  return (void *) 456;
}

TEST(test_thread) {
  StartSeccompSandbox();
  pthread_t tid;
  int x;
  void *result;
  pthread_create(&tid, NULL, thread_func, &x);
  printf("Waiting for thread\n");
  pthread_join(tid, &result);
  assert(result == (void *) 456);
  assert(x == 123);
}

int clone_func(void *x) {
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
  int rc = clone(clone_func, (void *) (stack + stack_size), flags, NULL,
                 NULL, NULL, NULL);
  assert(rc == -1);
  assert(errno == EPERM);
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


struct testcase {
  const char *test_name;
  void (*test_func)();
};

struct testcase all_tests[] = {
#include "test-list.h"
  { NULL, NULL },
};

void run_test_forked(struct testcase *test) {
  printf("** %s\n", test->test_name);
  int pid = fork();
  if (pid == 0) {
    test->test_func();
    _exit(0);
  }
  int status;
  waitpid(pid, &status, 0);
  if (status != 0) {
    printf("Test failed with exit status %i\n", status);
    exit(1);
  }
}

int run_test_by_name(const char *name) {
  struct testcase *test;
  for (test = all_tests; test->test_name != NULL; test++) {
    if (strcmp(name, test->test_name) == 0) {
      test->test_func();
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
    for (test = all_tests; test->test_name != NULL; test++) {
      run_test_forked(test);
    }
  }
  return 0;
}
