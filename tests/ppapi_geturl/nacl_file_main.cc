// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Tests nacl_file library.
//

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/untrusted/ppapi/nacl_file.h"

using ppapi_proxy::DebugPrintf;

struct ThreadInfo {
  bool threaded;
  unsigned int rseed;
};

// Open and read a valid existing file into a buffer.
// Return the number of bytes read.
int test_nacl_file_read_into_buffer(ThreadInfo* info,
                                    int buffer_size,
                                    char* buffer) {
  // Valid url should be open and read without errors.
  int fd = open("ppapi_geturl_success.html", O_RDONLY);
  CHECK(fd >= 0);
  int bytes_read = read(fd, buffer, buffer_size);
  CHECK(215 == bytes_read);
  CHECK(strstr(buffer, "TEST PASSED") == buffer);
  close(fd);
  if (!info->threaded) {
    // This part of the test only works when run in a single thread.  In a
    // multi-threaded test, between the close (above) and read (below), another
    // thread could recycle the fd.  In a single threaded test, the read below
    // should fail because the fd was just closed.
    bytes_read = read(fd, buffer, buffer_size);
    CHECK(-1 == bytes_read);
  }
  return bytes_read;
}

// Open and read a valid existing file.
void test_nacl_file_basic_read(ThreadInfo* info) {
  char buffer[256];
  test_nacl_file_read_into_buffer(info, sizeof(buffer), buffer);
}

// Do some basic lseek tests on a valid file.
void test_nacl_file_basic_lseek() {
  char buffer[256];
  // Some of these seeks are relative, and may depend on results of previous
  // seeks.
  int fd = open("ppapi_geturl_success.html", O_RDONLY);
  CHECK(fd >= 0);
  off_t start = lseek(fd, 0, SEEK_SET);
  CHECK(0 == start);
  off_t end = lseek(fd, 0, SEEK_END);
  CHECK(end > 0);
  off_t start2 = lseek(fd, -end, SEEK_CUR);
  CHECK(0 == start2);
  off_t end2 = lseek(fd, end, SEEK_CUR);
  CHECK(end2 == end);
  off_t end3 = lseek(fd, end, SEEK_SET);
  CHECK(end3 == end);
  off_t pos1 = lseek(fd, -2, SEEK_CUR);
  off_t pos2 = lseek(fd, 0, SEEK_CUR);
  CHECK(pos1 == pos2);
  lseek(fd, 1, SEEK_CUR);
  off_t end4 = lseek(fd, 1, SEEK_CUR);
  CHECK(end4 == end);
  off_t beyond = lseek(fd, 100, SEEK_END);
  CHECK((end + 100) == beyond);
  off_t invalid_offset = lseek(fd, -2, SEEK_SET);
  CHECK(-1 == invalid_offset);
  CHECK(EINVAL == errno);
  off_t beyond2 = lseek(fd, 0, SEEK_CUR);
  CHECK(beyond2 == beyond);
  off_t badfd = lseek(1234567, 0, SEEK_SET);
  CHECK(-1 == badfd);
  CHECK(EBADF == errno);
  // Seek back to start and try same read test again.
  CHECK(lseek(fd, 0, SEEK_SET) == 0);
  int bytes_read = read(fd, buffer, sizeof(buffer));
  CHECK(215 == bytes_read);
  CHECK(strstr(buffer, "TEST PASSED") == buffer);
  off_t off = lseek(fd, 0, SEEK_END);
  CHECK(215 == off);
  close(fd);
}

// Briefly test the fopen() and friends interface built on top of lower-
// level open() and friends.
void test_nacl_file_fopen() {
  FILE* f = fopen("ppapi_geturl_success.html", "r");
  CHECK(NULL != f);
  char buffer[256];
  char* pbuffer = fgets(buffer, sizeof(buffer), f);
  CHECK(pbuffer == buffer);
  fclose(f);
  int compare = strcmp(buffer, "TEST PASSED\n");
  CHECK(0 == compare);
}

