// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Tests NaClFile interface.
//
// Since main() is not invoked if this is loaded as a trusted dll, to sanity
// check outside of NaCl/Chrome, use
//   g++ -o test nacl_file_main.cc -pthread; ./test
//

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#if defined(__native_client__)
#include <nacl/nacl_check.h>
#include <nacl/nacl_file.h>
#include <nacl/ppruntime.h>
#include "native_client/src/shared/ppapi_proxy/utility.h"
#else
#include <assert.h>
#define CHECK assert
#define DebugPrintf printf
#endif

// Test NaClFile library functions that override standard POSIX file IO
// functions and are intended to have the same behavior.
void* test_nacl_file(void* /*unused*/) {
  // TODO(nfullagar): enable this when checking in nacl_file implementation
  // for open(), close() and read().
#if !defined(__native_client__)
  // Valid url should be open and read without errors.
  int fd = open("ppapi_geturl_success.html", O_RDONLY);
  DebugPrintf("*** Test NaClFile: valid fd=%d\n", fd);
  CHECK(fd >=0);

  char buffer[256];
  int bytes_read = read(fd, buffer, sizeof(buffer));
  DebugPrintf("*** Test NaClFile: valid bytes_read=%d\n", bytes_read);
  CHECK(bytes_read == 215);
  DebugPrintf("*** Test NaClFile: valid body:\n%s\n", buffer);
  CHECK(strcmp(buffer, "TEST PASSED") == 10);

  close(fd);
  bytes_read = read(fd, buffer, sizeof(buffer));
  DebugPrintf("*** Test NaClFile: valid after close fd=%d\n", fd);
  CHECK(bytes_read == -1);

  // Cross origin url should fail on open.
  fd = open("http://www.google.com/robots.txt", O_RDONLY);
  DebugPrintf("*** Test NaClFile: cross-origin fd=%d\n", fd);
  CHECK(fd == -1);

  // Invalid url should fail on open.
  fd = open("doesnotexist.html", O_RDONLY);
  DebugPrintf("*** Test NaClFile: invalid fd=%d\n", fd);
  CHECK(fd == -1);
#endif
  return NULL;
}

// NaClFile open() blocks until the url is asynchronously loaded by a previous
// call to LoadUrl(). At this point LoadUrl does not take a callback, so we
// do not have a reliable way of knowing when it completes. Thus,
// open() has to be called off the main thread because we are not supposed
// to block it.
//
// This test, therefore, creates new threads to test open() and friends.
// This way we can also ensure that the abstraction is not broken if one
// tries to open/manipulate/close the same NaClFile more then once.
//
// We use main() to drive the tests to avoid additional triggers based on
// JavaScript and PPAPI scripting interfaces, the way we do for testing
// PPAPI-based url loading. This allows us to keep the tests for the two
// interfaces (PPAPI and NaClFile) separate while still covering all url
// manipulation use cases in one sample test.
int main() {
  int status = 0;
#if defined(__native_client__)
  status = ppapi_proxy::PluginMain();
#endif
  if (status == 0) {
    pthread_t thread1, thread2;
    int err1 = pthread_create(&thread1, NULL, test_nacl_file, NULL);
    int err2 = pthread_create(&thread2, NULL, test_nacl_file, NULL);
    status = err1 + err2;
    if (status == 0) {
      pthread_join(thread1, NULL);
      pthread_join(thread2, NULL);
    }
  }
  return status;
}
