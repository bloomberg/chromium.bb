// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/test_util.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_POSIX)
#include <unistd.h>
#endif

namespace remoting {

bool MakePipe(base::PlatformFile* read_handle,
              base::PlatformFile* write_handle) {
#if defined(OS_WIN)
  return CreatePipe(read_handle, write_handle, NULL, 0) != FALSE;
#elif defined(OS_POSIX)
  int fds[2];
  if (pipe(fds) == 0) {
    *read_handle = fds[0];
    *write_handle = fds[1];
    return true;
  }
  return false;
#else
#error Not implemented
#endif
}

}  // namepsace remoting
