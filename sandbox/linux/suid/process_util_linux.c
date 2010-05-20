// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The following is the C version of code from base/process_utils_linux.cc.
// We shouldn't link against C++ code in a setuid binary.

#define _GNU_SOURCE  // needed for O_DIRECTORY

#include "process_util.h"

#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

bool AdjustOOMScore(pid_t process, int score) {
  if (score < 0 || score > 15)
    return false;

  char oom_adj[27];  // "/proc/" + log_10(2**64) + "\0"
                     //    6     +       20     +     1         = 27
  snprintf(oom_adj, sizeof(oom_adj), "/proc/%" PRIdMAX, (intmax_t)process);

  const int dirfd = open(oom_adj, O_RDONLY | O_DIRECTORY);
  if (dirfd < 0)
    return false;

  struct stat statbuf;
  if (fstat(dirfd, &statbuf) < 0) {
    close(dirfd);
    return false;
  }
  if (getuid() != statbuf.st_uid) {
    close(dirfd);
    return false;
  }

  const int fd = openat(dirfd, "oom_adj", O_WRONLY);
  close(dirfd);

  if (fd < 0)
    return false;

  char buf[3];  // 0 <= |score| <= 15;
  snprintf(buf, sizeof(buf), "%d", score);
  size_t len = strlen(buf);

  ssize_t bytes_written = write(fd, buf, len);
  close(fd);
  return (bytes_written == len);
}
