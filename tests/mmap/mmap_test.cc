/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"


#define PRINT_HEADER 0
#define TEXT_LINE_SIZE 1024

const char *example_file;

/*
 * function failed(testname, msg)
 *   print failure message and exit with a return code of -1
 */

bool failed(const char *testname, const char *msg) {
  printf("TEST FAILED: %s: %s\n", testname, msg);
  return false;
}

/*
 * function passed(testname, msg)
 *   print success message
 */

bool passed(const char *testname, const char *msg) {
  printf("TEST PASSED: %s: %s\n", testname, msg);
  return true;
}


static jmp_buf g_jmp_buf;

static void exception_handler(struct NaClExceptionContext *context) {
  /* We got an exception as expected.  Return from the handler. */
  int rc = NACL_SYSCALL(exception_clear_flag)();
  assert(rc == 0);
  longjmp(g_jmp_buf, 1);
}

static void assert_addr_is_unreadable(volatile char *addr) {
  /*
   * TODO(mseaborn): It would be better to use Valgrind annotations to
   * turn off the memory access checks temporarily.
   */
  if (getenv("RUNNING_ON_VALGRIND") != NULL) {
    fprintf(stderr, "Skipping assert_addr_is_unreadable() under Valgrind\n");
    return;
  }
  if (getenv("RUNNING_ON_ASAN") != NULL) {
    fprintf(stderr, "Skipping assert_addr_is_unreadable() under ASan\n");
    return;
  }

  int rc = NACL_SYSCALL(exception_handler)(exception_handler, NULL);
  assert(rc == 0);
  if (!setjmp(g_jmp_buf)) {
    char value = *addr;
    /* If we reach here, the assertion failed. */
    fprintf(stderr, "Address %p was readable, and contained %i\n",
            addr, value);
    exit(1);
  }
  /*
   * Clean up: Unregister the exception handler so that we do not
   * accidentally return through g_jmp_buf if an exception occurs.
   */
  rc = NACL_SYSCALL(exception_handler)(NULL, NULL);
  assert(rc == 0);
}


/*
 * function test*()
 *
 *   Simple tests follow below.  Each test may call one or more
 *   of the functions above.  They all have a boolean return value
 *   to indicate success (all tests passed) or failure (one or more
 *   tests failed)  Order matters - the parent should call
 *   test1() before test2(), and so on.
 */

