/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// NaCl inter-module communication primitives.
//
// This file implements common parts of IMC for "unix like systems" (i.e. not
// used on Windows).

// TODO(shiki): Perhaps this file should go into a platform-specific directory
// (posix? unixlike?)  We have a little convention going where mac/linux stuff
// goes in the linux directory and is referenced by the mac build but that's a
// little sloppy.

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>

#include <algorithm>

#include "native_client/src/include/atomic_ops.h"

#include "native_client/src/shared/imc/nacl_imc.h"

namespace nacl {

namespace {

// The pathname prefix for memory objects created by CreateMemoryObject().
const char kShmPrefix[] = "/google-nacl-shm-";

}  // namespace

bool WouldBlock() {
  return (errno == EAGAIN) ? true : false;
}

int GetLastErrorString(char* buffer, size_t length) {
#if NACL_LINUX
  // Note some Linux distributions provide only GNU version of strerror_r().
  if (buffer == NULL || length == 0) {
    errno = ERANGE;
    return -1;
  }
  char* message = strerror_r(errno, buffer, length);
  if (message != buffer) {
    size_t message_bytes = strlen(message) + 1;
    length = std::min(message_bytes, length);
    memmove(buffer, message, length);
    buffer[length - 1] = '\0';
  }
  return 0;
#else
  return strerror_r(errno, buffer, length);
#endif
}

Handle CreateMemoryObject(size_t length) {
  static AtomicWord memory_object_count;
  if (0 == length) {
    return -1;
  }

  char name[PATH_MAX];
  for (;;) {
    snprintf(name, sizeof name, "%s-%u.%u", kShmPrefix,
             getpid(),
             static_cast<uint32_t>(AtomicIncrement(&memory_object_count, 1)));
    int m = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0666);
    if (0 <= m) {
      if (ftruncate(m, length) == -1) {
        close(m);
        m = -1;
      }
      shm_unlink(name);
      return m;
    }
    if (errno != EEXIST) {
      return -1;
    }
  }
}

void* Map(void* start, size_t length, int prot, int flags,
          Handle memory, off_t offset) {
  static const int kPosixProt[4] = {
    PROT_NONE,
    PROT_READ,
    PROT_WRITE,
    PROT_READ | PROT_WRITE
  };

  int adjusted = 0;
  if (flags & kMapShared) {
    adjusted |= MAP_SHARED;
  }
  if (flags & kMapPrivate) {
    adjusted |= MAP_PRIVATE;
  }
  if (flags & kMapFixed) {
    adjusted |= MAP_FIXED;
  }
  return mmap(start, length, kPosixProt[prot & 3], adjusted, memory, offset);
}

int Unmap(void* start, size_t length) {
  return munmap(start, length);
}

}  // namespace nacl
