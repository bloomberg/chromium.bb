/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl tests for simple syscalls
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "native_client/src/include/nacl_assert.h"
#include "native_client/src/trusted/service_runtime/include/sys/nacl_syscalls.h"

#define PRINT_HEADER 0
#define TEXT_LINE_SIZE 1024

/*
 * TODO(sbc): remove this test once gethostname() declaration
 * gets added to the prebuilt newlib toolchain
 */
#ifndef __GLIBC__
extern "C" int gethostname(char *name, size_t len);
#endif

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

// TODO(sbc): remove this restriction once glibc is updated to
// use dev-filename-0.2:
// https://code.google.com/p/nativeclient/issues/detail?id=3709
#if defined(__GLIBC__)

bool test_chdir() {
  return passed("test_chdir", "all");
}

bool test_mkdir_rmdir(const char *test_file) {
  return passed("test_mkdir_rmdir", "all");
}

bool test_getcwd() {
  return passed("test_getcwd", "all");
}

bool test_unlink(const char *test_file) {
  return passed("test_unlink", "all");
}

#else

// Simple test that chdir returns zero for '.'.  chdir gets more
// significant testing as part of the getcwd test.
bool test_chdir() {
  int rtn = chdir(".");
  ASSERT_EQ_MSG(rtn, 0, "chdir() failed");

  return passed("test_chdir", "all");
}

bool test_mkdir_rmdir(const char *test_file) {
  // Use a temporary direcotry name alongside the test_file which
  // was passed in.
  char dirname[PATH_MAX];
  strncpy(dirname, test_file, PATH_MAX);

  // Strip off the trailing filename
  char *basename_start = strrchr(dirname, '/');
  if (basename_start == NULL) {
    basename_start = strrchr(dirname, '\\');
    ASSERT_NE_MSG(basename_start, NULL, "test_file contains no dir seperator");
  }
  basename_start[1] = '\0';

  ASSERT(strlen(dirname) + 6 < PATH_MAX);
  strncat(dirname, "tmpdir", 6);

  // Attempt to remove the directory in case it already exists
  // from a previous test run.
  if (rmdir(dirname) != 0)
    ASSERT_EQ_MSG(errno, ENOENT, "rmdir() failed to cleanup existing dir");

  char cwd[PATH_MAX];
  char *cwd_rtn = getcwd(cwd, PATH_MAX);
  ASSERT_EQ_MSG(cwd_rtn, cwd, "getcwd() failed");

  int rtn = mkdir(dirname, S_IRUSR | S_IWUSR | S_IXUSR);
  ASSERT_EQ_MSG(rtn, 0, "mkdir() failed");

  rtn = rmdir(dirname);
  ASSERT_EQ_MSG(rtn, 0, "rmdir() failed");

  rtn = rmdir("This file does not exist");
  ASSERT_EQ_MSG(rtn, -1, "rmdir() failed to fail");
  ASSERT_EQ_MSG(errno, ENOENT, "rmdir() failed to generate ENOENT");

  return passed("test_mkdir_rmdir", "all");
}

bool test_getcwd() {
  char dirname[PATH_MAX] = { '\0' };
  char newdir[PATH_MAX] = { '\0' };
  char parent[PATH_MAX] = { '\0' };

  char *rtn = getcwd(dirname, PATH_MAX);
  ASSERT_EQ_MSG(rtn, dirname, "getcwd() failed to return dirname");
  ASSERT_NE_MSG(strlen(dirname), 0, "getcwd() failed to set valid dirname");

  // Calculate parent folder.
  strncpy(parent, dirname, PATH_MAX);
  char *basename_start = strrchr(parent, '/');
  if (basename_start == NULL) {
    basename_start = strrchr(parent, '\\');
    ASSERT_NE_MSG(basename_start, NULL, "test_file contains no dir seperator");
  }
  basename_start[0] = '\0';

  int retcode = chdir("..");
  ASSERT_EQ_MSG(retcode, 0, "chdir() failed");

  rtn = getcwd(newdir, PATH_MAX);
  ASSERT_EQ_MSG(rtn, newdir, "getcwd() failed");

  ASSERT_MSG(strcmp(newdir, parent) == 0, "getcwd() failed after chdir");
  retcode = chdir(dirname);
  ASSERT_EQ_MSG(retcode, 0, "chdir() failed");

  rtn = getcwd(dirname, 2);
  ASSERT_EQ_MSG(rtn, NULL, "getcwd() failed to fail");
  ASSERT_EQ_MSG(errno, ERANGE, "getcwd() failed to generate ERANGE");

  return passed("test_getcwd", "all");
}

