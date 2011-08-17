/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl tests for simple syscalls
 */

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/nacl_syscalls.h>
#include <unistd.h>

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
  // test sched_yield
  if (sched_yield()) {
    printf("sched_yield failed\n");
    return false;
  }
  return true;
}

bool test2() {
  // test sysconf
  int rv;
  rv = sysconf(_SC_NPROCESSORS_ONLN);
  if (rv == -1) {
    printf("failed to get nprocs\n");
    return false;
  }
  if (rv < 1) {
    printf("got strange number of processors: %d\n", rv);
    return false;
  }
  // test sysconf on an invalid input.
  rv = sysconf(-1);
  if (rv != -1) {
    printf("succeeded on unsupported\n");
    return false;
  }
  return true;
}

// open() returns the new file descriptor, or -1 if an error occurred
bool test_open(const char *test_file) {
  int fd;
  const char *testname = "test_open";

  // file OK, flag OK
  fd = open(test_file, O_RDONLY);
  if (fd == -1)
    return failed(testname, "open(test_file, O_RDONLY)");
  close(fd);

  errno = 0;
  // file does not exist, flags OK
  fd = open("testdata/file_none.txt", O_RDONLY);
  if (fd != -1)
    return failed(testname, "open(testdata/file_none.txt, O_RDONLY)");
  // no such file or directory
  if (ENOENT != errno)
    return failed(testname, "ENOENT != errno");
  close(fd);

  // file OK, flags OK, mode OK
  fd = open(test_file, O_WRONLY, S_IRUSR);
  if (fd == -1)
    return failed(testname, "open(test_file, O_WRONLY, S_IRUSR)");
  close(fd);

  // too many args
  fd = open(test_file, O_RDWR, S_IRUSR, O_APPEND);
  if (fd == -1)
    return failed(testname, "open(test_file, O_RDWR, S_IRUSR, O_APPEND)");
  close(fd);

  // directory OK
  fd = open(".", O_RDONLY);
  if (fd == -1)
    return failed(testname, "open(., O_RDONLY)");
  close(fd);

  errno = 0;
  // directory does not exist
  fd = open("nosuchdir", O_RDONLY);
  if (fd != -1)
    return failed(testname, "open(nosuchdir, O_RDONLY)");
  // no such file or directory
  if (ENOENT != errno)
    return failed(testname, "ENOENT != errno");
  close(fd);

  return passed(testname, "all");
}

// close() returns 0 on success, -1 on error
bool test_close(const char *test_file) {
  int fd;
  int ret_val;
  const char *testname = "test_close";

  // file OK
  fd = open(test_file, O_RDWR);
  if (fd == -1)
    return failed(testname, "open(test_file, O_RDWR)");
  ret_val = close(fd);
  if (ret_val == -1)
    return failed(testname, "close(test_file, O_RDWR)");

  // file OK
  fd = open(test_file, O_RDWR);
  if (fd == -1)
    return failed(testname, "open(test_file, O_RDWR)");
  // close on wrong fd not OK
  errno = 0;
  ret_val = close(fd+1);
  if (ret_val != -1)
    return failed(testname, "close(fd+1)");
  // bad file number
  if (EBADF != errno)
    return failed(testname, "EBADF != errno");
  ret_val = close(fd);
  if (ret_val == -1)
    return failed(testname, "close(test_file, O_RDWR)");

  // file not OK
  fd = open("file_none.txt", O_WRONLY);
  if (fd != -1)
    return failed(testname, "open(file_none.txt, O_WRONLY)");
  errno = 0;
  ret_val = close(fd);
  if (ret_val == 0)
    return failed(testname, "close(file_none.txt, O_WRONLY)");
  // bad file number
  if (EBADF != errno)
    return failed(testname, "EBADF != errno");

  // directory OK
  fd = open(".", O_RDWR);
  if (fd == -1)
    return failed(testname, "open(., O_RDWR)");
  ret_val = close(fd);
  if (ret_val == -1)
    return failed(testname, "close(., O_RDWR)");

  // directory not OK
  fd = open("nosuchdir", O_RDWR);
  if (fd != -1)
    return failed(testname, "open(nosuchdir, O_RDWR)");
  errno = 0;
  ret_val = close(fd);
  if (ret_val == 0)
    return failed(testname, "close(nosuchdir, O_RDWR)");
  // bad file number
  if (EBADF != errno)
    return failed(testname, "EBADF != errno");

  return passed(testname, "all");
}

// read() returns the number of bytes read on success (0 indicates end
// of file), -1 on error
bool test_read(const char *test_file) {
  int fd;
  int ret_val;
  char out_char[5];
  const char *testname = "test_read";

  fd = open(test_file, O_RDONLY);
  if (fd == -1)
    return failed(testname, "open(test_file, O_RDONLY)");

  // fd OK, buffer OK, count OK
  ret_val = read(fd, out_char, 1);
  if (ret_val == -1)
    return failed(testname, "read(fd, out_char, 1)");

  errno = 0;
  // fd not OK, buffer OK, count OK
  ret_val = read(-1, out_char, 1);
  if (ret_val != -1)
    return failed(testname, "read(-1, out_char, 1)");
  // bad file number
  if (EBADF != errno)
    return failed(testname, "EBADF != errno");

  errno = 0;
  // fd OK, buffer OK, count not OK
  ret_val = read(fd, out_char, -1);
  if (ret_val != -1)
    return failed(testname, "read(fd, out_char, -1)");
  // bad address
  if (EFAULT != errno)
    return failed(testname, "EFAULT != errno");

  errno = 0;
  // fd not OK, buffer OK, count not OK
  ret_val = read(-1, out_char, -1);
  if (ret_val != -1)
    return failed(testname, "read(-1, out_char, -1)");
  // bad descriptor
  if (EBADF != errno)
    return failed(testname, "EBADF != errno");

  // fd OK, buffer OK, count 0
  ret_val = read(fd, out_char, 0);
  if (ret_val != 0)
    return failed(testname, "read(fd, out_char, 0)");

  // read 10, but only 3 are left
  ret_val = read(fd, out_char, 10);
  if (ret_val != 4)
    return failed(testname, "read(fd, out_char, 10)");

  // EOF
  ret_val = read(fd, out_char, 10);
  if (ret_val != 0)
    return failed(testname, "read(fd, out_char, 10)");

  close(fd);
  return passed(testname, "all");
}