// Do a blizzard of small, random file operations.  Randomly seek
// and read a byte many times, comparing the read result against the
// whole buffer that was read earlier.  Do this against a random number of
// simultaniously temporarily open files.
void test_nacl_file_many_times(ThreadInfo* info) {
  int bytes_read;
  char buffer[256];
  bytes_read = test_nacl_file_read_into_buffer(info, sizeof(buffer), buffer);
  const int kNumOuterLoop = 20;
  const int kNumMaxFilesOpen = 10;
  const int kNumRandomReads = 500;
  for (int i = 0; i < kNumOuterLoop; ++i) {
    int file[kNumMaxFilesOpen];
    int num_open = rand_r(&info->rseed) % (kNumMaxFilesOpen - 1) + 1;
    CHECK(0 < num_open);
    CHECK(kNumMaxFilesOpen > num_open);
    for (int j = 0; j < num_open; ++j) {
      file[j] = open("ppapi_geturl_success.html", O_RDONLY);
      CHECK(-1 != file[j]);
    }
    int num_reads = rand_r(&info->rseed) % kNumRandomReads;
    for (int k = 0; k < num_reads; ++k) {
      char a_byte;
      const int which_file = rand_r(&info->rseed) % num_open;
      const int which_byte = rand_r(&info->rseed) % bytes_read;
      int r = lseek(file[which_file], which_byte, SEEK_SET);
      CHECK(which_byte == r);
      size_t num_read = read(file[which_file], &a_byte, sizeof(a_byte));
      CHECK(1 == num_read);
      CHECK(a_byte == buffer[which_byte]);
    }
    for (int j = 0; j < num_open; ++j) {
      close(file[j]);
    }
  }
}

// Test nacl_file library functions that override standard POSIX file I/O
// functions and are intended to have the same behavior.
void* test_nacl_file_thread(void* user_data) {
  ThreadInfo* info = reinterpret_cast<ThreadInfo*>(user_data);
  test_nacl_file_basic_read(info);
  test_nacl_file_basic_lseek();
  test_nacl_file_basic_read(info);
  // Cross origin url should fail on open.
  int fd = open("http://www.google.com/robots.txt", O_RDONLY);
  CHECK(-1 == fd);
  // Invalid url should fail on open.
  fd = open("doesnotexist.html", O_RDONLY);
  CHECK(-1 == fd);
#if defined(__native_client__)
  // Open for write should fail in NaCl.
  int no_fd = open("ppapi_geturl_success.html", O_CREAT);
  CHECK(-1 == no_fd);
  CHECK(EACCES == errno);
#endif
  test_nacl_file_fopen();
  test_nacl_file_many_times(info);
  return NULL;
}

// Main entry point to test nacl_file library. It is assumed that before calling
// this function, LoadUrl() has been invoked for each file to be tested, and
// that the completion callback has been reached.
void test_nacl_file() {
  const char *nacl_enable_ppapi_dev = getenv("NACL_ENABLE_PPAPI_DEV");
  int enabled = 0;
  // Skip test if NACL_ENABLE_PPAPI_DEV is unset or set to 0.
  if (NULL != nacl_enable_ppapi_dev)
    enabled = strtol(nacl_enable_ppapi_dev, (char **) 0, 0);
  if (enabled == 0) {
    DebugPrintf("Skipping NaCl File test, NACL_ENABLE_PPAPI_DEV not set.\n");
    return;
  }
  const int kNumThreads = 8;
  pthread_t thread[kNumThreads];
  ThreadInfo info;
  unsigned int rseed = 123456;
  info.threaded = false;
  info.rseed = rand_r(&rseed);
  // Run the test once from the main thread, as the only thread.
  test_nacl_file_thread(&info);
  info.threaded = true;
  // The following test creates threads to test basic file I/O.
  // This way we can also ensure that the abstraction is not broken if one
  // tries to open/manipulate/close the same NaClFile more then once.
  for (int i = 0; i < kNumThreads; ++i) {
    info.rseed = rand_r(&rseed);
    int p = pthread_create(&thread[i], NULL, test_nacl_file_thread, &info);
    CHECK(0 == p);
  }
  // Run the test from the main thread again, this time competing with
  // the other threads.
  info.rseed = rand_r(&rseed);
  test_nacl_file_thread(&info);
  // Give the threads a chance to start racing before joining them.
  usleep(100000);
  for (int i = 0; i < kNumThreads; ++i) {
    pthread_join(thread[i], NULL);
  }
}
