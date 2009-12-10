// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The following is the C version of code from base/process_utils_linux.cc.
// We shouldn't link against C++ code in a setuid binary.

#include "process_util.h"

#include <fcntl.h>
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

  char oom_adj[35];  // "/proc/" + log_2(2**64) + "/oom_adj\0"
                     //    6     +       20     +     9         = 35
  snprintf(oom_adj, sizeof(oom_adj), "/proc/%lu", process);

  struct stat statbuf;
  if (stat(oom_adj, &statbuf) < 0)
    return false;
  if (getuid() != statbuf.st_uid)
    return false;

  strcat(oom_adj, "/oom_adj");
  int fd = open(oom_adj, O_WRONLY);
  if (fd < 0)
    return false;

  char buf[3];  // 0 <= |score| <= 15;
  snprintf(buf, sizeof(buf), "%d", score);
  size_t len = strlen(buf);

  ssize_t bytes_written = write(fd, buf, len);
  close(fd);
  return (bytes_written == len);
}
