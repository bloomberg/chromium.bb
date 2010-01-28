/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#define PRINT_HEADER 0
#define TEXT_LINE_SIZE 1024

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

  if (0 != rv) {
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
  if (MAP_FAILED == res) {
    printf("mmap okay\n");
    return true;
  } else {
    printf("mmap should not have succeeded\n");
    return false;
  }
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
