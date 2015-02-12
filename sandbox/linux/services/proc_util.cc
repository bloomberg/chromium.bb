// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/proc_util.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"

namespace sandbox {
namespace {

struct DIRCloser {
  void operator()(DIR* d) const {
    DCHECK(d);
    PCHECK(0 == closedir(d));
  }
};

typedef scoped_ptr<DIR, DIRCloser> ScopedDIR;

}  // namespace

int ProcUtil::CountOpenFds(int proc_fd) {
  DCHECK_LE(0, proc_fd);
  int proc_self_fd = HANDLE_EINTR(
      openat(proc_fd, "self/fd", O_DIRECTORY | O_RDONLY | O_CLOEXEC));
  PCHECK(0 <= proc_self_fd);

  // Ownership of proc_self_fd is transferred here, it must not be closed
  // or modified afterwards except via dir.
  ScopedDIR dir(fdopendir(proc_self_fd));
  CHECK(dir);

  int count = 0;
  struct dirent e;
  struct dirent* de;
  while (!readdir_r(dir.get(), &e, &de) && de) {
    if (strcmp(e.d_name, ".") == 0 || strcmp(e.d_name, "..") == 0) {
      continue;
    }

    int fd_num;
    CHECK(base::StringToInt(e.d_name, &fd_num));
    if (fd_num == proc_fd || fd_num == proc_self_fd) {
      continue;
    }

    ++count;
  }
  return count;
}

bool ProcUtil::HasOpenDirectory(int proc_fd) {
  int proc_self_fd = -1;
  if (proc_fd >= 0) {
    proc_self_fd = openat(proc_fd, "self/fd", O_DIRECTORY | O_RDONLY);
  } else {
    proc_self_fd = openat(AT_FDCWD, "/proc/self/fd", O_DIRECTORY | O_RDONLY);
    if (proc_self_fd < 0) {
      // If this process has been chrooted (eg into /proc/self/fdinfo) then
      // the new root dir will not have directory listing permissions for us
      // (hence EACCES).  And if we do have this permission, then /proc won't
      // exist anyway (hence ENOENT).
      DPCHECK(errno == EACCES || errno == ENOENT)
        << "Unexpected failure when trying to open /proc/self/fd: ("
        << errno << ") " << strerror(errno);

      // If not available, guess false.
      return false;
    }
  }
  PCHECK(0 <= proc_self_fd);

  // Ownership of proc_self_fd is transferred here, it must not be closed
  // or modified afterwards except via dir.
  ScopedDIR dir(fdopendir(proc_self_fd));
  CHECK(dir);

  struct dirent e;
  struct dirent* de;
  while (!readdir_r(dir.get(), &e, &de) && de) {
    if (strcmp(e.d_name, ".") == 0 || strcmp(e.d_name, "..") == 0) {
      continue;
    }

    int fd_num;
    CHECK(base::StringToInt(e.d_name, &fd_num));
    if (fd_num == proc_fd || fd_num == proc_self_fd) {
      continue;
    }

    struct stat s;
    // It's OK to use proc_self_fd here, fstatat won't modify it.
    CHECK(fstatat(proc_self_fd, e.d_name, &s, 0) == 0);
    if (S_ISDIR(s.st_mode)) {
      return true;
    }
  }

  // No open unmanaged directories found.
  return false;
}

//static
base::ScopedFD ProcUtil::OpenProcSelfTask() {
  base::ScopedFD proc_self_task(
   HANDLE_EINTR(
      open("/proc/self/task/", O_RDONLY | O_DIRECTORY | O_CLOEXEC)));
  PCHECK(proc_self_task.is_valid());
  return proc_self_task.Pass();
}

}  // namespace sandbox