// write() returns the number of bytes written on success, -1 on error
bool test_write(const char *test_file) {
  int fd;
  int ret_val;
  char out_char[] = "12";
  const char *testname = "test_write";

  fd = open(test_file, O_WRONLY);
  if (fd == -1)
    return failed(testname, "open(test_file, O_WRONLY)");

  // all params OK
  ret_val = write(fd, out_char, 2);
  if (ret_val != 2)
    return failed(testname, "write(fd, out_char, 2)");

  errno = 0;
  // invalid count
  ret_val = write(fd, out_char, -1);
  if (ret_val != -1)
    return failed(testname, "write(fd, out_char, -1)");
  // bad address
  if (EFAULT != errno)
    return failed(testname, "EFAULT != errno");

  errno = 0;
  // invalid fd
  ret_val = write(-1, out_char, 2);
  if (ret_val != -1)
    return failed(testname, "write(-1, out_char, 2)");
  // bad address
  if (EBADF != errno)
    return failed(testname, "EBADF != errno");

  close(fd);
  return passed(testname, "all");
}

// lseek returns the resulting offset location in bytes from the
// beginning of the file, -1 on error
bool test_lseek(const char *test_file) {
  int fd;
  int ret_val;
  char out_char;
  const char *testname = "test_lseek";

  fd = open(test_file, O_RDWR);
  if (fd == -1)
    return failed(testname, "open(test_file, O_RDWR)");

  ret_val = lseek(fd, 2, SEEK_SET);
  if (ret_val != 2)
    return failed(testname, "lseek(fd, 2, SEEK_SET)");

  errno = 0;
  ret_val = lseek(-1, 1, SEEK_SET);
  if (ret_val != -1)
    return failed(testname, "lseek(-1, 1, SEEK_SET)");
  // bad file number
  if (EBADF != errno)
    return failed(testname, "EBADF != errno");

  ret_val = read(fd, &out_char, 1);
  if ((ret_val != 1) || (out_char != '3'))
    return failed(testname, "read(fd, &out_char, 1) #1");

  ret_val = lseek(fd, 1, SEEK_CUR);
  if (ret_val != 4)
    return failed(testname, "lseek(fd, 1, SEEK_CUR)");

  ret_val = read(fd, &out_char, 1);
  if ((ret_val != 1) || (out_char != '5'))
    return failed(testname, "read(fd, &out_char, 1) #2");

  ret_val = lseek(fd, -1, SEEK_CUR);
  if (ret_val != 4)
    return failed(testname, "lseek(fd, -1, SEEK_CUR)");

  ret_val = read(fd, &out_char, 1);
  if ((ret_val != 1) || (out_char != '5'))
    return failed(testname, "read(fd, &out_char, 1) #3");

  ret_val = lseek(fd, -2, SEEK_END);
  if (ret_val != 3)
    return failed(testname, "lseek(fd, -2, SEEK_END)");

  ret_val = read(fd, &out_char, 1);
  if ((ret_val != 1) || (out_char != '4'))
    return failed(testname, "read(fd, &out_char, 1) #4");

  ret_val = lseek(fd, 4, SEEK_END);
  // lseek allows for positioning beyond the EOF
  if (ret_val != 9)
    return failed(testname, "lseek(fd, 4, SEEK_END)");

  ret_val = lseek(fd, 4, SEEK_SET);
  if (ret_val != 4)
    return failed(testname, "lseek(fd, 4, SEEK_SET)");

  errno = 0;
  ret_val = lseek(fd, 4, SEEK_END + 3);
  if (ret_val != -1)
    return failed(testname, "lseek(fd, 4, SEEK_END + 3)");
  // invalid argument
  if (EINVAL != errno)
    return failed(testname, "EINVAL != errno");

  errno = 0;
  ret_val = lseek(fd, -40, SEEK_SET);
  if (ret_val != -1)
    return failed(testname, "lseek(fd, -40, SEEK_SET)");
  // invalid argument
  if (EINVAL != errno)
    return failed(testname, "EINVAL != errno");

  ret_val = read(fd, &out_char, 1);
  if ((ret_val != 1) || (out_char == '4'))
    return failed(testname, "read(fd, &out_char, 1) #5");

  close(fd);
  return passed(testname, "all");
}

/*
 * function testSuite()
 *
 *   Run through a complete sequence of file tests.
 *
 * returns true if all tests succeed.  false if one or more fail.
 */

bool testSuite(const char *test_file) {
  bool ret = true;
  // The order of executing these tests matters!
  ret &= test1();
  ret &= test2();
  ret &= test_open(test_file);
  ret &= test_close(test_file);
  ret &= test_read(test_file);
  ret &= test_write(test_file);
  ret &= test_lseek(test_file);
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
    printf("Please specify the test file name\n");
    exit(-1);
  }
  // run the full test suite
  passed = testSuite(argv[1]);

  if (passed) {
    printf("All tests PASSED\n");
    exit(0);
  } else {
    printf("One or more tests FAILED\n");
    exit(-1);
  }
}