bool test1() {
  int size = 64 * 1024;  /* we need 64K */
  void *zeroes;
  // test simple mmap
  void *res = NULL;
  int rv;

  printf("test1\n");

  res = mmap(res, size, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
  if (0 >= res) /* define MAP_FAILED */
    return false;
  printf("mmap done\n");
  zeroes = malloc(size);
  memset(zeroes, 0, size);
  if (memcmp(res, zeroes, size)) {
    printf("memcmp failed\n");
    return false;
  }

  rv = munmap(res, 1024);
  if (rv != 0) {
    printf("munmap failed\n");
    return false;
  }
  printf("munmap good\n");
  return true;
}



/*
 *   Verify that munmap of executable text pages will fail.
 */

bool test2() {
  int rv;

  printf("test2\n");

  /* text starts at 64K */
  rv = munmap(reinterpret_cast<void*>(1<<16), (size_t) (1<<16));

  /*
   * if the munmap succeeds, we probably won't be able to continue to
   * run....
   */
  printf("munmap returned %d\n", rv);

  if (-1 == rv && EINVAL == errno) {
    printf("munmap good (failed as expected)\n");
    return true;
  }
  return false;
}

/*
 *   Verify that mmap into the NULL pointer guard page will fail.
 */

bool test3() {
  void  *res;

  printf("test3\n");
  res = mmap(static_cast<void*>(0), (size_t) (1 << 16),
             PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, 0, 0);
  printf("res = %p\n", res);
  if (MAP_FAILED == res && EINVAL == errno) {
    printf("mmap okay\n");
    return true;
  } else {
    printf("mmap should not have succeeded, or failed with wrong error\n");
    return false;
  }
}

/*
 *   Verify that mmap/MAP_FIXED with a non-page-aligned address will fail.
 */

bool test4() {
  printf("test4\n");
  /* First reserve some address space in which to perform the experiment. */
  char *alloc = (char *) mmap(NULL, 1 << 16, PROT_NONE,
                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (MAP_FAILED == alloc) {
    printf("mmap failed\n");
    return false;
  }

  void *res = mmap((void *) (alloc + 0x100), 1 << 16, PROT_READ,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  if (MAP_FAILED == res && EINVAL == errno) {
    printf("mmap gave an error as expected\n");
    return true;
  } else {
    printf("mmap should not have succeeded, or failed with wrong error\n");
    return false;
  }
}

/*
 *   Verify that the last page in a file can be mmapped when the file's
 *   size is not a multiple of the page size.
 *   Tests for http://code.google.com/p/nativeclient/issues/detail?id=836
 */

bool test_mmap_end_of_file() {
  printf("test_mmap_end_of_file\n");
  int fd = open(example_file, O_RDONLY);
  if (fd < 0) {
    printf("open() failed\n");
    return false;
  }
  /*
   * TODO(mseaborn): Extend this to 0x20000 or larger and check that
   * the later 64k pages are inaccessible.
   * See http://code.google.com/p/nativeclient/issues/detail?id=824
   */
  size_t map_size = 0x10000;
  /*
   * First, map an address range as readable+writable, in order to
   * check that these mappings are properly overwritten by the second
   * mmap() call.
   */
  char *alloc = (char *) mmap(NULL, map_size, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert(alloc != MAP_FAILED);
  char *addr = (char *) mmap((void *) alloc, map_size, PROT_READ,
                             MAP_PRIVATE | MAP_FIXED, fd, 0);
  assert(addr == alloc);
  int rc = close(fd);
  if (rc != 0) {
    printf("close() failed\n");
    return false;
  }
  /* To avoid line ending issues, this test file contains no newlines. */
  const char *expected_data =
    "Test file for mmapping, less than a page in size.";
  if (memcmp(alloc, expected_data, strlen(expected_data)) != 0) {
    printf("Unexpected contents: %s\n", alloc);
    return false;
  }
  /* The first 4k page should be readable. */
  for (size_t i = strlen(expected_data); i < 0x1000; i++) {
    if (alloc[i] != 0) {
      printf("Unexpected padding byte: %i\n", alloc[i]);
      return false;
    }
  }
  /*
   * Addresses beyond the first 4k should not be readable.  This is
   * one case where we expose a 4k page size rather than a 64k page
   * size.  Windows forces us to expose a mixture of 4k and 64k page
   * sizes for end-of-file mappings.
   */
  assert_addr_is_unreadable(alloc + 0x1000);
  assert_addr_is_unreadable(alloc + 0x2000);
  /*
   * TODO(mseaborn): Also check the following:
   *   assert_addr_is_unreadable(alloc + 0x10000);
   * See http://code.google.com/p/nativeclient/issues/detail?id=824
   */
  rc = munmap(alloc, map_size);
  if (rc != 0) {
    printf("munmap() failed\n");
    return false;
  }
  return true;
}

/*
 * function testSuite()
 *
 *   Run through a complete sequence of file tests.
 *
 * returns true if all tests succeed.  false if one or more fail.
 */

bool testSuite() {
  bool ret = true;
  // The order of executing these tests matters!
  ret &= test1();
  ret &= test2();
  ret &= test3();
  ret &= test4();

  ret &= test_mmap_end_of_file();

  return ret;
}

/*
 * main entry point.
 *
 * run all tests and call system exit with appropriate value
 *   0 - success, all tests passed.
 *  -1 - one or more tests failed.
 */

int main(const int argc, const char *argv[]) {
  bool passed;

  if (argc != 2) {
    printf("Error: Expected test file arg\n");
    return 1;
  }
  example_file = argv[1];

  // run the full test suite
  passed = testSuite();

  if (passed) {
    printf("All tests PASSED\n");
    exit(0);
  } else {
    printf("One or more tests FAILED\n");
    exit(-1);
  }
}