bool test_unlink(const char *test_file) {
  int rtn;
  struct stat buf;
  char buffer[PATH_MAX];
  snprintf(buffer, PATH_MAX, "%s.tmp", test_file);
  buffer[PATH_MAX - 1] = '\0';

  int fd = open(buffer, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  ASSERT_MSG(fd >= 0, "open() failed");

  rtn = close(fd);
  ASSERT_EQ_MSG(rtn, 0, "close() failed");

  rtn = stat(buffer, &buf);
  ASSERT_EQ_MSG(rtn, 0, "stat() failed");

  rtn = unlink(buffer);
  ASSERT_EQ_MSG(rtn, 0, "unlink() failed");

  rtn = stat(buffer, &buf);
  ASSERT_NE_MSG(rtn, 0, "unlink() failed to remove file");

  rtn = unlink(buffer);
  ASSERT_NE_MSG(rtn, 0, "unlink() failed to fail");

  return passed("test_unlink", "all");
}

#endif  // !__GLIBC__

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

bool test_stat(const char *test_file) {
  struct stat buf;

  // Test incoming test_file for read and write permission.
  ASSERT_EQ(stat(test_file, &buf), 0);
  ASSERT_MSG(buf.st_mode & S_IRUSR, "stat() failed to report S_IRUSR");
  ASSERT_MSG(buf.st_mode & S_IWUSR, "stat() failed to report S_IWUSR");

  // Test a new read-only file
  // The current unlink() implemenation in the sel_ldr for Windows
  // doesn't support removing read-only files.
  // TODO(sbc): enable this part of the test once this gets fixed.
#if 0
  char buffer[PATH_MAX];
  snprintf(buffer, PATH_MAX, "%s.readonly", test_file);
  buffer[PATH_MAX - 1] = '\0';
  unlink(buffer);
  ASSERT_EQ(stat(buffer, &buf), -1);
  int fd = open(buffer, O_RDWR | O_CREAT, S_IRUSR);
  ASSERT_NE(fd, -1);
  ASSERT_EQ(close(fd), 0);
  ASSERT_EQ(stat(buffer, &buf), 0);
  ASSERT_MSG(buf.st_mode & S_IRUSR, "stat() failed to report S_IRUSR");
  ASSERT_MSG(!(buf.st_mode & S_IWUSR), "S_IWUSR report for a read-only file");
#endif

  // Windows doesn't support the concept of write only files,
  // so we can't test this case.

  return passed("test_stat", "all");
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
  // Linux's open() (unsandboxed) does not allow O_RDWR on a directory.
  // TODO(mseaborn): sel_ldr should reject O_RDWR on a directory too.
  if (!PNACL_UNSANDBOXED) {
    fd = open(".", O_RDWR);
    if (fd == -1)
      return failed(testname, "open(., O_RDWR)");
    ret_val = close(fd);
    if (ret_val == -1)
      return failed(testname, "close(., O_RDWR)");
  }

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
  // Linux's read() (unsandboxed) does not reject this buffer size.
  if (!PNACL_UNSANDBOXED) {
    ret_val = read(fd, out_char, -1);
    if (ret_val != -1)
      return failed(testname, "read(fd, out_char, -1)");
    // bad address
    ASSERT_EQ(errno, EFAULT);
    if (EFAULT != errno)
      return failed(testname, "EFAULT != errno");
  }

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
  // Linux's write() (unsandboxed) does not reject this buffer size.
  if (!PNACL_UNSANDBOXED) {
    ret_val = write(fd, out_char, -1);
    if (ret_val != -1)
      return failed(testname, "write(fd, out_char, -1)");
    // bad address
    if (EFAULT != errno)
      return failed(testname, "EFAULT != errno");
  }

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

bool test_readdir(const char *test_file) {
  // TODO(mseaborn): Implement listing directories for unsandboxed mode.
  if (PNACL_UNSANDBOXED)
    return true;

  // Read the directory containing the test file
  char dirname[PATH_MAX];
  strncpy(dirname, test_file, PATH_MAX);

  // Strip off the trailing filename
  char *basename = strrchr(dirname, '/');
  if (basename == NULL) {
    basename = strrchr(dirname, '\\');
    ASSERT_NE_MSG(basename, NULL, "test_file contains no dir seperator");
  }
  basename[0] = '\0';
  basename++;

  // Read the directory listing and verify that the test_file is
  // present.
  int found = 0;
  DIR *d = opendir(dirname);
  ASSERT_NE_MSG(d, NULL, "opendir failed");
  int count = 0;
  struct dirent *ent;
  while (1) {
    ent = readdir(d);
    if (!ent)
      break;
    if (!strcmp(ent->d_name, basename))
      found = 1;
    count++;
  }
  ASSERT_EQ_MSG(1, found, "failed to find test file in directory listing");

  // Rewind directory and verify that the number of elements
  // matches the previous count.
  rewinddir(d);
  while (readdir(d))
    count--;
  ASSERT_EQ_MSG(0, count, "readdir after rewinddir was inconsistent");

  ASSERT_EQ(0, closedir(d));
  return passed("test_readdir", "all");
}

// isatty returns 1 for TTY descriptors and 0 on error (setting errno)
bool test_isatty(const char *test_file) {
  // TODO(mseaborn): Implement isatty() for unsandboxed mode.
  if (PNACL_UNSANDBOXED)
    return true;
  // TODO(sbc): isatty() in glibc is not yet hooked up to the IRT
  // interfaces. Remove this conditional once this gets addressed:
  // https://code.google.com/p/nativeclient/issues/detail?id=3709
#if defined(__GLIBC__)
  return true;
#endif

  // Open a regular file that check that it is not a tty.
  int fd = open(test_file, O_RDONLY);
  ASSERT_NE_MSG(fd, -1, "open() failed");
  errno = 0;
  ASSERT_EQ_MSG(isatty(fd), 0, "isatty returned non-zero");
  ASSERT_EQ_MSG(errno, ENOTTY, "isatty failed to set errno to ENOTTY");
  close(fd);

  // Verify that isatty() on closed file returns 0 and sets errno to EBADF
  errno = 0;
  ASSERT_EQ_MSG(isatty(fd), 0, "isatty returned non-zero");
  ASSERT_EQ_MSG(errno, EBADF, "isatty failed to set errno to EBADF");

  // On Linux opening /dev/ptmx always returns a TTY file descriptor.
  fd = open("/dev/ptmx", O_RDWR);
  if (fd >= 0) {
    errno = 0;
    ASSERT_EQ(isatty(fd), 1);
    ASSERT_EQ(errno, 0);
    close(fd);
  }

  return passed("test_isatty", "all");
}

/*
 * Not strictly speaking a syscall, but we have a 'fake' implementation
 * that we want to test.
 */
bool test_gethostname() {
  char hostname[256];
  ASSERT_EQ(gethostname(hostname, 1), -1);
#ifdef __GLIBC__
  // glibc only provides a stub gethostbyname() that returns
  // ENOSYS in all cases.
  ASSERT_EQ(errno, ENOSYS);
#else
  ASSERT_EQ(errno, ENAMETOOLONG);

  errno = 0;
  ASSERT_EQ(gethostname(hostname, 256), 0);
  ASSERT_EQ(errno, 0);
  ASSERT_EQ(strcmp(hostname, "naclhost"), 0);
#endif
  return passed("test_gethostname", "all");
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
  ret &= test_stat(test_file);
  ret &= test_open(test_file);
  ret &= test_close(test_file);
  ret &= test_read(test_file);
  ret &= test_write(test_file);
  ret &= test_lseek(test_file);
  ret &= test_unlink(test_file);
  ret &= test_chdir();
  ret &= test_getcwd();
  ret &= test_mkdir_rmdir(test_file);
  ret &= test_readdir(test_file);
  ret &= test_gethostname();
  ret &= test_isatty(test_file);
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
