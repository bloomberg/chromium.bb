// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The following is duplicated from base/linux_utils.cc.
// We shouldn't link against C++ code in a setuid binary.

#include "linux_util.h"

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// expected prefix of the target of the /proc/self/fd/%d link for a socket
static const char kSocketLinkPrefix[] = "socket:[";

// Parse a symlink in /proc/pid/fd/$x and return the inode number of the
// socket.
//   inode_out: (output) set to the inode number on success
//   path: e.g. /proc/1234/fd/5 (must be a UNIX domain socket descriptor)
static bool ProcPathGetInode(ino_t* inode_out, const char* path) {
  char buf[256];
  const ssize_t n = readlink(path, buf, sizeof(buf) - 1);
  if (n == -1)
    return false;
  buf[n] = 0;

  if (memcmp(kSocketLinkPrefix, buf, sizeof(kSocketLinkPrefix) - 1))
    return false;

  char *endptr;
  const unsigned long long int inode_ul =
      strtoull(buf + sizeof(kSocketLinkPrefix) - 1, &endptr, 10);
  if (*endptr != ']')
    return false;

  if (inode_ul == ULLONG_MAX)
    return false;

  *inode_out = inode_ul;
  return true;
}

bool FindProcessHoldingSocket(pid_t* pid_out, ino_t socket_inode) {
  bool already_found = false;

  DIR* proc = opendir("/proc");
  if (!proc)
    return false;

  const uid_t uid = getuid();
  struct dirent* dent;
  while ((dent = readdir(proc))) {
    char *endptr;
    const unsigned long int pid_ul = strtoul(dent->d_name, &endptr, 10);
    if (pid_ul == ULONG_MAX || *endptr)
      continue;

    // We have this setuid code here because the zygote and its children have
    // /proc/$pid/fd owned by root. While scanning through /proc, we add this
    // extra check so users cannot accidentally gain information about other
    // users' processes. To determine process ownership, we use the property
    // that if user foo owns process N, then /proc/N is owned by foo.
    {
      char buf[256];
      struct stat statbuf;
      snprintf(buf, sizeof(buf), "/proc/%lu", pid_ul);
      if (stat(buf, &statbuf) < 0)
        continue;
      if (uid != statbuf.st_uid)
        continue;
    }

    char buf[256];
    snprintf(buf, sizeof(buf), "/proc/%lu/fd", pid_ul);
    DIR* fd = opendir(buf);
    if (!fd)
      continue;

    while ((dent = readdir(fd))) {
      if (snprintf(buf, sizeof(buf), "/proc/%lu/fd/%s", pid_ul,
                   dent->d_name) >= sizeof(buf) - 1) {
        continue;
      }

      ino_t fd_inode;
      if (ProcPathGetInode(&fd_inode, buf)) {
        if (fd_inode == socket_inode) {
          if (already_found) {
            closedir(fd);
            closedir(proc);
            return false;
          }

          already_found = true;
          *pid_out = pid_ul;
          break;
        }
      }
    }
    closedir(fd);
  }
  closedir(proc);

  return already_found;
}
