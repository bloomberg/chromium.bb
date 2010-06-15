/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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

#if NACL_LINUX && !defined(NACL_STANDALONE)
#include "chrome/renderer/renderer_sandbox_support_linux.h"
#endif

namespace nacl {

namespace {

// The pathname or SHM-namespace prefixes for memory objects created
// by CreateMemoryObject().
const char kShmTempPrefix[] = "/tmp/google-nacl-shm-";
const char kShmOpenPrefix[] = "/google-nacl-shm-";

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

static AtomicWord memory_object_count = 0;

static int TryShmOrTempOpen(size_t length, bool use_temp) {
  if (0 == length) {
    return -1;
  }

  char name[PATH_MAX];
  for (;;) {
    const char *prefix = use_temp ? kShmTempPrefix : kShmOpenPrefix;
    snprintf(name, sizeof name, "%s-%u.%u", prefix,
             getpid(),
             static_cast<uint32_t>(AtomicIncrement(&memory_object_count, 1)));
    int m;
    if (use_temp) {
      m = open(name, O_RDWR | O_CREAT | O_EXCL, 0);
    } else {
      m = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0);
    }
    if (0 <= m) {
      (void) shm_unlink(name);
      if (ftruncate(m, length) == -1) {
        close(m);
        m = -1;
      }
      return m;
    }
    if (errno != EEXIST) {
      return -1;
    }
    // Retry only if we got EEXIST.
  }
}

Handle CreateMemoryObject(size_t length) {
  if (0 == length) {
    return -1;
  }
  int fd;

  // On Mac OS X, shm_open() gives us file descriptors that the OS
  // won't mmap() with PROT_EXEC, which is no good for the dynamic
  // code region.  Try open()ing a file in /tmp first, but fall back
  // to using shm_open() if /tmp is not available, which will be the
  // case inside the Chromium sandbox.  This means that dynamic
  // loading will only work with --no-sandbox.
  //
  // TODO(mseaborn): We will probably need to do IPC to acquire SHM FDs
  // inside the Chromium Mac sandbox, as on Linux.

#if NACL_OSX
  // Try /tmp first.  It would be OK to enable this for Linux, but
  // there's no need because shm_open() (which uses /dev/shm rather
  // than /tmp) is fine on Linux.
  fd = TryShmOrTempOpen(length, true);
  if (fd >= 0)
    return fd;
#endif
  
  // Try shm_open().
  fd = TryShmOrTempOpen(length, false);
  if (fd >= 0)
    return fd;

#if NACL_LINUX && !defined(NACL_STANDALONE)
  // As a temporary measure, we try shm_open() as well as calling
  // the unsandboxed browser process.  This code runs in the
  // context of both the renderer and (Chromium's compiled-in)
  // sel_ldr.  Currently sel_ldr is not sandboxed and doesn't have
  // the appropriate socket FD set up for talking to the browser.
  // TODO(mseaborn): Move this to be the first method tried.
  return renderer_sandbox_support::MakeSharedMemorySegmentViaIPC(length);
#endif

  return -1;
}

void* Map(void* start, size_t length, int prot, int flags,
          Handle memory, off_t offset) {
  static const int kPosixProt[] = {
    PROT_NONE,
    PROT_READ,
    PROT_WRITE,
    PROT_READ | PROT_WRITE,
    PROT_EXEC,
    PROT_READ | PROT_EXEC,
    PROT_WRITE | PROT_EXEC,
    PROT_READ | PROT_WRITE | PROT_EXEC
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
  return mmap(start, length, kPosixProt[prot & 7], adjusted, memory, offset);
}

int Unmap(void* start, size_t length) {
  return munmap(start, length);
}

}  // namespace nacl
